#include "Exchange.h"

#include <algorithm>
#include <limits>

#include "ExchangeTiming.h"

namespace SEN059x::example {
namespace {
constexpr auto NO_WAKE = std::numeric_limits<std::uint64_t>::max();
constexpr Status DEADLINE{Err::TIMEOUT, 0, "Exchange deadline expired"};
constexpr Status INVALID{Err::INVALID_CONFIG, 0, "Invalid exchange request or binding"};

std::size_t buildRequest(const RequestContext& context, std::array<std::uint8_t, 8>& tx) {
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
  return buildWriteSingleRegister(context.model, context.address, setting,
                                  {descriptor.unit, context.countOrWriteWord}, tx.data(), tx.size(),
                                  built, context.addressPolicy);
}
}  // namespace

Exchange::Exchange(ExchangePort& port, std::uint8_t* rx, std::size_t capacity)
    : port_(port), rx_(rx), capacity_(capacity) {}

Status Exchange::begin(ExchangeRequest request, DeviceHealth& health, MeasurementCache* cache) {
  if ((phase_ != Phase::Idle && !done()) || (result_.txAccepted && !result_.txDrained)) {
    return {Err::BUS_BUSY, 0, "Exchange active or TX drain unresolved"};
  }
  if (port_.nowUs() >= request.deadlineUs) {
    return DEADLINE;
  }
  if (!rx_ || !capacity_ || !rtuTiming(request.baud, interCharacterUs_, closureUs_)) {
    return INVALID;
  }
  const auto& context = request.context;
  const auto kind = static_cast<MeasurementKind>(context.startRegister);
  if (request.publishMeasurement &&
      (!cache || context.function != FunctionCode::ReadHoldingRegisters ||
       context.countOrWriteWord != 1 ||
       request.bindingGeneration != cache->snapshot(kind, 0, 0).bindingGeneration)) {
    return INVALID;
  }
  if (!health.beginAttempt(attempt_)) {
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
  } else if (buildRequest(context, tx_) != tx_.size()) {
    health.discard(attempt_);
    return INVALID;
  }
  result_ = {};
  result_.request = request;
  health_ = &health;
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
  if (attributable && result_.txAccepted == tx_.size()) {
    if (status.ok() || status.code == Err::EXCEPTION) {
      health_->recordReply(attempt_, ms, true);
    } else if (!health_->recordFailure(attempt_, status, ms, true)) {
      health_->discard(attempt_);
    }
  } else {
    health_->discard(attempt_);
  }
  result_.status = status;
  result_.completedUs = now;
  phase_ = Phase::Done;
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

void Exchange::receive() {
  const auto offset = ignoredFrame_ ? 0 : result_.rxAccessible;
  const auto room = capacity_ - offset;
  const auto batch = port_.read(rx_ + offset, room);
  const auto now = port_.nowUs();  // Foreground delivery time, including the bounded read itself.
  if (batch.total || batch.copied) {
    if (ignoredFrame_) {
      result_.rxTotal = 0;
      result_.rxAccessible = 0;
      ignoredFrame_ = false;
    } else if (result_.wireFrameClosed) {
      // This batch follows a proven frame boundary. Combining the two could create a valid CRC.
      result_.wireGapError = true;
    }
    if (!result_.rxTotal) {
      result_.origin = batch.origin;
    } else if (result_.origin != batch.origin) {
      result_.origin = AckOrigin::Unknown;
    }
    result_.lastDeliveryUs = now;
    result_.wireFrameClosed = batch.wireFrameClosed;
    const auto remaining = std::numeric_limits<std::size_t>::max() - result_.rxTotal;
    result_.overflow =
        batch.total > remaining || batch.total != batch.copied || batch.copied > room;
    result_.rxTotal += std::min(remaining, batch.total);
    result_.rxAccessible += std::min(room, batch.copied);
  } else if (batch.wireFrameClosed && result_.rxTotal && !ignoredFrame_) {
    result_.wireFrameClosed = true;
  }
  result_.overflow |= batch.overflow;
  if (batch.wireGapUs && *batch.wireGapUs > interCharacterUs_) {
    result_.wireGapError = true;
  }
  result_.uartStatus = batch.uartStatus;
  if (!batch.uartStatus || result_.overflow || result_.wireGapError) {
    // Transport errors remain visible on the exchange. Only charge the binding for damage
    // to its own reply or for a port failure while waiting for the first byte.
    const bool attributable = result_.rxTotal == 0
                                  ? batch.origin != AckOrigin::LocalEcho
                                  : result_.rxAccessible && result_.origin == AckOrigin::Device &&
                                        rx_[0] == result_.request.context.address && !ignoredFrame_;
    const Status frameError{Err::FRAME_ERROR, 0, "RX overflow or excessive wire gap"};
    finish(batch.uartStatus.ok() ? frameError : batch.uartStatus, now, attributable);
  } else if (now >= result_.request.deadlineUs) {
    finish(DEADLINE, now);
  } else if (!batch.total && result_.rxTotal && !ignoredFrame_ &&
             (result_.wireFrameClosed || now - result_.lastDeliveryUs >= closureUs_)) {
    parse(now);
  }
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
  if (phase_ == Phase::Receive && result_.rxTotal && !ignoredFrame_) {
    const auto elapsed = now - result_.lastDeliveryUs;
    if (result_.wireFrameClosed || elapsed >= closureUs_) {
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
      if (result_.txAccepted != tx_.size()) {
        finish({Err::IO, 0, "Short UART write"}, now);
      } else if (now >= deadline) {
        finish(DEADLINE, now);
      } else {
        phase_ = Phase::Receive;
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
      finish(DEADLINE, now);
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
    receive();
  }
  return nextWake(port_.nowUs());
}

}  // namespace SEN059x::example
