#include "Exchange.h"

#include <algorithm>
#include <limits>

#include "ExchangeTiming.h"

namespace SEN059x::example {
namespace {
constexpr auto NO_WAKE = std::numeric_limits<std::uint64_t>::max();
constexpr Status DEADLINE{Err::TIMEOUT, 0, "Exchange deadline expired"};
constexpr Status INVALID{Err::INVALID_CONFIG, 0, "Invalid exchange request or binding"};

std::size_t buildRequest(const ExchangeRequest& request, std::array<std::uint8_t, 8>& tx) {
  const auto& context = request.context;
  RequestContext built;
  if (context.function == FunctionCode::ReadHoldingRegisters) {
    return buildReadRegisters(context.model, context.address, context.startRegister,
                              context.countOrWriteWord, tx.data(), tx.size(), built,
                              context.addressPolicy);
  }
  const auto setting = static_cast<Setting>(context.startRegister);
  SettingDescriptor descriptor;
  if (context.function != FunctionCode::WriteSingleRegister ||
      !settingDescriptor(context.model, setting, descriptor)) {
    return 0;
  }
  const SettingValue value{descriptor.unit, context.countOrWriteWord};
  if (request.scope == WriteScope::VendorSendOnly) {
    return buildVendorBroadcastWrite(context.model, setting, value, tx.data(), tx.size(),
                                     context.addressPolicy);
  }
  return buildWriteSingleRegister(context.model, context.address, setting, value, tx.data(),
                                  tx.size(), built, context.addressPolicy);
}
}  // namespace

Exchange::Exchange(ExchangePort& port, std::uint8_t* rx, std::size_t capacity)
    : port_(port), rx_(rx), capacity_(capacity) {}

Status Exchange::begin(ExchangeRequest request, DeviceHealth& health, MeasurementCache* cache) {
  if ((phase_ != Phase::Idle && !done()) || (result_.txAccepted && !result_.txDrained)) {
    return {Err::BUS_BUSY, 0, "Exchange active or TX drain unresolved"};
  }
  if (request.deadlineUs == NO_WAKE) {
    return INVALID;
  }
  if (port_.nowUs() >= request.deadlineUs) {
    return DEADLINE;
  }
  if (!rx_ || !capacity_ || !rtuTiming(request.baud, interCharacterUs_, closureUs_)) {
    return INVALID;
  }
  const auto& context = request.context;
  if (request.scope != WriteScope::Addressed &&
      (request.scope != WriteScope::VendorSendOnly ||
       context.function != FunctionCode::WriteSingleRegister || context.address != 0xFF)) {
    return INVALID;
  }
  const auto kind = static_cast<MeasurementKind>(context.startRegister);
  if (request.publishMeasurement &&
      (!cache || context.function != FunctionCode::ReadHoldingRegisters ||
       context.countOrWriteWord != 1 ||
       request.bindingGeneration != cache->snapshot(kind, 0, 0).bindingGeneration)) {
    return INVALID;
  }
  const bool addressed = request.scope == WriteScope::Addressed;
  if (addressed && !health.beginAttempt(attempt_)) {
    return {Err::BUS_BUSY, 0, "Health record unbound or busy"};
  }
  if (request.publishMeasurement) {
    if (!cache->buildRead(kind, tx_.data(), tx_.size(), sample_)) {
      health.discard(attempt_);
      return INVALID;
    }
    const auto& built = sample_.context();
    if (context.model != built.model || context.address != built.address ||
        context.addressPolicy != built.addressPolicy) {
      cache->discard(sample_);
      health.discard(attempt_);
      return INVALID;
    }
  } else if (buildRequest(request, tx_) != tx_.size()) {
    if (addressed) {
      health.discard(attempt_);
    }
    return INVALID;
  }
  result_ = {};
  result_.request = request;
  health_ = addressed ? &health : nullptr;
  cache_ = request.publishMeasurement ? cache : nullptr;
  ignoredFrame_ = false;
  phase_ = Phase::Send;
  return Ok();
}

void Exchange::finish(Status status, std::uint64_t now, bool attributable) {
  const auto ms = static_cast<std::uint32_t>(now / 1000);
  if (cache_) {
    cache_->discard(sample_);
  }
  // Only the receive phase has a fully drained request and a bus released for a reply.
  if (attributable && phase_ == Phase::Receive) {
    if (status.ok() || status.code == Err::EXCEPTION) {
      health_->recordReply(attempt_, ms, true);
    } else if (!health_->recordFailure(attempt_, status, ms, true)) {
      health_->discard(attempt_);
    }
  } else if (health_) {
    health_->discard(attempt_);
  }
  result_.status = status;
  result_.completedUs = now;
  phase_ = Phase::Done;
}

void Exchange::cancel() {
  if (phase_ != Phase::Idle && !done()) {
    finish({Err::ABORTED, 0, "Exchange cancelled"}, port_.nowUs(), false);
  }
}

void Exchange::parse(std::uint64_t now) {
  const auto& context = result_.request.context;
  if (rx_[0] != context.address || result_.origin == AckOrigin::LocalEcho) {
    // A closed foreign frame or local echo may precede the actual device reply.
    ignoredFrame_ = true;
    return;
  }
  if (result_.origin != AckOrigin::Device ||
      (context.function == FunctionCode::WriteSingleRegister && !result_.wireFrameClosed)) {
    finish({Err::FRAME_ERROR, 0, "Reply source or FC06 frame closure is unqualified"}, now, false);
    return;
  }
  Status status;
  if (cache_) {
    status = cache_->complete(sample_, rx_, result_.rxAccessible,
                              static_cast<std::uint32_t>(now / 1000), true, result_.readError);
  } else if (context.function == FunctionCode::ReadHoldingRegisters) {
    status = validateReadResponseExpected(rx_, result_.rxAccessible, context, result_.words.data(),
                                          result_.words.size(), result_.readError);
    if (status) {
      result_.wordCount = context.countOrWriteWord;
    }
  } else {
    status = parseWriteSingleRegister(rx_, result_.rxAccessible, context, result_.writeError);
  }
  finish(status, now);
}

std::uint64_t Exchange::receive(std::uint64_t readStart) {
  const bool vendor = phase_ == Phase::SettleVendor;
  const auto offset = ignoredFrame_ ? 0 : result_.rxAccessible;
  const auto room = (vendor ? std::min(capacity_, result_.vendorRx.size()) : capacity_) - offset;
  const auto batch = port_.read(rx_ + offset, room);
  const auto now = port_.nowUs();  // Foreground delivery time, including the bounded read itself.
  if (batch.total || batch.copied) {
    if (ignoredFrame_) {
      result_.rxTotal = 0;
      result_.rxAccessible = 0;
      ignoredFrame_ = false;
    }
    if (!result_.rxTotal) {
      result_.origin = batch.origin;
    } else if (result_.origin != batch.origin) {
      result_.origin = AckOrigin::Unknown;
    }
    result_.lastDeliveryUs = now;
    result_.wireFrameClosed = batch.wireFrameClosed;
    const auto remaining = std::numeric_limits<std::size_t>::max() - result_.rxTotal;
    result_.overflow |=
        batch.total > remaining || batch.total != batch.copied || batch.copied > room;
    result_.rxTotal += std::min(remaining, batch.total);
    result_.rxAccessible += std::min(room, batch.copied);
    if (vendor) {
      std::copy_n(rx_ + offset, std::min(room, batch.copied), result_.vendorRx.begin() + offset);
    }
  } else if (batch.wireFrameClosed && result_.rxTotal && !ignoredFrame_) {
    result_.wireFrameClosed = true;
  }
  result_.overflow |= batch.overflow;
  result_.uartStatus = batch.uartStatus;
  if (vendor) {
    // All RX is incidental to the local send. Retain its prefix and count and wait for
    // a full quiet interval after release of DE or the most recent delivery.
    if (!batch.uartStatus || batch.overflow) {
      finish(batch.uartStatus.ok() ? Status{Err::FRAME_ERROR, 0, "UART overflow during settlement"}
                                   : batch.uartStatus,
             now);
    } else if (now >= result_.request.deadlineUs) {
      finish(DEADLINE, now);
    } else if (batch.total || batch.copied) {
      return now;
    } else if (readStart - result_.lastDeliveryUs >= closureUs_) {
      finish(Ok(), now);
    }
    return nextWake(now);
  }
  // A newly observed frame start also proves a gap in an unfinished candidate.
  if ((offset && batch.wireFrameStarted) ||
      (batch.wireGapUs && *batch.wireGapUs > interCharacterUs_)) {
    result_.wireGapError = true;
  }
  if (!batch.uartStatus || result_.overflow || result_.wireGapError) {
    // Transport errors remain visible on the exchange. Only charge the binding for damage
    // to its own reply or for a port failure while waiting for the first byte.
    const bool addressed = !ignoredFrame_ && result_.rxAccessible &&
                           result_.origin == AckOrigin::Device &&
                           rx_[0] == result_.request.context.address;
    const bool attributable =
        batch.origin != AckOrigin::LocalEcho && (!result_.rxTotal || addressed);
    const Status frameError{Err::FRAME_ERROR, 0, "RX overflow or excessive wire gap"};
    finish(batch.uartStatus.ok() ? frameError : batch.uartStatus, now, attributable);
  } else if (now >= result_.request.deadlineUs) {
    finish(DEADLINE, now);
  } else if (result_.rxTotal && !ignoredFrame_ &&
             (result_.wireFrameClosed ||
              (!batch.total && readStart - result_.lastDeliveryUs >= closureUs_))) {
    // With foreground timing alone, the empty queue check must start at or after t3.5.
    parse(now);
  }
  // One UART event may cover several buffered batches. Drain them one service step at a time.
  if (batch.total && !done()) {
    return now;
  }
  return nextWake(now);
}

std::uint64_t Exchange::nextWake(std::uint64_t now) const {
  if (done() || phase_ == Phase::Idle) {
    return NO_WAKE;
  }
  const auto deadline = result_.request.deadlineUs;
  if (now >= deadline) {
    return now;
  }
  std::uint64_t delay = std::min<std::uint64_t>(interCharacterUs_, deadline - now);
  if (phase_ == Phase::SettleVendor ||
      (phase_ == Phase::Receive && result_.rxTotal && !ignoredFrame_)) {
    const auto elapsed = now - result_.lastDeliveryUs;
    if (elapsed >= closureUs_) {
      return now;
    }
    delay = std::min(delay, closureUs_ - elapsed);
  }
  return now + delay;
}

std::uint64_t Exchange::service() {
  if (phase_ == Phase::Idle || done()) {
    return NO_WAKE;
  }
  auto now = port_.nowUs();
  const auto deadline = result_.request.deadlineUs;
  if (phase_ == Phase::Drain) {
    bool drained = false;
    result_.uartStatus = port_.txDrained(drained);
    now = port_.nowUs();
    if (!result_.uartStatus) {
      finish(result_.uartStatus, now);
    } else if (drained) {
      result_.txDrained = true;
      port_.setTransmit(false);
      now = port_.nowUs();
      if (result_.txAccepted != tx_.size()) {
        finish({Err::IO, 0, "Short UART write"}, now);
      } else if (now >= deadline) {
        finish(DEADLINE, now);
      } else {
        if (result_.request.scope == WriteScope::VendorSendOnly) {
          phase_ = Phase::SettleVendor;
          result_.lastDeliveryUs = now;  // Start quiet interval at DE release.
        } else {
          phase_ = Phase::Receive;
        }
        return now;
      }
    } else if (now >= deadline) {
      finish(DEADLINE, now);  // DE stays asserted until the application recovers the port.
    }
  } else if (now >= deadline) {
    finish(DEADLINE, now);
  } else if (phase_ == Phase::Send) {
    port_.setTransmit(true);
    now = port_.nowUs();
    if (now >= deadline) {
      port_.setTransmit(false);
      finish(DEADLINE, port_.nowUs());
      return NO_WAKE;
    }
    result_.txAccepted = port_.write(tx_.data(), tx_.size());
    if (!result_.txAccepted) {
      port_.setTransmit(false);
      finish({Err::IO, 0, "UART accepted zero bytes"}, port_.nowUs());
    } else {
      phase_ = Phase::Drain;
      return port_.nowUs();
    }
  } else {
    return receive(now);
  }
  return nextWake(port_.nowUs());
}

}  // namespace SEN059x::example
