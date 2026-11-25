#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "../../examples/common/ExchangeTiming.h"
#include "FieldCoreRodtesterBinding.h"

namespace {
using namespace FieldCoreMain;
using namespace SEN059x;
namespace Rod = FieldCoreRodtester;
using Reason = Rs485TransportTerminalReason;
using Frame = std::array<std::uint8_t, 8>;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

// Synthetic 0x0206 cases from OPERATIONS.csv, encoded independently of the builders.
constexpr Frame READ{1, 3, 2, 6, 0, 1, 0x65, 0xB3};
constexpr Frame WRITE{1, 6, 2, 6, 2, 0xEE, 0xE9, 0x5F};
constexpr std::uint8_t ORIGINAL[]{1, 3, 2, 0, 100, 0xB9, 0xAF};
constexpr std::uint8_t REQUESTED[]{1, 3, 2, 2, 0xEE, 0x39, 0x68};
constexpr auto THRESHOLD = Setting::SwitchThreshold;
constexpr Rs485SerialFormat FORMAT{9600, 8, Rs485Parity::None, 1};

enum class Script { Read, Echo, FailedDrain };
struct Evidence {
  Rs485BusObservation wire;
  bool drainEstablished;
  bool attributableRead;
};

struct MainBus {
  Owner owner;
  bool available = true;
  unsigned transfers = 0;
  Evidence exchange(const Rs485DeviceStatus& status, const Frame& request, Script script,
                    const std::uint8_t* reply, std::uint16_t length, std::uint64_t now) {
    Rs485ModuleStep step;
    step.action = Rs485ModuleAction::RequestTransaction;
    auto& tx = step.transaction;
    tx.format = FORMAT;
    tx.format.baud = status.baud;
    tx.rxCapacity = kRs485TransactionRxCapacity;
    tx.responseTimeoutMs = status.responseTimeoutMs;
    tx.expectedLength = script == Script::Read ? 7 : 8;
    std::memcpy(tx.txBytes, request.data(), request.size());
    tx.txLength = request.size();
    CHECK(request[0] == status.address && rs485BusTransactionValid(tx));
    ++transfers;
    Rs485BusObservation obs;
    if (script == Script::FailedDrain) {
      // Main's processWriteAndFlush reports the full accepted count on a failed flush.
      // The receive phase of Owner is never entered.
      obs = {Reason::BackendError,
             {ErrorCode::Unavailable, -17},
             nullptr,
             0,
             tx.txLength,
             0,
             now + 10};
      available = false;  // finishTransport marks a backend reporting Unavailable unready.
      return {obs, false, false};
    }
    const auto drainedMs = now + 11;  // 2 ms pre-TX guard and eight 8N1 bytes at 9600 baud.
    CHECK(owner.dispatch(step, drainedMs, now + 300));
    if (script == Script::Echo) {
      CHECK(!owner.receive(reply, length, now + 30, obs));
      CHECK(owner.receive(nullptr, 0, drainedMs + tx.responseTimeoutMs, obs));
      CHECK(obs.reason == Reason::ResponseTimeout && obs.rxLength == 0);
    } else {
      CHECK(owner.receive(reply, length, now + 30, obs));
    }
    // Entry to WaitResponse follows a successful flush on this owner. FC03
    // has a distinguishable reply; main supplies no on-wire closure bounds.
    return {obs, true, script == Script::Read && obs.reason == Reason::Complete};
  }
};

struct RodBus {
  bool available = true;
  unsigned transfers = 0;
  Evidence exchange(const Rs485DeviceStatus& status, const Frame& request, Script script,
                    const std::uint8_t* reply, std::uint16_t length, std::uint64_t now) {
    Rod::Rs485ModuleStep step;
    step.action = Rs485ModuleAction::RequestTransaction;
    auto& tx = step.transaction;
    tx.format = FORMAT;
    tx.format.baud = status.baud;
    tx.responseTimeoutMs = status.responseTimeoutMs;
    tx.eligibleUs = now * 1000;
    tx.deadlineUs = (now + 300) * 1000;
    std::uint32_t interCharacterUs{};
    CHECK(SEN059x::example::rtuTiming(tx.format.baud, interCharacterUs, tx.frameGapUs));
    tx.echoPolicy = script == Script::Read ? Rod::Rs485EchoPolicy::QualifiedResponse
                                           : Rod::Rs485EchoPolicy::Ambiguous;
    std::memcpy(tx.txBytes, request.data(), request.size());
    tx.txLength = request.size();
    CHECK(request[0] == status.address && status.baud == 9600);
    ++transfers;
    tx.identity = {transfers, status.device.deviceId, transfers, status.address, 0};
    Rod::Delivery received{};
    // finishTransport may collect a buffered prefix even after a failed drain.
    received.bytes = script == Script::FailedDrain ? WRITE.data() : reply;
    received.length = script == Script::FailedDrain ? WRITE.size() : length;
    received.totalBytes = received.length;
    received.txAccepted = tx.txLength;
    received.drained = script != Script::FailedDrain;
    if (!received.drained) {
      received.drainError = {ErrorCode::Unavailable, -17};  // Inject a distinct backend cause.
    }
    received.observedUs = (now + 30) * 1000;
    if (received.drained) {
      received.txFinishedUs = (now + 11) * 1000;
      received.timingQualified = true;
      received.earliestUs = received.latestUs = (now + 27) * 1000;
    }
    const auto obs = Rod::observe(step, received);
    available = obs.busAvailable;
    if (script == Script::Echo) {
      CHECK(obs.reason == Reason::Complete && obs.timingQualified && !obs.responseConfirmed);
      CHECK(obs.rxLength == WRITE.size() &&
            std::memcmp(obs.rxBytes, WRITE.data(), WRITE.size()) == 0);
    } else if (script == Script::FailedDrain) {
      CHECK(obs.wireErrors == (1U << 3) && !obs.busAvailable);
      CHECK(obs.settlementError.code == ErrorCode::Unavailable &&
            obs.settlementError.backendCode == -17);
      CHECK(obs.error.code == ErrorCode::Unavailable && obs.error.backendCode == -17);
      CHECK(obs.reason == Reason::BackendError && !obs.txDrainComplete && !obs.txComplete);
      CHECK(!obs.responseConfirmed && obs.rxLength == WRITE.size() &&
            std::memcmp(obs.rxBytes, WRITE.data(), WRITE.size()) == 0);
    }
    return {obs, obs.txComplete, Rod::qualifiedResponse(tx, obs)};
  }
};

// This host maintenance caller borrows a standard binding's admitted endpoint.
// DeviceRequestKind remains the pinned three-command measurement contract.
template <typename Binding, typename Bus>
bool maintenance(Binding& binding, Bus& bus, DeviceModel model, Rs485InstanceRuntimeConfig runtime,
                 Script failure) {
  ErrorDetail error;
  Rs485DeviceStatus product;
  if (!binding.bind(error) || !binding.copyStatus(product) || !product.device.enabled ||
      binding.poll(0) != DeviceModulePollState::Idle || !bus.available ||
      !binding.applyRuntimeConfig(runtime, 0, error)) {
    return false;
  }
  CHECK(binding.copyStatus(product));
  const Endpoint endpoint{product.address, product.baud};
  Configuration configuration;
  CHECK(configuration.bind(model, endpoint));
  const auto read = [&](const std::uint8_t (&reply)[7], std::uint64_t now) {
    if (!bus.available) {
      return false;
    }
    ConfigurationToken token;
    Frame request{};
    CHECK(configuration.buildRead(endpoint, THRESHOLD, request.data(), request.size(), token) == 8);
    CHECK(request == READ);
    const auto result = bus.exchange(product, request, Script::Read, reply, sizeof(reply), now);
    CHECK(result.attributableRead);
    CHECK(configuration.completeRead(token, endpoint, result.wire.rxBytes, result.wire.rxLength,
                                     static_cast<std::uint32_t>(result.wire.completedUptimeMs)));
    return true;
  };
  const auto history = [&](std::uint16_t word, bool trusted, std::uint32_t time) {
    const auto state = configuration.snapshot(THRESHOLD);
    CHECK(state.trusted == trusted && state.observed);
    CHECK(state.observed->value.model == model && state.observed->value.setting == THRESHOLD);
    CHECK(state.observed->endpoint.address == endpoint.address &&
          state.observed->endpoint.bps == endpoint.bps);
    CHECK(state.observed->value.known && state.observed->value.rawWord == word &&
          state.observed->completedMs == time);
  };
  CHECK(read(ORIGINAL, 10));
  history(100, true, 40);
  ConfigurationToken write;
  Frame request{};
  CHECK(configuration.buildWrite(THRESHOLD, {SettingUnit::Millimetres, 750}, WriteScope::Addressed,
                                 request.data(), request.size(), write) == 8);
  CHECK(request == WRITE && bus.transfers == 1);  // Preview caused zero transfers.
  const bool echo = failure == Script::Echo;
  Evidence evidence{};
  const auto submit = [&](bool maintenanceConfirmed) {
    if (!maintenanceConfirmed || !bus.available) {
      return false;
    }
    CHECK(configuration.recordTx(write, TxProgress::Possible, Ok()));
    history(100, false, 40);
    evidence = bus.exchange(product, request, failure, echo ? WRITE.data() : nullptr,
                            echo ? WRITE.size() : 0, 60);
    return true;
  };
  CHECK(!submit(false) && bus.transfers == 1);
  history(100, true, 40);
  CHECK(submit(true));
  const auto& obs = evidence.wire;
  CHECK(obs.txCount == WRITE.size() && !evidence.attributableRead);
  const auto txProgress = evidence.drainEstablished ? TxProgress::Complete : TxProgress::Possible;
  const Status send =
      evidence.drainEstablished ? Ok() : Status{Err::IO, obs.error.backendCode, "Flush failed"};
  CHECK(configuration.recordTx(write, txProgress, send).code == send.code);
  Status uncertain = send;
  if (evidence.drainEstablished && obs.reason == Reason::Complete && obs.rxLength) {
    CHECK(configuration.recordAck(write, obs.rxBytes, obs.rxLength, AckOrigin::Unknown));
    uncertain = {Err::NO_DATA, 0, "Unattributed write acknowledgement"};
  } else {
    if (obs.reason == Reason::ResponseTimeout) {
      uncertain = {Err::TIMEOUT, 0, "No write acknowledgement"};
    }
    CHECK(configuration.missAck(write, uncertain).code == uncertain.code);
  }
  CHECK(configuration.finishWrite(write, uncertain).code == uncertain.code);
  const auto checkWrite = [&] {
    constexpr bool rod = std::is_same_v<Bus, RodBus>;
    const auto expectedPrimary = echo ? (rod ? Err::NO_DATA : Err::TIMEOUT) : Err::IO;
    const auto& op = configuration.operation();
    CHECK(op.setting == THRESHOLD && op.desired.word == 750 && op.scope == WriteScope::Addressed);
    CHECK(op.before.trusted && op.before.observed && op.before.observed->value.rawWord == 100);
    CHECK(op.tx == (echo ? TxProgress::Complete : TxProgress::Possible));
    CHECK(op.txStatus && op.txStatus->code == (echo ? Err::OK : Err::IO));
    CHECK(op.ackOrigin == AckOrigin::Unknown && op.ackStatus);
    CHECK(op.ackStatus->code == (echo && rod ? Err::OK : expectedPrimary));
    CHECK(op.primary && op.primary->code == expectedPrimary);
    if (!echo) {
      CHECK(op.txStatus->detail == -17 && op.ackStatus->detail == -17 && op.primary->detail == -17);
    }
    CHECK(!op.verified && !op.readback);
  };
  checkWrite();
  history(100, false, 40);
  CHECK(bus.transfers == 2);

  if (!echo) {
    CHECK(!bus.available && !read(ORIGINAL, 200) && bus.transfers == 2);
    history(100, false, 40);
    bus.available = true;  // Scripted owner reports successful bus recovery.
  }
  CHECK(read(echo ? REQUESTED : ORIGINAL, 400));
  history(echo ? 750 : 100, true, 430);
  checkWrite();
  CHECK(bus.transfers == 3);
  return true;
}

template <typename Binding, typename Bus>
void exercise(Binding& binding, Bus& bus, DeviceModel model, Script failure) {
  for (std::uint8_t address : {248, 255}) {
    CHECK(!maintenance(binding, bus, model, {address, 9600, 100, 0}, failure));
  }
  CHECK(!maintenance(binding, bus, model, {1, 4800, 100, 0}, failure));
  const Rs485InstanceRuntimeConfig admitted{1, 9600, 100, 0};
  binding.setEnabled(false, 0);
  CHECK(!maintenance(binding, bus, model, admitted, failure));
  binding.setEnabled(true, 0);
  const DeviceCommand active{9, 11, DeviceRequestKind::Measure, {true, 300}};
  CHECK(binding.start(active, 0).status == DeviceModuleStartStatus::Accepted);
  CHECK(!maintenance(binding, bus, model, admitted, failure));
  binding.cancel(0);
  CHECK(!maintenance(binding, bus, model, admitted, failure));
  DeviceMeasurementResult terminal;
  CHECK(binding.takeResult(terminal) && terminal.status == DeviceResultStatus::Cancelled);
  CHECK(bus.transfers == 0);
  CHECK(maintenance(binding, bus, model, admitted, failure));
}
}  // namespace

int main() {
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    for (auto failure : {Script::Echo, Script::FailedDrain}) {
      constexpr auto kind = MeasurementKind::RealTimeDistance;
      FieldCoreMainBinding main(11, model, kind, FORMAT);
      MainBus mainBus;
      exercise(main, mainBus, model, failure);
      Rod::Binding rod(11, model, kind, FORMAT, Rod::Rs485EchoPolicy::QualifiedResponse);
      RodBus rodBus;
      exercise(rod, rodBus, model, failure);
    }
  }
}
