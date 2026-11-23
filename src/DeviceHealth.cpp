#include "SEN059x/DeviceHealth.h"

#include <limits>

#include "../internal/SaturatingIncrement.h"

namespace SEN059x {

const char* driverStateName(DriverState state) {
  switch (state) {
    case DriverState::UNINIT:
      return "UNINIT";
    case DriverState::READY:
      return "READY";
    case DriverState::DEGRADED:
      return "DEGRADED";
    case DriverState::OFFLINE:
      return "OFFLINE";
  }
  return "UNKNOWN";
}

Status DeviceHealth::bind(DeviceModel model, std::uint8_t address, std::uint8_t offlineThreshold,
                          AddressPolicy addressPolicy) {
  if ((model != DeviceModel::SEN0591 && model != DeviceModel::SEN0592) ||
      !isValidAddress(address, addressPolicy) || offlineThreshold == 0) {
    return {Err::INVALID_CONFIG, 0, "Invalid device health binding or offline threshold"};
  }
  unbind();
  offlineThreshold_ = offlineThreshold;
  return Ok();
}

void DeviceHealth::unbind() {
  snapshot_ = {};
  active_ = false;
  offlineThreshold_ = 0;
}

bool DeviceHealth::beginAttempt(HealthAttempt& out) {
  if (offlineThreshold_ == 0 || active_ || sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    return false;
  }
  out.owner_ = this;
  out.sequence_ = ++sequence_;
  active_ = true;
  return true;
}

bool DeviceHealth::discard(const HealthAttempt& attempt) {
  if (!active_ || attempt.owner_ != this || attempt.sequence_ != sequence_) {
    return false;
  }
  active_ = false;
  return true;
}

bool DeviceHealth::recordReply(const HealthAttempt& attempt, std::uint32_t completedMs,
                               bool timeValid) {
  if (!discard(attempt)) {
    return false;
  }
  snapshot_.initialized = true;
  snapshot_.state = DriverState::READY;
  snapshot_.consecutiveFailures = 0;
  internal::saturatingIncrement(snapshot_.totalSuccess);
  snapshot_.lastOkMs = completedMs;
  snapshot_.lastOkTimeValid = timeValid;
  return true;
}

bool DeviceHealth::recordFailure(const HealthAttempt& attempt, Status failure,
                                 std::uint32_t completedMs, bool timeValid) {
  switch (failure.code) {
    case Err::TIMEOUT:
    case Err::IO:
    case Err::CRC_ERROR:
    case Err::FRAME_ERROR:
      break;
    default:
      return false;
  }
  if (!discard(attempt)) {
    return false;
  }
  internal::saturatingIncrement(snapshot_.totalFailures);
  internal::saturatingIncrement(snapshot_.consecutiveFailures);
  snapshot_.state = snapshot_.consecutiveFailures >= offlineThreshold_ ? DriverState::OFFLINE
                                                                       : DriverState::DEGRADED;
  snapshot_.lastError = failure;
  snapshot_.lastErrorMs = completedMs;
  snapshot_.lastErrorTimeValid = timeValid;
  return true;
}

bool DeviceHealth::isOnline() const {
  return snapshot_.initialized &&
         (state() == DriverState::READY || state() == DriverState::DEGRADED);
}

void DeviceHealth::resetStatistics() {
  snapshot_.totalSuccess = 0;
  snapshot_.totalFailures = 0;
}

}  // namespace SEN059x
