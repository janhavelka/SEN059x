#include <cstdio>
#include <cstdlib>
#include <optional>
#include <type_traits>

#include "FieldCoreRodtesterBinding.h"

namespace {
using namespace FieldCoreMain;
namespace Rod = FieldCoreRodtester;
using Kind = DeviceRequestKind;
using Start = DeviceModuleStartStatus;
using Poll = DeviceModulePollState;
using Result = DeviceResultStatus;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

constexpr std::uint8_t DISTANCE[]{1, 3, 2, 2, 0xEF, 0xF8, 0xA8};

// Script an otherwise valid callback so accepting a delayed duplicate would publish a sample.
Rs485BusObservation distanceReply(FieldCoreMainBinding& binding, std::uint64_t completed) {
  Rs485ModuleStep step{};
  CHECK(binding.takeStep(step) && step.action == Rs485ModuleAction::RequestTransaction);
  return {Rs485TransportTerminalReason::Complete,
          {},
          DISTANCE,
          sizeof(DISTANCE),
          step.transaction.txLength,
          sizeof(DISTANCE),
          completed};
}

Rod::Rs485BusObservation distanceReply(Rod::Binding& binding, std::uint64_t completed) {
  Rod::Rs485ModuleStep step{};
  CHECK(binding.takeStep(step, completed - 20) &&
        step.action == Rs485ModuleAction::RequestTransaction);
  Rod::Delivery delivery{};
  delivery.bytes = DISTANCE;
  delivery.length = delivery.totalBytes = sizeof(DISTANCE);
  delivery.drained = true;
  delivery.txAccepted = step.transaction.txLength;
  delivery.txFinishedUs = (completed - 10) * 1000;
  delivery.earliestUs = delivery.latestUs = (completed - 1) * 1000;
  delivery.observedUs = completed * 1000;
  delivery.timingQualified = true;
  auto obs = Rod::observe(step, delivery);
  CHECK(obs.responseConfirmed);
  return obs;
}

void checkTerminal(const DeviceMeasurementResult& out, const DeviceCommand& cmd, Result expected,
                   std::uint64_t completed, ErrorCode error) {
  CHECK(out.requestId == cmd.requestId && out.deviceId == cmd.deviceId && out.kind == cmd.kind);
  CHECK(out.deadline.active && out.deadline.expiresAtMs == cmd.deadline.expiresAtMs);
  CHECK(out.completedUptimeMs == completed && out.status == expected && out.error.code == error);
  const bool reading = cmd.kind != Kind::Probe;
  CHECK(out.readingCount == (reading ? 1 : 0));
  CHECK(out.validMask == (reading && expected == Result::Ok ? 1u : 0u));
  CHECK(out.staleMask == 0);
  const bool errorBit =
      expected == Result::Cancelled || expected == Result::Timeout || expected == Result::Failed;
  CHECK(out.errorMask == (reading && errorBit ? 1u : 0u));
}

// In both pinned Rs485Task::pollModules implementations the signal belongs to
// the module. Inject it at this boundary; the pinned SHZK caller never emits it.
template <typename Binding>
bool collectOnPoll(Binding& binding, Poll state, std::uint64_t now, DeviceMeasurementResult& out) {
  if (state == Poll::NeedsBusRecovery) {
    binding.onBusInvalidated(now, {ErrorCode::Unavailable, 0});
  } else if (state != Poll::Complete) {
    return false;
  }
  return binding.takeResult(out);
}

template <typename Binding>
void collect(Binding& binding, const DeviceCommand& cmd, Result expected, std::uint64_t completed,
             ErrorCode error, std::uint64_t now) {
  CHECK(binding.poll(now) == Poll::Complete);
  DeviceMeasurementResult out;
  CHECK(collectOnPoll(binding, binding.poll(now), now, out));
  checkTerminal(out, cmd, expected, completed, error);
  CHECK(!binding.takeResult(out) && binding.poll(now) == Poll::Idle);
}

// Exercise the queued command's module-start branch of processMeasurementWork.
// Accepted work runs in the module. An admission error produces a direct result.
template <typename Binding>
std::optional<DeviceMeasurementResult> dispatchQueued(Binding& binding, const DeviceCommand& cmd,
                                                      std::uint64_t now) {
  const auto started = binding.start(cmd, now);
  if (started.status == Start::Accepted) {
    return std::nullopt;
  }
  const auto status = started.status == Start::Disabled ? Result::Disabled : Result::Failed;
  const auto error = started.status == Start::Busy || started.error.code == ErrorCode::Ok
                         ? ErrorDetail{ErrorCode::InternalFault, 0}
                         : started.error;
  return makeDeviceTerminalResult(cmd, binding.readingCatalog().readingCount, status, error, now);
}

template <typename Binding>
void administrative(Binding& binding) {
  constexpr Deadline64 deadline{true, 0x1000001FFULL};
  const DeviceCommand measure{101, 11, Kind::Measure, deadline};
  CHECK(binding.start(measure, 0).status == Start::Rejected);
  ErrorDetail error{};
  CHECK(binding.bind(error));
  CHECK(binding.start({103, 11, static_cast<Kind>(99), deadline}, 1).status == Start::Unsupported);
  CHECK(binding.poll(1) == Poll::Idle);

  CHECK(!dispatchQueued(binding, measure, 10));
  auto old = distanceReply(binding, 30);
  binding.acceptObservation(old, 30);
  binding.acceptObservation(old, 31);
  CHECK(binding.start(measure, 32).status == Start::Busy);
  collect(binding, measure, Result::Ok, 30, ErrorCode::Ok, 33);
  binding.acceptObservation(old, 34);
  CHECK(binding.poll(34) == Poll::Idle);

  // Reuse the request ID after collecting. Main's owner serializes its
  // synchronous callbacks. Rodtester can also reject an old generation while
  // the next exchange is awaiting a response with the same address and FC.
  CHECK(binding.start(measure, 40).status == Start::Accepted);
  binding.acceptObservation(old, 40);  // Before issuing the new step in either mode.
  auto late = distanceReply(binding, 60);
  if constexpr (std::is_same_v<Binding, Rod::Binding>) {
    CHECK(old.identity.generation != late.identity.generation);
    binding.acceptObservation(old, 50);
  }
  CHECK(binding.poll(50) == Poll::Running);
  binding.cancel(51, {});
  binding.acceptObservation(late, 60);
  collect(binding, measure, Result::Cancelled, 51, ErrorCode::Busy, 61);
  CHECK(binding.sample(61).usable.ageMs == 31);

  const DeviceCommand probe{104, 11, Kind::Probe, deadline};
  CHECK(binding.start(probe, 65).status == Start::Accepted);
  DeviceMeasurementResult recovered;
  CHECK(!collectOnPoll(binding, binding.poll(65), 65, recovered));
  CHECK(collectOnPoll(binding, Poll::NeedsBusRecovery, 66, recovered));
  checkTerminal(recovered, probe, Result::Failed, 66, ErrorCode::Unavailable);
  CHECK(binding.poll(67) == Poll::Idle && !binding.takeResult(recovered));
  CHECK(binding.sample(67).usable.ageMs == 37);

  DeviceCommand expired{105, 11, Kind::Measure, {true, 70}};
  CHECK(binding.start(expired, 68).status == Start::Accepted);
  binding.onBusInvalidated(70);
  collect(binding, expired, Result::Timeout, 70, ErrorCode::DeadlineExpired, 71);

  const DeviceCommand interrupted{106, 11, Kind::Measure, deadline};
  CHECK(binding.start(interrupted, 80).status == Start::Accepted);
  auto afterDisable = distanceReply(binding, 100);
  binding.setEnabled(false, 91);
  Rs485DeviceStatus status{};
  CHECK(binding.copyStatus(status) && !status.device.enabled && status.validMask == 0);
  CHECK(status.device.health == HealthState::Disabled &&
        status.device.presence == PresenceState::Disabled);

  // processMeasurementWork maps a disabled admission to a direct owner
  // result. It must not replace the module's uncollected cancelled command.
  CHECK(binding.start(probe, 92).status == Start::Disabled);
  std::uint32_t queuedId = 200;
  for (Kind kind : {Kind::Measure, Kind::Probe, Kind::ReadLast}) {
    const DeviceCommand queued{++queuedId, 11, kind, deadline};
    const auto direct = dispatchQueued(binding, queued, 92);
    CHECK(direct.has_value());
    checkTerminal(*direct, queued, Result::Disabled, 92, ErrorCode::Unavailable);
    // Absent is a valid generic result from a reporting module. SHZK signals
    // absence through its presence field on misses; it does not return Absent.
    const auto count = binding.readingCatalog().readingCount;
    const auto absent =
        makeDeviceTerminalResult(queued, count, Result::Absent, {ErrorCode::Unavailable, 0}, 93);
    checkTerminal(absent, queued, Result::Absent, 93, ErrorCode::Unavailable);
  }
  binding.setEnabled(true, 94);
  CHECK(binding.start(probe, 95).status == Start::Busy);
  binding.acceptObservation(afterDisable, 100);
  collect(binding, interrupted, Result::Cancelled, 91, ErrorCode::Unavailable, 101);
  CHECK(binding.copyStatus(status) && status.device.enabled);
  CHECK(status.device.health == HealthState::Initializing &&
        status.device.presence == PresenceState::Unknown);
  CHECK(status.link.totalSuccess == 1 && status.link.totalFailures == 0);
  CHECK(binding.sample(101).usable.freshness == SEN059x::Freshness::NeverSampled);

  const DeviceCommand cached{108, 11, Kind::ReadLast, deadline};
  CHECK(binding.start(cached, 102).status == Start::Accepted);
  collect(binding, cached, Result::Failed, 102, ErrorCode::Rs485NoCachedSnapshot, 103);
}
}  // namespace

int main() {
  constexpr Rs485SerialFormat format{115200, 8, Rs485Parity::None, 1};
  constexpr auto kind = SEN059x::MeasurementKind::RealTimeDistance;
  for (auto model : {SEN059x::DeviceModel::SEN0591, SEN059x::DeviceModel::SEN0592}) {
    FieldCoreMainBinding mainBinding(11, model, kind, format);
    administrative(mainBinding);
    Rod::Binding rodBinding(11, model, kind, format, Rod::Rs485EchoPolicy::QualifiedResponse);
    administrative(rodBinding);
  }
}
