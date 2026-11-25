#include "FieldCoreMainBinding.h"

#include <algorithm>
#include <limits>

namespace FieldCoreMain {
namespace {

const DeviceReadingDescriptor* descriptor(SEN059x::MeasurementKind kind) {
  using Kind = SEN059x::MeasurementKind;
  static constexpr DeviceReadingDescriptor readings[] = {{0, "processed_distance", "mm"},
                                                         {0, "real_time_distance", "mm"},
                                                         {0, "temperature", "degC"},
                                                         {0, "echo_time", "us"}};
  switch (kind) {
    case Kind::ProcessedDistance:
      return &readings[0];
    case Kind::RealTimeDistance:
      return &readings[1];
    case Kind::Temperature:
      return &readings[2];
    case Kind::EchoTime:
      return &readings[3];
  }
  return nullptr;
}

Rs485ParserError projectParser(SEN059x::ReadResponseError error) {
  using Error = SEN059x::ReadResponseError;
  switch (error) {
    case Error::NONE:
      return Rs485ParserError::None;
    case Error::FRAME_TOO_SHORT:
      return Rs485ParserError::TruncatedFrame;
    case Error::CRC_MISMATCH:
      return Rs485ParserError::BadCrc;
    case Error::UNEXPECTED_ADDRESS:
      return Rs485ParserError::UnexpectedAddress;
    case Error::UNEXPECTED_FUNCTION:
      return Rs485ParserError::UnexpectedFunction;
    case Error::BYTE_COUNT_MISMATCH:
    case Error::LENGTH_MISMATCH:
      return Rs485ParserError::ValueCountMismatch;
    case Error::EXCEPTION_RESPONSE:
      return Rs485ParserError::ExceptionResponse;
    case Error::INVALID_ARGUMENT:
      return Rs485ParserError::UnsupportedFrame;
  }
  return Rs485ParserError::UnsupportedFrame;
}

void increment(std::uint32_t& count) {
  if (count != std::numeric_limits<std::uint32_t>::max()) {
    ++count;
  }
}

}  // namespace

const char* healthStateName(HealthState state) {
  switch (state) {
    case HealthState::Initializing:
      return "initializing";
    case HealthState::Ok:
      return "ok";
    case HealthState::Degraded:
      return "degraded";
    case HealthState::Fault:
      return "fault";
    case HealthState::Disabled:
      return "disabled";
    case HealthState::Unknown:
      return "unknown";
  }
  return "unknown";
}

const char* presenceStateName(PresenceState state) {
  switch (state) {
    case PresenceState::Present:
      return "present";
    case PresenceState::Absent:
      return "absent";
    case PresenceState::Disabled:
      return "disabled";
    case PresenceState::Unknown:
      return "unknown";
  }
  return "unknown";
}

FieldCoreMainBinding::FieldCoreMainBinding(std::uint16_t deviceId, SEN059x::DeviceModel model,
                                           SEN059x::MeasurementKind kind, Rs485SerialFormat format,
                                           std::uint8_t staleAfterMisses,
                                           std::uint8_t faultAfterMisses, std::uint32_t maxAgeMs)
    : model_(model),
      kind_(kind),
      format_(format),
      staleAfterMisses_(staleAfterMisses),
      faultAfterMisses_(faultAfterMisses),
      maxAgeMs_(maxAgeMs) {
  status_.device.deviceId = deviceId;
  status_.device.enabled = true;
  status_.device.health = HealthState::Initializing;
  config_.baud = format.baud;
  catalog_.readings = descriptor(kind);
  catalog_.readingCount = catalog_.readings ? 1 : 0;
}

bool FieldCoreMainBinding::bind() {
  if (bound_) {
    return true;
  }
  if (!status_.device.deviceId || !catalog_.readings || !rs485SerialFormatValid(format_) ||
      !staleAfterMisses_ || faultAfterMisses_ <= staleAfterMisses_ ||
      (model_ != SEN059x::DeviceModel::SEN0591 && model_ != SEN059x::DeviceModel::SEN0592)) {
    return false;
  }
  bound_ = true;
  activateEndpoint();
  return true;
}

bool FieldCoreMainBinding::applyRuntimeConfig(const Rs485InstanceRuntimeConfig& config) {
  auto format = format_;
  format.baud = config.baud;
  if (!bound_ || !rs485SerialFormatValid(format) || !SEN059x::isValidAddress(config.address) ||
      config.responseTimeoutMs < 100 || config.responseTimeoutMs > 5000 || config.retries > 3) {
    return false;
  }
  config_ = config;
  if (phase_ == Phase::Idle) {
    activateEndpoint();
  }
  return true;
}

// Keep a running request and its terminal result on the original endpoint.
// Switch the histories to the new endpoint after its result has been taken.
void FieldCoreMainBinding::activateEndpoint() {
  if (config_.address == status_.address && config_.baud == status_.baud) {
    return;
  }
  const auto deviceId = status_.device.deviceId;
  const bool enabled = status_.device.enabled;
  status_ = {};
  status_.device.deviceId = deviceId;
  status_.device.enabled = enabled;
  status_.device.health = enabled ? HealthState::Initializing : HealthState::Disabled;
  status_.device.presence = enabled ? PresenceState::Unknown : PresenceState::Disabled;
  status_.address = config_.address;
  status_.baud = config_.baud;
  cache_.bind(model_, config_.address);
  link_.bind(model_, config_.address, faultAfterMisses_);
  usableUptimeMs_.reset();
}

DeviceModuleStartResult FieldCoreMainBinding::start(const DeviceCommand& command,
                                                    std::uint64_t now) {
  if (!bound_ || !command.requestId || command.deviceId != status_.device.deviceId ||
      !command.deadline.active || command.deadline.expiresAtMs <= now) {
    return {DeviceModuleStartStatus::Rejected, {ErrorCode::InvalidArgument, 0}};
  }
  if (!status_.device.enabled) {
    return {DeviceModuleStartStatus::Disabled, {ErrorCode::Unavailable, 0}};
  }
  if (phase_ != Phase::Idle) {
    return {DeviceModuleStartStatus::Busy, {ErrorCode::Busy, 0}};
  }
  if (command.kind != DeviceRequestKind::Probe && command.kind != DeviceRequestKind::Measure &&
      command.kind != DeviceRequestKind::ReadLast) {
    return {DeviceModuleStartStatus::Unsupported, {ErrorCode::Unsupported, 0}};
  }
  command_ = command;
  commandConfig_ = config_;
  retriesUsed_ = 0;
  phase_ = Phase::Ready;
  if (command.kind == DeviceRequestKind::ReadLast) {
    if (!usableUptimeMs_ || status_.lastStatus != DeviceResultStatus::Ok) {
      finish(DeviceResultStatus::Failed, now, {ErrorCode::Rs485NoCachedSnapshot, 0});
    } else if (now - *usableUptimeMs_ > maxAgeMs_) {
      finish(DeviceResultStatus::Stale, now, {ErrorCode::Rs485DeviceStale, 0});
    } else {
      finish(DeviceResultStatus::Ok, now);
    }
  }
  return {DeviceModuleStartStatus::Accepted, {}};
}

DeviceModulePollState FieldCoreMainBinding::poll(std::uint64_t now) {
  if (phase_ == Phase::Idle) {
    return DeviceModulePollState::Idle;
  }
  if (phase_ != Phase::Complete && now >= command_.deadline.expiresAtMs) {
    finish(DeviceResultStatus::Timeout, now, {ErrorCode::DeadlineExpired, 0});
  }
  if (phase_ == Phase::RetryWait && now >= wakeMs_) {
    phase_ = Phase::Ready;
  }
  return phase_ == Phase::Complete ? DeviceModulePollState::Complete
                                   : DeviceModulePollState::Running;
}

bool FieldCoreMainBinding::takeStep(Rs485ModuleStep& step) {
  step = {};
  if (phase_ == Phase::RetryWait) {
    step.action = Rs485ModuleAction::WaitUntil;
    step.wakeUptimeMs = wakeMs_;
    return true;
  }
  if (phase_ != Phase::Ready) {
    return false;
  }
  auto& tx = step.transaction;
  tx.format = format_;
  tx.format.baud = commandConfig_.baud;
  const auto length =
      command_.kind == DeviceRequestKind::Probe
          ? SEN059x::buildSettingRead(model_, commandConfig_.address, SEN059x::Setting::Baud,
                                      tx.txBytes, sizeof(tx.txBytes), probeRequest_)
          : cache_.buildRead(kind_, tx.txBytes, sizeof(tx.txBytes), sampleRequest_);
  tx.txLength = static_cast<std::uint16_t>(length);
  tx.rxCapacity = kRs485TransactionRxCapacity;
  tx.responseTimeoutMs = commandConfig_.responseTimeoutMs;
  tx.completionMode = Rs485CompletionMode::FixedLength;
  tx.expectedLength = 7;
  if (!rs485BusTransactionValid(tx)) {
    finish(DeviceResultStatus::Failed, 0, {ErrorCode::InvalidArgument, 0},
           Rs485ParserError::UnsupportedFrame);
    return false;
  }
  step.action = Rs485ModuleAction::RequestTransaction;
  phase_ = Phase::AwaitObservation;
  return true;
}

void FieldCoreMainBinding::acceptObservation(const Rs485BusObservation& obs, std::uint64_t now) {
  if (phase_ != Phase::AwaitObservation) {
    return;
  }
  now = std::max(now, obs.completedUptimeMs);
  const auto tick = static_cast<std::uint32_t>(now);
  using Reason = Rs485TransportTerminalReason;
  using Err = SEN059x::Err;
  // A transaction request may expire before transmission. Only a reported TX
  // enters the library's per-device communication statistics.
  SEN059x::HealthAttempt attempt;
  const bool sent = obs.txCount > 0 && link_.beginAttempt(attempt);
  if (obs.reason == Reason::DeadlineExpired || now >= command_.deadline.expiresAtMs ||
      obs.reason == Reason::BackendError) {
    const bool expired =
        obs.reason == Reason::DeadlineExpired || now >= command_.deadline.expiresAtMs;
    auto error = expired ? ErrorDetail{ErrorCode::DeadlineExpired, 0} : obs.error;
    if (error.code == ErrorCode::Ok) {
      error = {ErrorCode::InternalFault, 0};
    }
    if (sent) {
      link_.recordFailure(attempt, {expired ? Err::TIMEOUT : Err::IO, error.backendCode, "owner"},
                          tick, true);
    }
    finish(expired ? DeviceResultStatus::Timeout : DeviceResultStatus::Failed, now, error);
    return;
  }
  if (obs.reason == Reason::ResponseTimeout && obs.rxLength == 0) {
    if (sent) {
      link_.recordFailure(attempt, {Err::TIMEOUT, 0, "No response"}, tick, true);
    }
    failOrRetry(DeviceResultStatus::Timeout, now, {ErrorCode::Timeout, 0}, Rs485ParserError::None,
                sent);
    return;
  }

  SEN059x::ReadResponseError validation = SEN059x::ReadResponseError::INVALID_ARGUMENT;
  SEN059x::Status parsed{Err::INVALID_CONFIG, 0, "Invalid observation"};
  if (sent && obs.rxLength <= kRs485TransactionRxCapacity && obs.rxCount == obs.rxLength) {
    if (command_.kind == DeviceRequestKind::Probe) {
      SEN059x::SettingReadback probe;
      parsed = SEN059x::decodeSettingResponse(obs.rxBytes, obs.rxLength, probeRequest_, probe,
                                              validation);
      if (parsed) {
        status_.lastProbe = probe;
      }
    } else {
      parsed = cache_.complete(sampleRequest_, obs.rxBytes, obs.rxLength, tick, true, validation);
    }
  }
  const auto parser = projectParser(validation);
  if (sent) {
    if (parsed || parsed.code == Err::EXCEPTION) {
      link_.recordReply(attempt, tick, true);
    } else if (parser == Rs485ParserError::UnexpectedAddress ||
               parser == Rs485ParserError::UnsupportedFrame) {
      link_.discard(attempt);
    } else {
      link_.recordFailure(attempt, parsed, tick, true);
    }
  }
  if (!parsed) {
    ErrorDetail error{ErrorCode::Rs485ParserError, parsed.detail};
    if (parser == Rs485ParserError::BadCrc || parser == Rs485ParserError::TruncatedFrame) {
      error.code = ErrorCode::Rs485BadFrame;
    } else if (parser == Rs485ParserError::UnsupportedFrame) {
      error = {ErrorCode::Rs485ProtocolLibraryError, static_cast<std::int32_t>(parsed.code)};
    }
    failOrRetry(DeviceResultStatus::Failed, now, error, parser);
    return;
  }
  if (command_.kind == DeviceRequestKind::Measure) {
    if (!sample(now).raw.measurement.value) {
      failOrRetry(DeviceResultStatus::Failed, now, {ErrorCode::Rs485ParserError, 0},
                  Rs485ParserError::OutOfRangeValue);
      return;
    }
    usableUptimeMs_ = now;
  }
  finish(DeviceResultStatus::Ok, now);
}

void FieldCoreMainBinding::failOrRetry(DeviceResultStatus status, std::uint64_t now,
                                       ErrorDetail error, Rs485ParserError parser, bool miss) {
  cache_.discard(sampleRequest_);
  if (retriesUsed_ < commandConfig_.retries) {
    const std::uint64_t backoff = 75U << retriesUsed_;
    ++retriesUsed_;
    wakeMs_ = now + std::min(backoff, command_.deadline.expiresAtMs - now);
    phase_ = Phase::RetryWait;
    status_.lastParserError = parser;
    return;
  }
  if (status == DeviceResultStatus::Timeout && retriesUsed_ > 0) {
    error = {ErrorCode::Rs485RetryExhausted, 0};
  }
  finish(status, now, error, parser, miss);
}

void FieldCoreMainBinding::updateStatus(DeviceResultStatus status, std::uint64_t now,
                                        ErrorDetail error, Rs485ParserError parser, bool miss) {
  auto& device = status_.device;
  device.lastAttemptUptimeMs = now;
  status_.lastStatus = status;
  status_.lastParserError = parser;
  device.lastError = error;
  if (status == DeviceResultStatus::Ok) {
    device.health = HealthState::Ok;
    device.presence = PresenceState::Present;
    device.lastSuccessUptimeMs = now;
    increment(device.successCount);
    status_.consecutiveMissCount = 0;
    return;
  }
  device.lastErrorUptimeMs = now;
  increment(device.errorCount);
  if (status == DeviceResultStatus::Timeout) {
    increment(device.timeoutCount);
  }
  if (parser != Rs485ParserError::None) {
    increment(status_.parserErrorCount);
  }
  if (!miss) {
    device.health = HealthState::Degraded;
    return;
  }
  increment(status_.consecutiveMissCount);
  if (status_.consecutiveMissCount >= faultAfterMisses_) {
    device.presence = PresenceState::Absent;
    device.health = HealthState::Fault;
  } else if (status_.consecutiveMissCount >= staleAfterMisses_) {
    device.presence = PresenceState::Absent;
    device.health = HealthState::Degraded;
    if (status_.consecutiveMissCount == staleAfterMisses_) {
      increment(status_.staleCount);
    }
  } else if (device.presence != PresenceState::Present) {
    device.health = HealthState::Initializing;
  }
}

void FieldCoreMainBinding::finish(DeviceResultStatus status, std::uint64_t now, ErrorDetail error,
                                  Rs485ParserError parser, bool miss) {
  cache_.discard(sampleRequest_);
  if (command_.kind != DeviceRequestKind::ReadLast && status != DeviceResultStatus::Cancelled) {
    updateStatus(status, now, error, parser, miss);
  }
  if (command_.kind == DeviceRequestKind::Measure && miss &&
      status_.consecutiveMissCount >= staleAfterMisses_ &&
      status_.consecutiveMissCount < faultAfterMisses_) {
    status = DeviceResultStatus::Stale;
    error = {ErrorCode::Rs485DeviceStale, 0};
  }
  result_ = {};
  result_.requestId = command_.requestId;
  result_.deviceId = command_.deviceId;
  result_.kind = command_.kind;
  result_.deadline = command_.deadline;
  result_.completedUptimeMs = now;
  result_.status = status;
  result_.error = error;
  result_.parserError = parser;
  if (command_.kind != DeviceRequestKind::Probe) {
    result_.readingCount = catalog_.readingCount;
    if (status == DeviceResultStatus::Ok) {
      result_.validMask = 1;
      result_.values[0] = static_cast<float>(*sample(now).usable.measurement.value);
    } else if (status == DeviceResultStatus::Stale) {
      result_.staleMask = 1;
    } else {
      result_.errorMask = 1;
    }
  }
  phase_ = Phase::Complete;
}

bool FieldCoreMainBinding::takeResult(DeviceMeasurementResult& result) {
  if (phase_ != Phase::Complete) {
    return false;
  }
  result = result_;
  phase_ = Phase::Idle;
  activateEndpoint();
  return true;
}

void FieldCoreMainBinding::setEnabled(bool enabled, std::uint64_t now) {
  if (enabled == status_.device.enabled) {
    return;
  }
  if (!enabled) {
    cancel(now);
    if (bound_) {
      cache_.bind(model_, status_.address);
    }
    usableUptimeMs_.reset();
  }
  status_.device.enabled = enabled;
  status_.device.presence = enabled ? PresenceState::Unknown : PresenceState::Disabled;
  status_.device.health = enabled ? HealthState::Initializing : HealthState::Disabled;
}

void FieldCoreMainBinding::cancel(std::uint64_t now, ErrorDetail reason) {
  if (phase_ != Phase::Idle && phase_ != Phase::Complete) {
    finish(DeviceResultStatus::Cancelled, now, reason);
  }
}

void FieldCoreMainBinding::onBusInvalidated(std::uint64_t now, ErrorDetail reason) {
  if (phase_ != Phase::Idle && phase_ != Phase::Complete) {
    if (now >= command_.deadline.expiresAtMs) {
      finish(DeviceResultStatus::Timeout, now, {ErrorCode::DeadlineExpired, 0});
    } else {
      finish(DeviceResultStatus::Failed, now, reason);
    }
  } else if (status_.device.enabled) {
    status_.device.presence = PresenceState::Unknown;
    status_.device.health = HealthState::Initializing;
    status_.device.lastError = reason;
    status_.device.lastErrorUptimeMs = now;
  }
}

bool FieldCoreMainBinding::copyStatus(Rs485DeviceStatus& status) const {
  status = status_;
  // The product reports current runtime configuration even when a previous
  // command is completing against the saved commandConfig_.
  status.address = config_.address;
  status.baud = config_.baud;
  status.responseTimeoutMs = config_.responseTimeoutMs;
  status.retries = config_.retries;
  status.link = link_.healthSnapshot();
  status.readingCount = usableUptimeMs_.has_value() ? catalog_.readingCount : 0;
  status.validMask = usableUptimeMs_.has_value() ? 1 : 0;
  if (usableUptimeMs_) {
    status.values[0] = static_cast<float>(*sample(*usableUptimeMs_).usable.measurement.value);
  }
  return bound_;
}

SEN059x::SampleSnapshot FieldCoreMainBinding::sample(std::uint64_t now) const {
  return cache_.snapshot(kind_, static_cast<std::uint32_t>(now), maxAgeMs_);
}

}  // namespace FieldCoreMain
