#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FieldCoreRodtesterBinding.h"

namespace {

namespace Product = FieldCoreMain;
namespace Rod = FieldCoreRodtester;
using Policy = Rod::Rs485EchoPolicy;
using Result = Product::DeviceResultStatus;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

// SEN0591 example 2, also used by the main fixed-length prefix test.
constexpr std::array<std::uint8_t, 8> REQUEST{1, 3, 1, 1, 0, 1, 0xD4, 0x36};
constexpr std::array<std::uint8_t, 7> PREFIX{1, 3, 2, 2, 0xEF, 0xF8, 0xA8};

Rod::Rs485BusObservation runMeasure(Policy policy, const Rod::Delivery& delivery, Result expected,
                                    std::uint64_t deadlineMs = 100) {
  Rod::Binding binding(11, SEN059x::DeviceModel::SEN0591,
                       SEN059x::MeasurementKind::RealTimeDistance,
                       {115200, 8, Product::Rs485Parity::None, 1}, policy);
  Product::ErrorDetail error;
  CHECK(binding.bind(error));
  CHECK(binding.applyRuntimeConfig({1, 115200, 100, 0}, 0, error));
  const Product::DeviceCommand cmd{31, 11, Product::DeviceRequestKind::Measure, {true, deadlineMs}};
  CHECK(binding.start(cmd, 10).status == Product::DeviceModuleStartStatus::Accepted);
  Rod::Rs485ModuleStep step;
  CHECK(binding.takeStep(step, 10));
  const auto& tx = step.transaction;
  static_assert(sizeof(tx.txBytes) == 21);
  CHECK(tx.rxCapacity == 192 && tx.txLength == REQUEST.size());
  CHECK(std::memcmp(tx.txBytes, REQUEST.data(), REQUEST.size()) == 0);
  CHECK(tx.completionMode == Rod::Rs485CompletionMode::Rtu && tx.echoPolicy == policy);
  CHECK(tx.identity.operationId == cmd.requestId && tx.identity.targetId == cmd.deviceId &&
        tx.identity.address == 1);

  auto obs = Rod::observe(step, delivery);
  CHECK(obs.identity.operationId == tx.identity.operationId &&
        obs.identity.targetId == tx.identity.targetId &&
        obs.identity.generation == tx.identity.generation &&
        obs.identity.address == tx.identity.address && obs.identity.step == tx.identity.step);
  const auto now = delivery.observedUs / 1000;
  // The wrong generation has no claim on these bytes, even with identical address and FC.
  auto old = obs;
  --old.identity.generation;
  binding.acceptObservation(old, now);
  Product::DeviceMeasurementResult result;
  CHECK(!binding.takeResult(result));
  binding.acceptObservation(obs, now);
  CHECK(binding.poll(now) == Product::DeviceModulePollState::Complete);
  CHECK(binding.takeResult(result));
  CHECK(result.requestId == cmd.requestId && result.deviceId == cmd.deviceId &&
        result.kind == cmd.kind);
  CHECK(result.deadline.active && result.deadline.expiresAtMs == cmd.deadline.expiresAtMs);
  CHECK(result.status == expected && result.completedUptimeMs == now);
  CHECK(result.readingCount == 1 && result.staleMask == 0);
  Product::Rs485DeviceStatus status;
  CHECK(binding.copyStatus(status));
  CHECK(status.consecutiveMissCount == 0);  // Every case supplied at least one received byte.
  const auto sample = binding.sample(now);
  if (expected == Result::Ok) {
    CHECK(result.validMask == 1 && result.errorMask == 0 && result.values[0] == 751);
    CHECK(sample.usable.measurement.value == 751 &&
          sample.usable.measurement.unit == SEN059x::MeasurementUnit::Millimetres);
  } else {
    CHECK(result.validMask == 0 && result.errorMask == 1);
    CHECK(sample.raw.freshness == SEN059x::Freshness::NeverSampled);
    if (obs.reason == Product::Rs485TransportTerminalReason::BackendError) {
      CHECK(obs.error.code != Product::ErrorCode::Ok && result.error.code == obs.error.code);
    }
  }
  binding.acceptObservation(obs, now);
  CHECK(!binding.takeResult(result) && binding.poll(now) == Product::DeviceModulePollState::Idle);
  return obs;
}

}  // namespace

