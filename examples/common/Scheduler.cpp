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
constexpr std::uint64_t MAINTENANCE_IDLE_US = 60000000;
constexpr Status UNSUPPORTED_BAUD{Err::UNSUPPORTED, 0, "Baud rate unavailable on this host"};

bool sameEndpoint(Endpoint a, Endpoint b) {
  return a.address == b.address && a.bps == b.bps;
}
}  // namespace

bool Scheduler::addressInUse(std::size_t slot, std::uint8_t address) const {
  for (std::size_t i = 0; i < BINDINGS; ++i) {
    const auto current = endpoint(i);
    const auto candidate = possibleEndpoint(i);
    if (i != slot && current &&
        (current->address == address || (candidate && candidate->address == address))) {
      return true;
    }
  }
  // Reserve an admitted destination even before the first FC06 byte reaches the port.
  return std::any_of(operations_.begin(), operations_.end(), [&](const Operation& op) {
    return op.binding != slot && (op.state == State::Queued || op.state == State::Active) &&
           op.result.write && op.result.write->intended.address == address;
  });
}

Status Scheduler::bind(std::size_t slot, DeviceModel model, std::uint8_t address,
                       std::uint32_t baud, AddressPolicy policy) {
  Baud code{};
  if (slot >= BINDINGS || !isValidAddress(address, policy) || !valueToBaud(baud, code)) {
    return INVALID;
  }
  if (!port_.supportsBaud(baud)) {
    return UNSUPPORTED_BAUD;
  }
  if (addressInUse(slot, address)) {
    return {Err::BUS_BUSY, 0, "Address occupied or reserved by another binding"};
  }
  auto& binding = bindings_[slot];
  // bind() validates the model and generation before invalidating old sample tokens.
  const auto status = binding.cache.bind(model, address, policy);
  if (!status) {
    return status;
  }
  disable(slot);
  binding.health.bind(model, address, 5, policy);
  binding.configuration.bind(model, {address, baud}, policy);
  binding.policy = policy;
  binding.model = model;
  binding.address = address;
  binding.baud = baud;
  return Ok();
}

