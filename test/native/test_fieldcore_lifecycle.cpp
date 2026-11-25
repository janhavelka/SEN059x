#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "FieldCoreRodtesterBinding.h"

namespace {
using namespace FieldCoreMain;
namespace Rod = FieldCoreRodtester;
using Kind = DeviceRequestKind;
using Result = DeviceResultStatus;
using Reason = Rs485TransportTerminalReason;
using SEN059x::DeviceModel;
using SEN059x::DriverState;
using SEN059x::Freshness;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

using Request = std::array<std::uint8_t, 8>;
constexpr Request BAUD_TX{1, 3, 2, 1, 0, 1, 0xD4, 0x72};
constexpr Request DISTANCE_TX{1, 3, 1, 1, 0, 1, 0xD4, 0x36};

struct Frame {
  const std::uint8_t* bytes;
  std::uint16_t length;
};

// Examples 5 and 2 appear in both SKU manuals. The exception is synthetic.
constexpr std::uint8_t BAUD_BYTES[]{1, 3, 2, 0, 3, 0xF8, 0x45};
constexpr std::uint8_t DISTANCE_BYTES[]{1, 3, 2, 2, 0xEF, 0xF8, 0xA8};
constexpr std::uint8_t EXCEPTION_BYTES[]{1, 0x83, 2, 0xC0, 0xF1};
constexpr Frame BAUD{BAUD_BYTES, sizeof(BAUD_BYTES)};
constexpr Frame DISTANCE{DISTANCE_BYTES, sizeof(DISTANCE_BYTES)};
constexpr Frame EXCEPTION{EXCEPTION_BYTES, sizeof(EXCEPTION_BYTES)};
constexpr Frame SILENCE{nullptr, 0};

bool takeStep(FieldCoreMainBinding& binding, Rs485ModuleStep& step, std::uint64_t) {
  return binding.takeStep(step);
}

bool takeStep(Rod::Binding& binding, Rod::Rs485ModuleStep& step, std::uint64_t now) {
  return binding.takeStep(step, now);
}

void deliver(FieldCoreMainBinding& binding, const Rs485ModuleStep& step, const DeviceCommand& cmd,
             Frame frame, std::uint64_t start, std::uint64_t end) {
  Owner owner;
  // At 9600 baud the 2 ms pre-TX guard and eight 8N1 bytes take over 10 ms.
  CHECK(owner.dispatch(step, start + 11, cmd.deadline.expiresAtMs));
  Rs485BusObservation obs;
  if (frame.length == EXCEPTION.length) {
    // The five exception bytes alone do not meet main's seven-byte target.
    CHECK(!owner.receive(frame.bytes, frame.length, start + 25, obs));
    CHECK(binding.poll(start + 25) == DeviceModulePollState::Running);
    CHECK(owner.receive(nullptr, 0, end, obs));
  } else {
    CHECK(owner.receive(frame.bytes, frame.length, end, obs));
  }
  CHECK(obs.reason == (frame.length == 7 ? Reason::Complete : Reason::ResponseTimeout));
  CHECK(obs.rxLength == frame.length);
  binding.acceptObservation(obs, end);
}

void deliver(Rod::Binding& binding, const Rod::Rs485ModuleStep& step, const DeviceCommand&,
             Frame frame, std::uint64_t start, std::uint64_t end) {
  Rod::Delivery delivery{};
  delivery.bytes = frame.bytes;
  delivery.length = frame.length;
  delivery.totalBytes = frame.length;
  delivery.txAccepted = step.transaction.txLength;
  delivery.drained = true;
  delivery.txFinishedUs = (start + 11) * 1000;
  delivery.observedUs = end * 1000;
  if (frame.length) {
    delivery.timingQualified = true;
    delivery.earliestUs = delivery.observedUs - 500;
    delivery.latestUs = delivery.observedUs - 100;
  }
  const auto obs = Rod::observe(step, delivery);
  CHECK(obs.reason == (frame.length ? Reason::Complete : Reason::ResponseTimeout));
  CHECK(obs.responseConfirmed == (frame.length != 0));
  binding.acceptObservation(obs, end);
}

template <typename Step, typename Binding>
void lifecycle(Binding& binding, DeviceModel model, std::uint64_t exceptionEndMs) {
  constexpr bool mainProfile = std::is_same_v<Binding, FieldCoreMainBinding>;
  ErrorDetail error{};
  CHECK(binding.bind(error));
  CHECK(binding.applyRuntimeConfig({1, 9600, 100, 0}, 0, error));
  std::uint32_t requestId = 0;
  const auto run = [&](Kind kind, std::uint64_t start, std::uint64_t end, Frame frame,
                       Result expected) {
    const DeviceCommand cmd{++requestId, 11, kind, {true, start + 200}};
    CHECK(binding.start(cmd, start).status == DeviceModuleStartStatus::Accepted);
    Step step{};
    if (kind == Kind::ReadLast) {
      CHECK(!takeStep(binding, step, start));
      CHECK(step.action == Rs485ModuleAction::None);
    } else {
      CHECK(binding.poll(start) == DeviceModulePollState::Running);
      CHECK(takeStep(binding, step, start));
      CHECK(step.action == Rs485ModuleAction::RequestTransaction);
      const auto& request = kind == Kind::Probe ? BAUD_TX : DISTANCE_TX;
      CHECK(step.transaction.txLength == request.size());
      CHECK(std::memcmp(step.transaction.txBytes, request.data(), request.size()) == 0);
      deliver(binding, step, cmd, frame, start, end);
    }
    CHECK(binding.poll(end) == DeviceModulePollState::Complete);
    DeviceMeasurementResult out;
    CHECK(binding.takeResult(out));
    CHECK(out.requestId == cmd.requestId && out.deviceId == cmd.deviceId && out.kind == kind);
    CHECK(out.deadline.active && out.deadline.expiresAtMs == cmd.deadline.expiresAtMs);
    CHECK(out.completedUptimeMs == end && out.status == expected);
    const bool reading = kind != Kind::Probe;
    CHECK(out.readingCount == (reading ? 1 : 0));
    CHECK(out.validMask == (reading && expected == Result::Ok ? 1u : 0u));
    CHECK(out.staleMask == (reading && expected == Result::Stale ? 1u : 0u));
    CHECK(out.errorMask ==
          (reading && expected != Result::Ok && expected != Result::Stale ? 1u : 0u));
    DeviceMeasurementResult duplicate;
    CHECK(!binding.takeResult(duplicate) && binding.poll(end) == DeviceModulePollState::Idle);
    return out;
  };

  Rs485DeviceStatus status{};
  run(Kind::Probe, 0, 27, BAUD, Result::Ok);
  CHECK(binding.copyStatus(status));
  CHECK(status.lastProbe && status.lastProbe->known && status.lastProbe->rawWord == 3);
  CHECK(status.validMask == 0 && status.readingCount == 0 && status.link.totalSuccess == 1);
  CHECK(binding.sample(27).usable.freshness == Freshness::NeverSampled);
  CHECK(run(Kind::Measure, 47, 75, DISTANCE, Result::Ok).values[0] == 751);
  const auto history = [&](std::uint64_t now, Freshness expected) {
    const auto sample = binding.sample(now);
    for (const auto* entry : {&sample.raw, &sample.usable}) {
      CHECK(entry->measurement.completedMs == 75 && entry->ageMs == now - 75);
      CHECK(entry->measurement.model == model && entry->measurement.address == 1);
      CHECK(entry->measurement.rawWord == 751 && entry->measurement.value == 751);
      CHECK(entry->freshness == expected);
    }
  };
  CHECK(run(Kind::ReadLast, 1075, 1075, SILENCE, Result::Ok).values[0] == 751);
  history(1075, Freshness::Fresh);
  run(Kind::ReadLast, 1076, 1076, SILENCE, Result::Stale);
  history(1076, Freshness::Stale);
  CHECK(binding.copyStatus(status));
  CHECK(status.lastStatus == Result::Ok && status.device.lastAttemptUptimeMs == 75);
  CHECK(status.device.successCount == 2 && status.device.errorCount == 0);
  CHECK(status.link.totalSuccess == 2 && status.link.totalFailures == 0);

  auto failed = run(Kind::Measure, 1100, exceptionEndMs, EXCEPTION, Result::Failed);
  CHECK(failed.parserError == Rs485ParserError::ExceptionResponse &&
        failed.error.code == ErrorCode::Rs485ParserError && failed.error.backendCode == 2);
  CHECK(binding.copyStatus(status));
  CHECK(status.lastStatus == Result::Failed && status.lastParserError == failed.parserError);
  CHECK(status.parserErrorCount == 1 && status.consecutiveMissCount == 0 && status.staleCount == 0);
  CHECK(status.device.health == HealthState::Degraded &&
        status.device.presence == PresenceState::Present);
  CHECK(status.device.lastAttemptUptimeMs == exceptionEndMs &&
        status.device.lastErrorUptimeMs == exceptionEndMs &&
        status.device.lastSuccessUptimeMs == 75);
  CHECK(status.link.state == DriverState::READY && status.link.totalSuccess == 3 &&
        status.link.lastOkMs == exceptionEndMs && status.link.totalFailures == 0);
  history(exceptionEndMs, Freshness::Stale);

  failed = run(Kind::ReadLast, 1230, 1230, SILENCE, Result::Failed);
  CHECK(failed.error.code == ErrorCode::Rs485NoCachedSnapshot);
  CHECK(binding.copyStatus(status));
  CHECK(status.lastStatus == Result::Failed &&
        status.lastParserError == Rs485ParserError::ExceptionResponse);
  CHECK(status.device.lastError.code ==
        (mainProfile ? ErrorCode::Rs485NoCachedSnapshot : ErrorCode::Rs485ParserError));
  const auto lastFailedMs = mainProfile ? 1230 : exceptionEndMs;
  CHECK(status.device.lastAttemptUptimeMs == lastFailedMs &&
        status.device.lastErrorUptimeMs == lastFailedMs);
  constexpr auto errorsBeforeMisses = mainProfile ? 2u : 1u;
  CHECK(status.device.errorCount == errorsBeforeMisses &&
        status.parserErrorCount == errorsBeforeMisses);
  CHECK(status.link.totalSuccess == 3 && status.link.totalFailures == 0);

  for (std::uint32_t miss = 1; miss <= 3; ++miss) {
    const auto start = 1100 + 200 * miss;
    const auto end = start + 111;
    run(Kind::Measure, start, end, SILENCE, miss == 2 ? Result::Stale : Result::Timeout);
    CHECK(binding.copyStatus(status));
    CHECK(status.lastStatus == (!mainProfile && miss == 2 ? Result::Stale : Result::Timeout));
    CHECK(status.lastParserError == Rs485ParserError::None);
    CHECK(status.consecutiveMissCount == miss && status.parserErrorCount == errorsBeforeMisses);
    CHECK(status.staleCount == (miss >= 2 ? 1u : 0u));
    CHECK(status.device.presence == (miss >= 2 ? PresenceState::Absent : PresenceState::Present));
    CHECK(status.device.health == (miss == 3 ? HealthState::Fault : HealthState::Degraded));
    CHECK(status.device.timeoutCount == miss &&
          status.device.errorCount == miss + errorsBeforeMisses);
    CHECK(status.device.lastAttemptUptimeMs == end && status.device.lastErrorUptimeMs == end &&
          status.device.lastSuccessUptimeMs == 75);
    CHECK(status.readingCount == 1 && status.validMask == 1 && status.values[0] == 751);
    CHECK(status.link.state == (miss == 3 ? DriverState::OFFLINE : DriverState::DEGRADED));
    CHECK(status.link.consecutiveFailures == miss && status.link.totalFailures == miss &&
          status.link.totalSuccess == 3);
    history(end, Freshness::Stale);
  }

  // A probe recovers communication without changing the historical distance or its age.
  run(Kind::Probe, 1898, 1925, BAUD, Result::Ok);
  run(Kind::ReadLast, 1926, 1926, SILENCE, Result::Stale);
  CHECK(binding.copyStatus(status));
  CHECK(status.device.health == HealthState::Ok &&
        status.device.presence == PresenceState::Present);
  CHECK(status.lastStatus == Result::Ok && status.lastParserError == Rs485ParserError::None);
  CHECK(status.consecutiveMissCount == 0 && status.staleCount == 1 &&
        status.parserErrorCount == errorsBeforeMisses);
  CHECK(status.device.lastAttemptUptimeMs == 1925 && status.device.lastSuccessUptimeMs == 1925 &&
        status.device.lastErrorUptimeMs == 1811);
  const auto device = binding.snapshot();
  CHECK(device.deviceId == status.device.deviceId && device.health == status.device.health &&
        device.presence == status.device.presence && device.lastErrorUptimeMs == 1811 &&
        device.lastSuccessUptimeMs == 1925 && device.errorCount == status.device.errorCount);
  CHECK(status.device.successCount == 3 && status.device.errorCount == errorsBeforeMisses + 3 &&
        status.device.timeoutCount == 3);
  CHECK(status.link.state == DriverState::READY && status.link.consecutiveFailures == 0);
  CHECK(status.link.totalFailures == 3 && status.link.totalSuccess == 4);
  CHECK(status.link.lastOkMs == 1925 && status.link.lastErrorMs == 1811);
  history(1926, Freshness::Stale);
}
}  // namespace

int main() {
  constexpr Rs485SerialFormat format{9600, 8, Rs485Parity::None, 1};
  for (const auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    constexpr auto kind = SEN059x::MeasurementKind::RealTimeDistance;
    FieldCoreMainBinding mainBinding(11, model, kind, format);
    lifecycle<Rs485ModuleStep>(mainBinding, model, 1211);
    Rod::Binding rodBinding(11, model, kind, format, Rod::Rs485EchoPolicy::QualifiedResponse);
    lifecycle<Rod::Rs485ModuleStep>(rodBinding, model, 1125);
  }
}