int main() {
  Rod::Delivery buffered{};
  buffered.bytes = PREFIX.data();
  buffered.length = PREFIX.size();
  buffered.totalBytes = PREFIX.size();
  buffered.txAccepted = REQUEST.size();
  buffered.drained = true;
  buffered.txFinishedUs = 13000;  // Includes the 2 ms guard and eight 8N1 bytes at 115200 baud.
  buffered.observedUs = 25000;
  runMeasure(Policy::QualifiedResponse, buffered, Result::Failed);

  auto closed = buffered;
  closed.timingQualified = true;
  closed.earliestUs = 19000;
  closed.latestUs = 19900;
  runMeasure(Policy::Ambiguous, closed, Result::Failed);
  runMeasure(Policy::QualifiedResponse, closed, Result::Ok);

  // Delivery is late, but the scripted wire closure met the original deadline.
  auto delayed = closed;
  delayed.observedUs = 120000;
  runMeasure(Policy::QualifiedResponse, delayed, Result::Ok);
  delayed.latestUs = 100001;
  runMeasure(Policy::QualifiedResponse, delayed, Result::Timeout);
  // A longer command deadline leaves the response deadline at 113 ms.
  delayed.latestUs = 113000;
  runMeasure(Policy::QualifiedResponse, delayed, Result::Ok, 200);
  ++delayed.latestUs;
  runMeasure(Policy::QualifiedResponse, delayed, Result::Timeout, 200);

  auto partialTx = closed;
  --partialTx.txAccepted;
  const auto partial = runMeasure(Policy::QualifiedResponse, partialTx, Result::Failed);
  CHECK(partial.txDrainComplete && !partial.txComplete && partial.timingQualified);
  CHECK(partial.closureEarliestUs == closed.earliestUs &&
        partial.closureLatestUs == closed.latestUs);
  CHECK(!partial.responseConfirmed && partial.wireErrors == 0 && partial.busAvailable);
  auto overreported = closed;
  ++overreported.txAccepted;
  const auto excess = runMeasure(Policy::QualifiedResponse, overreported, Result::Failed);
  CHECK(excess.txDrainComplete && !excess.txComplete && excess.wireErrors == (1U << 3));
  CHECK(!excess.timingQualified && !excess.responseConfirmed);

  // An unclosed RTU prefix at response timeout is evidence of bytes on the bus.
  auto partialRx = buffered;
  partialRx.length = 2;
  partialRx.totalBytes = 2;
  partialRx.observedUs = 113000;
  runMeasure(Policy::QualifiedResponse, partialRx, Result::Timeout, 200);

  auto lostByte = closed;
  ++lostByte.totalBytes;
  const auto truncated = runMeasure(Policy::QualifiedResponse, lostByte, Result::Failed);
  CHECK(truncated.rxTotalCount == 8 && truncated.rxLength == 7 &&
        truncated.wireErrors == (1U << 4) &&
        truncated.error.code == Product::ErrorCode::Rs485BadFrame);
  auto parityError = closed;
  parityError.wireErrors = 1U << 1;  // Rs485WireParity
  const auto parity = runMeasure(Policy::QualifiedResponse, parityError, Result::Failed);
  CHECK(parity.wireErrors == parityError.wireErrors &&
        parity.error.code == Product::ErrorCode::Rs485BadFrame);
  auto overlapsTx = closed;
  overlapsTx.earliestUs = overlapsTx.txFinishedUs - 1;
  runMeasure(Policy::QualifiedResponse, overlapsTx, Result::Failed);

  constexpr std::array<std::uint8_t, 9> TRAILING{1, 3, 2, 2, 0xEF, 0xF8, 0xA8, 0, 0};
  auto extra = closed;
  extra.bytes = TRAILING.data();
  extra.length = TRAILING.size();
  extra.totalBytes = TRAILING.size();
  runMeasure(Policy::QualifiedResponse, extra, Result::Failed);
  return EXIT_SUCCESS;
}
