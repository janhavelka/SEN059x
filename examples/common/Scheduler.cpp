#include "Scheduler.h"

#include <algorithm>
#include <limits>

#include "ExchangeTiming.h"

namespace SEN059x::example {
namespace {
constexpr auto NO_WAKE = std::numeric_limits<std::uint64_t>::max();
constexpr Status INVALID{Err::INVALID_CONFIG, 0, "Invalid scheduler binding or request"};
constexpr Status QUEUE_TIMEOUT{Err::TIMEOUT, 0, "Deadline expired before transmission"};
constexpr Status CANCELLED{Err::ABORTED, 0, "Scheduled operation cancelled"};
}  // namespace

Status Scheduler::bind(std::size_t slot, DeviceModel model, std::uint8_t address,
                       std::uint32_t baud) {
  Baud code{};
  if (slot >= BINDINGS || !isValidAddress(address) || !valueToBaud(baud, code)) {
    return INVALID;
  }
  for (std::size_t i = 0; i < BINDINGS; ++i) {
    if (i != slot && bindings_[i].address == address) {
      return {Err::BUS_BUSY, 0, "Address already enabled"};
    }
  }
  auto& binding = bindings_[slot];
  // bind() validates the model and generation before invalidating old sample tokens.
  const auto status = binding.cache.bind(model, address);
  if (!status) {
    return status;
  }
  disable(slot);
  binding.health.bind(model, address);  // Same validated endpoint, default nonzero threshold.
  binding.configuration.bind(model, {address, baud});
  binding.model = model;
  binding.address = address;
  binding.baud = baud;
  return Ok();
}

void Scheduler::disable(std::size_t slot) {
  if (slot >= BINDINGS) {
    return;
  }
  for (auto& op : operations_) {
    if (op.binding == slot && (op.state == State::Active || op.state == State::Queued)) {
      cancel(op.result.ticket);
    }
  }
  bindings_[slot].address = 0;
  bindings_[slot].health.unbind();
}

void Scheduler::reset() {
  for (std::size_t slot = 0; slot < BINDINGS; ++slot) {
    disable(slot);
  }
}

Status Scheduler::submit(std::size_t slot, MeasurementKind kind, std::uint64_t operationId,
                         std::uint32_t step, std::uint64_t deadlineUs, bool retryOnce,
                         std::uint64_t& ticket) {
  return submitRead(slot, static_cast<std::uint16_t>(kind), true, operationId, step, deadlineUs,
                    retryOnce, ticket);
}

Status Scheduler::submitSettingRead(std::size_t slot, Setting setting, std::uint64_t operationId,
                                    std::uint32_t step, std::uint64_t deadlineUs,
                                    std::uint64_t& ticket) {
  return submitRead(slot, static_cast<std::uint16_t>(setting), false, operationId, step, deadlineUs,
                    false, ticket);
}

Status Scheduler::submitRead(std::size_t slot, std::uint16_t reg, bool measurement,
                             std::uint64_t operationId, std::uint32_t step,
                             std::uint64_t deadlineUs, bool retryOnce, std::uint64_t& ticket) {
  if (slot >= BINDINGS || deadlineUs == NO_WAKE ||
      lastTicket_ == std::numeric_limits<std::uint64_t>::max()) {
    return INVALID;
  }
  const auto& binding = bindings_[slot];
  if (!binding.address) {
    return {Err::NOT_INITIALIZED, 0, "Binding disabled"};
  }
  MeasurementTiming timing;
  SettingDescriptor descriptor;
  if (measurement ? !measurementTiming(binding.model, static_cast<MeasurementKind>(reg), timing)
                  : !settingDescriptor(binding.model, static_cast<Setting>(reg), descriptor)) {
    return INVALID;
  }
  if (deadlineUs <= port_.nowUs()) {
    return QUEUE_TIMEOUT;
  }
  for (auto& op : operations_) {
    if (op.state != State::Free) {
      continue;
    }
    op = {};
    op.state = State::Queued;
    op.binding = slot;
    op.retryOnce = retryOnce;
    op.result.step = step;
    op.result.ticket = ticket = ++lastTicket_;
    auto& request = op.result.exchange.request;
    request.operationId = operationId;
    request.bindingGeneration =
        binding.cache.snapshot(MeasurementKind::ProcessedDistance, 0, 0).bindingGeneration;
    request.context = {binding.model, binding.address, FunctionCode::ReadHoldingRegisters, reg, 1};
    request.baud = binding.baud;
    request.deadlineUs = deadlineUs;
    request.publishMeasurement = measurement;
    return Ok();
  }
  return {Err::BUS_BUSY, 0, "All operation slots occupied"};
}

Scheduler::Operation* Scheduler::nextOperation() {
  // Rotate bindings after each dispatch, with the oldest ticket first within one binding.
  for (std::size_t offset = 0; offset < BINDINGS; ++offset) {
    const auto slot = (nextBinding_ + offset) % BINDINGS;
    Operation* first = nullptr;
    for (auto& op : operations_) {
      if (op.state == State::Queued && op.binding == slot &&
          (!first || op.result.ticket < first->result.ticket)) {
        first = &op;
      }
    }
    if (first) {
      nextBinding_ = (slot + 1) % BINDINGS;
      return first;
    }
  }
  return nullptr;
}

void Scheduler::complete(Operation& op, Status status, std::uint64_t now) {
  op.result.status = status;
  op.result.completedUs = now;
  op.state = State::Done;
  stats_.recordOperation(status, static_cast<std::uint32_t>(now / 1000));
}

void Scheduler::finishAttempt() {
  auto& op = *active_;
  active_ = nullptr;
  const auto& wire = exchange_->result();
  const bool sent = wire.txAccepted != 0;
  // A retry that accepted zero bytes has a logical outcome but no new wire evidence.
  if (sent || op.result.attempts == 0) {
    op.result.exchange = wire;
  }
  auto status = wire.status;
  const auto ms = static_cast<std::uint32_t>(wire.completedUs / 1000);
  auto& binding = bindings_[op.binding];
  if (!wire.request.publishMeasurement) {
    if (status || wire.readError == ReadResponseError::EXCEPTION_RESPONSE) {
      const Endpoint endpoint{wire.request.context.address, wire.request.baud};
      status = binding.configuration.completeRead(settingRead_, endpoint, rx_.data(),
                                                  wire.rxAccessible, ms);
      if (status) {
        const auto setting = static_cast<Setting>(wire.request.context.startRegister);
        op.result.settingObservation = binding.configuration.snapshot(setting).observed;
      }
    } else {
      binding.configuration.failRead(settingRead_, status);
    }
  } else if (status) {
    const auto kind = static_cast<MeasurementKind>(wire.request.context.startRegister);
    op.result.measurement = binding.cache.snapshot(kind, ms, 0).raw.measurement;
  }
  if (sent) {
    stats_.recordAttemptError(wire.status);
  }
  needsDrain_ = sent && !wire.txDrained;
  if (op.retryOnce && op.result.attempts == 1 && sent && !wire.status &&
      wire.status.code != Err::ABORTED && wire.completedUs < wire.request.deadlineUs) {
    op.state = State::Queued;
  } else {
    op.result.queueExpired = !sent && wire.status.code == Err::TIMEOUT;
    complete(op, status, wire.completedUs);
  }
  // A validated normal or exception reply consumes the only expected device response.
  // After any other outcome with bytes accepted, preserve the old exchange and UART format.
  if (!sent || wire.status.ok() || wire.status.code == Err::EXCEPTION) {
    exchange_.reset();
  }
}

bool Scheduler::cancel(std::uint64_t ticket) {
  for (auto& op : operations_) {
    if (op.result.ticket == ticket) {
      if (op.state == State::Active) {
        exchange_->cancel();
        finishAttempt();
        return true;
      }
      if (op.state == State::Queued) {
        complete(op, CANCELLED, port_.nowUs());
        return true;
      }
    }
  }
  return false;
}

bool Scheduler::take(std::uint64_t ticket, ScheduledResult& out) {
  for (auto& op : operations_) {
    if (op.state == State::Done && op.result.ticket == ticket) {
      out = op.result;
      op.state = State::Free;
      return true;
    }
  }
  return false;
}

std::uint64_t Scheduler::queueWake(std::uint64_t wake) const {
  for (const auto& op : operations_) {
    if (op.state == State::Queued) {
      if (settled()) {
        return port_.nowUs();
      }
      wake = std::min(wake, op.result.exchange.request.deadlineUs);
    }
  }
  return wake;
}

std::uint64_t Scheduler::service() {
  const auto now = port_.nowUs();
  for (auto& op : operations_) {
    if (op.state == State::Queued && now >= op.result.exchange.request.deadlineUs) {
      op.result.queueExpired = true;
      complete(op, QUEUE_TIMEOUT, now);
    }
  }
  if (settled()) {
    auto* op = nextOperation();
    if (!op) {
      return NO_WAKE;
    }
    const auto& request = op->result.exchange.request;
    auto& binding = bindings_[op->binding];
    auto status = port_.configure8N1(request.baud);
    if (status) {
      exchange_.emplace(port_, rx_.data(), rx_.size());
      status = exchange_->begin(request, binding.health, &binding.cache);
      if (status && !request.publishMeasurement) {
        // Exchange encodes the admitted context for the port. Issue the matching configuration
        // token only at dispatch so queued reads cannot supersede the live request.
        std::array<std::uint8_t, 8> tx;
        const auto setting = static_cast<Setting>(request.context.startRegister);
        if (!binding.configuration.buildRead({request.context.address, request.baud}, setting,
                                             tx.data(), tx.size(), settingRead_)) {
          exchange_->cancel();
          status = INVALID;
        }
      }
    }
    if (!status) {
      exchange_.reset();
      op->result.queueExpired = status.code == Err::TIMEOUT && port_.nowUs() >= request.deadlineUs;
      complete(*op, status, port_.nowUs());
      return queueWake(NO_WAKE);
    }
    active_ = op;
    op->state = State::Active;
  }
  if (!active_) {
    return queueWake(NO_WAKE);  // Retired wire ownership survives taking/cancelling a result.
  }
  const bool alreadySent = exchange_->result().txAccepted != 0;
  auto wake = exchange_->service();
  if (!alreadySent && exchange_->result().txAccepted) {
    if (active_->result.attempts > 0) {
      stats_.recordRetry();
    }
    ++active_->result.attempts;
    if (physicalAttempts_ != std::numeric_limits<std::uint64_t>::max()) {
      ++physicalAttempts_;
    }
  }
  if (exchange_->done()) {
    finishAttempt();
    wake = NO_WAKE;
  }
  return queueWake(wake);
}

bool Scheduler::recover() {
  if (!exchange_ || active_) {
    return settled();
  }
  if (needsDrain_) {
    bool drained = false;
    if (port_.txDrained(drained) && drained) {
      port_.setTransmit(false);
      needsDrain_ = false;
    }
    return false;
  }
  const auto& retired = exchange_->result();
  if (retired.txAccepted != 8) {
    return false;  // Partial request: no valid device reply can establish settlement.
  }
  const auto batch = port_.read(rx_.data(), rx_.size());
  if (!batch.uartStatus || batch.origin != AckOrigin::Device || !batch.wireFrameStarted ||
      !batch.wireFrameClosed || batch.overflow || batch.copied != batch.total ||
      batch.copied > rx_.size()) {
    return false;
  }
  std::uint32_t interCharacterUs = 0, closureUs = 0;
  rtuTiming(retired.request.baud, interCharacterUs, closureUs);
  if (batch.wireGapUs && *batch.wireGapUs > interCharacterUs) {
    return false;
  }
  std::uint16_t discarded = 0;
  ReadResponseError error{};
  const auto status = validateReadResponseExpected(rx_.data(), batch.copied,
                                                   retired.request.context, &discarded, 1, error);
  if (status || status.code == Err::EXCEPTION) {
    exchange_.reset();
  }
  return settled();
}

bool Scheduler::recoverIsolated() {
  if (exchange_ && !active_ && port_.confirmIsolation(exchange_->result().request)) {
    exchange_.reset();
    needsDrain_ = false;
  }
  return settled();
}

HealthSnapshot Scheduler::health(std::size_t slot) const {
  return slot < BINDINGS ? bindings_[slot].health.healthSnapshot() : HealthSnapshot{};
}

bool Scheduler::endpointTrusted(std::size_t slot) const {
  return slot < BINDINGS && bindings_[slot].address &&
         bindings_[slot].configuration.endpointTrusted();
}

ConfigurationSnapshot Scheduler::settingSnapshot(std::size_t slot, Setting setting) const {
  if (slot >= BINDINGS || !bindings_[slot].address) {
    return {};
  }
  return bindings_[slot].configuration.snapshot(setting);
}

SampleSnapshot Scheduler::snapshot(std::size_t slot, MeasurementKind kind, std::uint32_t nowMs,
                                   std::uint32_t maxAgeMs) const {
  if (slot >= BINDINGS || !bindings_[slot].address) {
    return {};
  }
  return bindings_[slot].cache.snapshot(kind, nowMs, maxAgeMs);
}

}  // namespace SEN059x::example
