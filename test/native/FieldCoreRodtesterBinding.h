#pragma once

#include "FieldCoreMainBinding.h"

// Host subset of the RTU step and delivered observation at FieldCore c40ea83.
namespace FieldCoreRodtester {

enum class Rs485CompletionMode : std::uint8_t { Rtu = 2 };
enum class Rs485EchoPolicy : std::uint8_t { QualifiedResponse = 1, Ambiguous = 2 };

struct Rs485StepIdentity {
  std::uint32_t operationId{}, targetId{}, generation{};
  std::uint8_t address{}, step{};
};

struct Rs485BusTransaction {
  FieldCoreMain::Rs485SerialFormat format{};
  std::uint8_t txBytes[21]{};
  std::uint16_t txLength{}, rxCapacity{192};
  std::uint32_t responseTimeoutMs{};
  Rs485CompletionMode completionMode{Rs485CompletionMode::Rtu};
  Rs485EchoPolicy echoPolicy{Rs485EchoPolicy::Ambiguous};
  std::uint64_t eligibleUs{}, deadlineUs{};
  std::uint32_t frameGapUs{};
  Rs485StepIdentity identity{};
};

struct Rs485ModuleStep {
  FieldCoreMain::Rs485ModuleAction action{FieldCoreMain::Rs485ModuleAction::None};
  Rs485BusTransaction transaction{};
  std::uint64_t wakeUptimeMs{};
};

// One scripted, closed receive batch on a common monotonic clock. A buffered
// backend can report foreground silence and drain, but supplies no wire bounds.
// bytes borrows length accessible bytes through the synchronous callback.
struct Delivery {
  const std::uint8_t* bytes{};
  std::uint16_t length{}, txAccepted{};
  std::uint32_t totalBytes{}, wireErrors{};
  bool drained{};
  std::uint64_t txFinishedUs{}, observedUs{};
  bool timingQualified{};
  std::uint64_t earliestUs{}, latestUs{};
};

struct Rs485BusObservation : FieldCoreMain::Rs485BusObservation {
  Rs485StepIdentity identity{};
  std::uint64_t observedUs{}, closureEarliestUs{}, closureLatestUs{};
  std::uint32_t rxTotalCount{}, wireErrors{};
  bool txDrainComplete{}, txComplete{}, timingQualified{}, responseConfirmed{};
};

Rs485BusObservation observe(const Rs485ModuleStep& step, const Delivery& delivery);

class Binding : private FieldCoreMain::FieldCoreMainBinding {
 public:
  Binding(std::uint16_t deviceId, SEN059x::DeviceModel model, SEN059x::MeasurementKind kind,
          FieldCoreMain::Rs485SerialFormat format, Rs485EchoPolicy policy)
      : FieldCoreMainBinding(deviceId, model, kind, format), policy_(policy) {}
  using FieldCoreMainBinding::applyRuntimeConfig;
  using FieldCoreMainBinding::bind;
  using FieldCoreMainBinding::cancel;
  using FieldCoreMainBinding::copyStatus;
  using FieldCoreMainBinding::onBusInvalidated;
  using FieldCoreMainBinding::poll;
  using FieldCoreMainBinding::sample;
  using FieldCoreMainBinding::setEnabled;
  using FieldCoreMainBinding::start;
  using FieldCoreMainBinding::takeResult;
  bool takeStep(Rs485ModuleStep& step, std::uint64_t nowMs);
  void acceptObservation(const Rs485BusObservation& observation, std::uint64_t nowMs);

 private:
  Rs485EchoPolicy policy_;
  Rs485BusTransaction pending_{};
  std::uint32_t generation_{};
};

}  // namespace FieldCoreRodtester