void Scheduler::disable(std::size_t slot) {
  if (slot >= BINDINGS) {
    return;
  }
  if (grant_ && grant_->slot == slot) {
    maintenanceOff();
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
  return enqueue(slot, static_cast<std::uint16_t>(kind), true, operationId, step, deadlineUs,
                 retryOnce, ticket);
}

Status Scheduler::submitSettingRead(std::size_t slot, Setting setting, std::uint64_t operationId,
                                    std::uint32_t step, std::uint64_t deadlineUs,
                                    std::uint64_t& ticket, std::optional<Endpoint> at) {
  return enqueue(slot, static_cast<std::uint16_t>(setting), false, operationId, step, deadlineUs,
                 false, ticket, nullptr, WriteScope::Addressed, at);
}

Status Scheduler::previewWrite(std::size_t slot, Setting setting, SettingValue value,
                               ConfigurationOperation& out, WriteScope scope) const {
  if (slot >= BINDINGS) {
    return INVALID;
  }
  const auto& binding = bindings_[slot];
  if (!binding.address) {
    return {Err::NOT_INITIALIZED, 0, "Binding disabled"};
  }
  // buildWrite issues a token and retires any pending read. Build the plan on a separate
  // configuration so preview can run even while the live binding awaits a reply.
  Configuration preview;
  auto status = preview.bind(binding.model, binding.configuration.endpoint(), binding.policy);
  if (!status) {
    return status;
  }
  std::array<std::uint8_t, 8> tx;
  ConfigurationToken token;
  if (!preview.buildWrite(setting, value, scope, tx.data(), tx.size(), token)) {
    return INVALID;
  }
  out = preview.operation();
  out.before = binding.configuration.snapshot(setting);
  return Ok();
}

Status Scheduler::grantIsolatedMaintenance(std::size_t slot, std::uint64_t session) {
  maintenanceOff();
  if (slot >= BINDINGS || !bindings_[slot].address) {
    return {Err::DISALLOWED, 0, "Select an enabled binding for isolated maintenance"};
  }
  grant_ = Grant{slot, session, port_.nowUs()};
  return Ok();
}

void Scheduler::sessionActivity(std::uint64_t session) {
  if (grant_) {
    const auto now = port_.nowUs();
    if (grant_->session != session || now - grant_->lastActivityUs >= MAINTENANCE_IDLE_US) {
      maintenanceOff();
    } else {
      grant_->lastActivityUs = now;
    }
  }
}

Status Scheduler::submitWrite(std::size_t slot, Setting setting, SettingValue value,
                              std::uint64_t session, std::uint64_t operationId, std::uint32_t step,
                              std::uint64_t deadlineUs, std::uint64_t& ticket, WriteScope scope) {
  sessionActivity(session);
  if (!grant_ || grant_->slot != slot) {
    return {Err::DISALLOWED, 0, "Isolated maintenance grant required for this binding"};
  }
  if (!endpointTrusted(slot)) {
    return {Err::DISALLOWED, 0, "Confirm the endpoint with an addressed read"};
  }
  if (!settled() || std::any_of(operations_.begin(), operations_.end(),
                                [](const Operation& op) { return op.state == State::Queued; })) {
    return {Err::DISALLOWED, 0, "Isolated write requires an idle and settled bus"};
  }
  ConfigurationOperation plan;
  auto status = previewWrite(slot, setting, value, plan, scope);
  if (!status) {
    return status;
  }
  if (!port_.supportsBaud(plan.intended.bps) || !port_.supportsBaud(plan.original.bps)) {
    return UNSUPPORTED_BAUD;
  }
  if (addressInUse(slot, plan.intended.address)) {
    return {Err::BUS_BUSY, 0, "Intended address occupied by another binding"};
  }
  status = enqueue(slot, static_cast<std::uint16_t>(setting), false, operationId, step, deadlineUs,
                   false, ticket, &value, scope);
  if (status) {
    maintenanceOff();
    nextBinding_ = slot;  // Dispatch the admitted write before subsequently queued reads.
  }
  return status;
}

Status Scheduler::enqueue(std::size_t slot, std::uint16_t reg, bool measurement,
                          std::uint64_t operationId, std::uint32_t step, std::uint64_t deadlineUs,
                          bool retryOnce, std::uint64_t& ticket, const SettingValue* write,
                          WriteScope scope, std::optional<Endpoint> at) {
  if (slot >= BINDINGS || deadlineUs == NO_WAKE ||
      lastTicket_ == std::numeric_limits<std::uint64_t>::max()) {
    return INVALID;
  }
  auto& binding = bindings_[slot];
  if (!binding.address) {
    return {Err::NOT_INITIALIZED, 0, "Binding disabled"};
  }
  const auto current = binding.configuration.endpoint();
  const auto candidate = binding.configuration.possibleEndpoint();
  if (at && !sameEndpoint(*at, current) && !(candidate && sameEndpoint(*at, *candidate))) {
    return INVALID;
  }
  if (measurement && candidate) {
    return {Err::DISALLOWED, 0, "Observe the endpoint with a setting read first"};
  }
  const auto target = at.value_or(current);
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
    if (write) {
      std::array<std::uint8_t, 8> tx;
      if (!binding.configuration.buildWrite(static_cast<Setting>(reg), *write, scope, tx.data(),
                                            tx.size(), writeToken_)) {
        return INVALID;
      }
      op.result.write = binding.configuration.operation();
    }
    op.state = State::Queued;
    op.binding = slot;
    op.retryOnce = retryOnce;
    op.result.step = step;
    op.result.ticket = ticket = ++lastTicket_;
    auto& request = op.result.exchange.request;
    request.operationId = operationId;
    request.bindingGeneration = binding.generation();
    request.context = {binding.model, target.address, FunctionCode::ReadHoldingRegisters, reg, 1,
                       binding.policy};
    request.scope = scope;
    if (write) {
      request.context.function = FunctionCode::WriteSingleRegister;
      request.context.countOrWriteWord = write->word;
      if (scope == WriteScope::VendorSendOnly) {
        request.context.address = 0xFF;
      }
    }
    request.baud = target.bps;
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

void Scheduler::complete(Operation& op, Status status, std::uint64_t now, Status cleanup) {
  if (op.result.write) {
    auto& config = bindings_[op.binding].configuration;
    if (!config.operation().primary) {
      if (config.operation().tx == TxProgress::Zero) {
        config.recordTx(writeToken_, TxProgress::Zero, status);
      } else {
        config.finishWrite(writeToken_, status);
      }
    }
    config.recordCleanup(writeToken_, cleanup);
    op.result.write = config.operation();
  }
  op.result.status = status;
  op.result.completedUs = now;
  op.state = State::Done;
  stats_.recordOperation(status, static_cast<std::uint32_t>(now / 1000));
}

Status Scheduler::selectEndpoint(Binding& binding, Endpoint at) {
  auto status = port_.configure8N1(at.bps);
  if (!status || (binding.address == at.address && binding.baud == at.bps)) {
    return status;
  }
  // Host selection starts a new measurement and communication generation. Configuration
  // retains the old observations and candidate until an attributable FC03 resolves them.
  status = binding.cache.bind(binding.model, at.address, binding.policy);
  if (status) {
    binding.health.bind(binding.model, at.address, 5, binding.policy);
    binding.address = at.address;
    binding.baud = at.bps;
    if (grant_ && &bindings_[grant_->slot] == &binding) {
      maintenanceOff();
    }
  }
  return status;
}

Status Scheduler::beginVerification(Operation& op, ExchangeRequest request) {
  auto& binding = bindings_[op.binding];
  std::array<std::uint8_t, 8> tx;
  if (!binding.configuration.buildVerification(writeToken_, tx.data(), tx.size(), settingRead_)) {
    return INVALID;
  }
  const auto intended = binding.configuration.operation().intended;
  auto status = Ok();
  if (intended.address != binding.address || intended.bps != binding.baud) {
    status = selectEndpoint(binding, intended);
  }
  request.context.address = intended.address;
  request.baud = intended.bps;
  request.bindingGeneration = binding.generation();
  request.context.function = FunctionCode::ReadHoldingRegisters;
  request.context.countOrWriteWord = 1;
  if (status) {
    status = exchange_->begin(request, binding.health);
  }
  if (!status) {
    return binding.configuration.failRead(settingRead_, status);
  }
  return status;
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
  if (sent) {
    stats_.recordAttemptError(wire.status);
  }
  if (wire.request.context.function == FunctionCode::WriteSingleRegister) {
    auto& config = binding.configuration;
    if (sent && !wire.txDrained) {
      config.recordTx(writeToken_, config.operation().tx, status);
    }
    if (wire.request.scope == WriteScope::Addressed) {
      if (status || wire.writeError != WriteResponseError::NONE) {
        status = config.recordAck(writeToken_, rx_.data(), wire.rxAccessible, wire.origin);
      } else if (sent) {
        config.missAck(writeToken_, status);
      }
      if (status) {
        status = beginVerification(op, wire.request);
        if (status) {
          active_ = &op;
          return;
        }
      }
    }
  } else if (!wire.request.publishMeasurement) {
    if (status || wire.readError != ReadResponseError::NONE) {
      const Endpoint endpoint{wire.request.context.address, wire.request.baud};
      const auto previous = binding.configuration.endpoint();
      status = binding.configuration.completeRead(settingRead_, endpoint, rx_.data(),
                                                  wire.rxAccessible, ms);
      if (grant_ && grant_->slot == op.binding &&
          !sameEndpoint(previous, binding.configuration.endpoint())) {
        maintenanceOff();
      }
      if (wire.status) {
        const auto setting = static_cast<Setting>(wire.request.context.startRegister);
        op.result.settingObservation = binding.configuration.snapshot(setting).observed;
      }
    } else {
      status = binding.configuration.failRead(settingRead_, status);
    }
  } else if (status) {
    const auto kind = static_cast<MeasurementKind>(wire.request.context.startRegister);
    op.result.measurement = binding.cache.snapshot(kind, ms, 0).raw.measurement;
  }
  needsDrain_ = sent && !wire.txDrained;
  const bool retired = sent && !wire.status && wire.status.code != Err::EXCEPTION;
  if (op.retryOnce && op.result.attempts == 1 && sent && !wire.status &&
      wire.status.code != Err::ABORTED && wire.completedUs < wire.request.deadlineUs) {
    op.state = State::Queued;
  } else {
    op.result.queueExpired =
        !sent && wire.status.code == Err::TIMEOUT && !(op.result.write && op.result.attempts != 0);
    Status cleanup = Ok();
    if (retired) {
      cleanup = !wire.uartStatus || wire.request.scope == WriteScope::VendorSendOnly
                    ? wire.status
                    : Status{Err::BUS_BUSY, 0, "Wire settlement required"};
    }
    complete(op, wire.status.code == Err::ABORTED ? wire.status : status, port_.nowUs(), cleanup);
  }
  // A validated normal or exception reply consumes the only expected device response.
  // After any other outcome with bytes accepted, preserve the old exchange and UART format.
  if (!retired) {
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

Status Scheduler::startAttempt(Operation& op) {
  auto& request = op.result.exchange.request;
  auto& binding = bindings_[op.binding];
  // A queued ticket keeps its original identity even if another operation moved the device.
  if (request.bindingGeneration != binding.generation()) {
    return CANCELLED;
  }
  const Endpoint target =
      op.result.write ? op.result.write->original : Endpoint{request.context.address, request.baud};
  if (!port_.supportsBaud(target.bps) ||
      (op.result.write && !port_.supportsBaud(op.result.write->intended.bps))) {
    return UNSUPPORTED_BAUD;
  }
  if (request.publishMeasurement && binding.configuration.possibleEndpoint()) {
    return {Err::DISALLOWED, 0, "Measurement endpoint is uncertain"};
  }
  const bool settingRead = !request.publishMeasurement && !op.result.write;
  if (settingRead) {
    // A queued read might now target a retired candidate. Validate it before selecting a UART
    // format, and issue its configuration token only when it reaches the front of the queue.
    std::array<std::uint8_t, 8> tx;
    const auto setting = static_cast<Setting>(request.context.startRegister);
    if (!binding.configuration.buildRead(target, setting, tx.data(), tx.size(), settingRead_)) {
      return INVALID;
    }
  }
  auto status = selectEndpoint(binding, target);
  if (status) {
    request.bindingGeneration = binding.generation();
    exchange_.emplace(port_, rx_.data(), rx_.size());
    status = exchange_->begin(request, binding.health, &binding.cache);
  }
  if (!status && settingRead) {
    binding.configuration.failRead(settingRead_, status);
  }
  return status;
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
    const auto status = startAttempt(*op);
    if (!status) {
      exchange_.reset();
      op->result.queueExpired =
          status.code == Err::TIMEOUT && port_.nowUs() >= op->result.exchange.request.deadlineUs;
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
  const bool alreadyDrained = exchange_->result().txDrained;
  auto wake = exchange_->service();
  const auto& wire = exchange_->result();
  if (!alreadySent && wire.txAccepted) {
    if (active_->retryOnce && active_->result.attempts > 0) {
      stats_.recordRetry();
    }
    ++active_->result.attempts;
    if (physicalAttempts_ != std::numeric_limits<std::uint64_t>::max()) {
      ++physicalAttempts_;
    }
  }
  if (wire.request.context.function == FunctionCode::WriteSingleRegister && wire.txAccepted) {
    auto& config = bindings_[active_->binding].configuration;
    if (!alreadySent) {
      config.recordTx(writeToken_,
                      wire.txAccepted == 8 ? TxProgress::Possible : TxProgress::Partial, Ok());
    }
    if (!alreadyDrained && wire.txDrained) {
      if (wire.txAccepted == 8) {
        config.recordTx(writeToken_, TxProgress::Complete, Ok());
      } else {
        config.recordTx(writeToken_, TxProgress::Partial, wire.status);
      }
    }
  }
  if (exchange_->done()) {
    finishAttempt();
    wake = active_ ? port_.nowUs() : NO_WAKE;
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
  if (retired.txAccepted != 8 || retired.request.scope == WriteScope::VendorSendOnly) {
    return false;  // No attributable reply can settle a partial request or a vendor send.
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
  Status status;
  if (retired.request.context.function == FunctionCode::WriteSingleRegister) {
    WriteResponseError error{};
    status = parseWriteSingleRegister(rx_.data(), batch.copied, retired.request.context, error);
  } else {
    std::uint16_t discarded = 0;
    ReadResponseError error{};
    status = validateReadResponseExpected(rx_.data(), batch.copied, retired.request.context,
                                          &discarded, 1, error);
  }
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

std::optional<Endpoint> Scheduler::endpoint(std::size_t slot) const {
  if (slot >= BINDINGS || !bindings_[slot].address) {
    return std::nullopt;
  }
  return bindings_[slot].configuration.endpoint();
}

std::optional<Endpoint> Scheduler::possibleEndpoint(std::size_t slot) const {
  if (slot >= BINDINGS || !bindings_[slot].address) {
    return std::nullopt;
  }
  return bindings_[slot].configuration.possibleEndpoint();
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
