#pragma once

#include <cstdint>
#include <optional>

#include "FieldCoreMainOwner.h"

#include <SEN059x/SEN059x.h>

namespace FieldCoreRodtester {
class Binding;
}

// Host consumer of the FieldCore main binding contract at eccdbdc. One catalog
// slot represents the configured measurement kind of this installed sensor.
namespace FieldCoreMain {

enum class DeviceRequestKind : std::uint8_t { Probe, Measure, ReadLast };
enum class DeviceResultStatus : std::uint8_t {
  Ok,
  Disabled,
  Absent,
  Timeout,
  Stale,
  Cancelled,
  Failed
};
enum class DeviceModuleStartStatus : std::uint8_t {
  Accepted,
  Busy,
  Disabled,
  Unsupported,
  Rejected
};
enum class DeviceModulePollState : std::uint8_t { Idle, Running, Complete, NeedsBusRecovery };
enum class HealthState : std::uint8_t { Initializing, Ok, Degraded, Fault, Unknown, Disabled };
enum class PresenceState : std::uint8_t { Unknown, Present, Absent, Disabled };
enum class Rs485ParserError : std::uint8_t {
  None,
  BadCrc,
  TruncatedFrame,
  UnexpectedAddress,
  UnexpectedFunction,
  ExceptionResponse,
  InvalidText,
  OutOfRangeValue,
  UnsupportedFrame,
  ValueCountMismatch
};
const char* healthStateName(HealthState state);
const char* presenceStateName(PresenceState state);

struct Deadline64 {
  bool active = false;
  std::uint64_t expiresAtMs = 0;
};
struct DeviceCommand {
  std::uint32_t requestId = 0;
  std::uint16_t deviceId = 0;
  DeviceRequestKind kind = DeviceRequestKind::Probe;
  Deadline64 deadline{};
};
struct DeviceModuleStartResult {
  DeviceModuleStartStatus status;
  ErrorDetail error{};
};
struct DeviceMeasurementResult {
  std::uint32_t requestId = 0;
  std::uint16_t deviceId = 0;
  DeviceRequestKind kind = DeviceRequestKind::Probe;
  Deadline64 deadline{};
  DeviceResultStatus status = DeviceResultStatus::Failed;
  ErrorDetail error{};
  Rs485ParserError parserError = Rs485ParserError::None;
  std::uint64_t completedUptimeMs = 0;
  std::uint8_t readingCount = 0;
  std::uint64_t validMask = 0, staleMask = 0, errorMask = 0;
  float values[1]{};
};
struct DeviceReadingDescriptor {
  std::uint8_t id;
  const char* key;
  const char* unit;
};
struct DeviceReadingCatalog {
  const DeviceReadingDescriptor* readings = nullptr;
  std::uint8_t readingCount = 0;
};
struct DeviceModuleStatus {
  std::uint16_t deviceId = 0;
  bool enabled = false;
  PresenceState presence = PresenceState::Unknown;
  HealthState health = HealthState::Unknown;
  ErrorDetail lastError{};
  std::uint64_t lastAttemptUptimeMs = 0, lastSuccessUptimeMs = 0, lastErrorUptimeMs = 0;
  std::uint32_t successCount = 0, errorCount = 0, timeoutCount = 0;
};
struct Rs485InstanceRuntimeConfig {
  std::uint8_t address = 1;
  std::uint32_t baud = 9600;
  std::uint32_t responseTimeoutMs = 1000;
  std::uint8_t retries = 0;
};
struct Rs485DeviceStatus {
  DeviceModuleStatus device{};
  std::uint8_t address = 0;
  std::uint32_t baud = 0;
  std::uint32_t responseTimeoutMs = 0;
  std::uint8_t retries = 0;
  DeviceResultStatus lastStatus = DeviceResultStatus::Failed;
  Rs485ParserError lastParserError = Rs485ParserError::None;
  std::uint32_t consecutiveMissCount = 0, parserErrorCount = 0, staleCount = 0;
  std::uint8_t readingCount = 0;
  std::uint64_t validMask = 0;
  float values[1]{};
  std::optional<SEN059x::SettingReadback> lastProbe;
  SEN059x::HealthSnapshot link;
};

class FieldCoreMainBinding {
 public:
  FieldCoreMainBinding(std::uint16_t deviceId, SEN059x::DeviceModel model,
                       SEN059x::MeasurementKind kind, Rs485SerialFormat format,
                       std::uint8_t staleAfterMisses = 2, std::uint8_t faultAfterMisses = 3,
                       std::uint32_t maxAgeMs = 1000);
  bool bind(ErrorDetail& error);
  bool applyRuntimeConfig(const Rs485InstanceRuntimeConfig& config, std::uint64_t now,
                          ErrorDetail& error);
  DeviceModuleStartResult start(const DeviceCommand& command, std::uint64_t now);
  DeviceModulePollState poll(std::uint64_t now);
  bool takeStep(Rs485ModuleStep& step);
  void acceptObservation(const Rs485BusObservation& observation, std::uint64_t now);
  bool takeResult(DeviceMeasurementResult& result);
  void setEnabled(bool enabled, std::uint64_t now);
  void cancel(std::uint64_t now, ErrorDetail reason = {ErrorCode::Busy, 0});
  void onBusInvalidated(std::uint64_t now, ErrorDetail reason = {ErrorCode::Unavailable, 0});
  DeviceModuleStatus snapshot() const { return status_.device; }
  const DeviceReadingCatalog& readingCatalog() const { return catalog_; }
  bool copyStatus(Rs485DeviceStatus& status) const;
  // Native history ages use the library's 32-bit monotonic interval contract.
  SEN059x::SampleSnapshot sample(std::uint64_t now) const;

 private:
  friend class FieldCoreRodtester::Binding;
  enum class Phase { Idle, Ready, AwaitObservation, RetryWait, Complete };
  std::uint16_t buildRequest(std::uint8_t* bytes, std::size_t capacity);
  void consumeObservation(const Rs485BusObservation& observation, std::uint64_t now,
                          bool closedOnTime);
  void activateEndpoint();
  void finish(DeviceResultStatus status, std::uint64_t now, ErrorDetail error = {},
              Rs485ParserError parser = Rs485ParserError::None, bool miss = false);
  void updateStatus(DeviceResultStatus status, std::uint64_t now, ErrorDetail error,
                    Rs485ParserError parser, bool miss);
  void failOrRetry(DeviceResultStatus status, std::uint64_t now, ErrorDetail error,
                   Rs485ParserError parser, bool miss = false);
  SEN059x::DeviceModel model_;
  SEN059x::MeasurementKind kind_;
  Rs485SerialFormat format_;
  std::uint8_t staleAfterMisses_, faultAfterMisses_;
  std::uint32_t maxAgeMs_;
  DeviceReadingCatalog catalog_;
  Rs485InstanceRuntimeConfig config_, commandConfig_;
  bool bound_ = false;
  bool rodtester_ = false;
  Phase phase_ = Phase::Idle;
  DeviceCommand command_;
  DeviceMeasurementResult result_;
  Rs485DeviceStatus status_;
  SEN059x::MeasurementCache cache_;
  SEN059x::SampleRequest sampleRequest_;
  SEN059x::SettingRequest probeRequest_;
  SEN059x::DeviceHealth link_;
  std::uint8_t retriesUsed_ = 0;
  std::uint64_t wakeMs_ = 0;
  std::optional<std::uint64_t> usableUptimeMs_;
};

}  // namespace FieldCoreMain
