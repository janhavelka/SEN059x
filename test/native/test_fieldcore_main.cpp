#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FieldCoreMainBinding.h"

void testFieldCoreMainOwner();

namespace {
using namespace FieldCoreMain;
using SEN059x::DeviceModel;
using SEN059x::DriverState;
using SEN059x::Freshness;
using SEN059x::MeasurementKind;
using SEN059x::MeasurementUnit;
using Start = DeviceModuleStartStatus;
using Poll = DeviceModulePollState;
using Result = DeviceResultStatus;
using Kind = DeviceRequestKind;
using Reason = Rs485TransportTerminalReason;
using Parser = Rs485ParserError;
using Binding = FieldCoreMainBinding;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

// DFRobot examples 5 and 2. Peer temperature uses a synthetic 30.5 C word.
using Request = std::array<uint8_t, 8>;
constexpr Request BAUD_TX{1, 3, 2, 1, 0, 1, 0xD4, 0x72};
constexpr Request DISTANCE_TX{1, 3, 1, 1, 0, 1, 0xD4, 0x36};
constexpr Request PEER_TEMP_TX{2, 3, 1, 2, 0, 1, 0x24, 0x05};
constexpr uint8_t BAUD_RX[]{1, 3, 2, 0, 3, 0xF8, 0x45};
constexpr uint8_t DISTANCE_RX[]{1, 3, 2, 2, 0xEF, 0xF8, 0xA8};
constexpr uint8_t PEER_TEMP_RX[]{2, 3, 2, 1, 0x31, 0x3C, 0x00};
constexpr Rs485SerialFormat FACTORY{115200, 8, Rs485Parity::None, 1};
constexpr auto REALTIME = MeasurementKind::RealTimeDistance;

DeviceCommand command(uint32_t id, uint16_t device, Kind kind, uint64_t expires = 5000) {
  DeviceCommand cmd{};
  cmd.requestId = id;
  cmd.deviceId = device;
  cmd.kind = kind;
  cmd.deadline = {true, expires};
  return cmd;
}

Rs485DeviceStatus status(const Binding& binding) {
  Rs485DeviceStatus out{};
  CHECK(binding.copyStatus(out));
  return out;
}

void configure(Binding& binding, uint8_t address, uint32_t baud = 115200, uint8_t retries = 0) {
  ErrorDetail error{ErrorCode::InternalFault, 1};
  CHECK(binding.bind(error) && error.code == ErrorCode::Ok && error.backendCode == 0);
  CHECK(binding.applyRuntimeConfig({address, baud, 100, retries}, 0, error));
}

void noStep(Binding& binding) {
  Rs485ModuleStep step;
  CHECK(!binding.takeStep(step));
  CHECK(step.action == Rs485ModuleAction::None);
}

void dispatch(Binding& binding, Owner& owner, const DeviceCommand& cmd, uint64_t now,
              const Request& expected, uint32_t baud = 115200) {
  CHECK(binding.poll(now) == Poll::Running);
  Rs485ModuleStep step;
  CHECK(binding.takeStep(step));
  CHECK(step.action == Rs485ModuleAction::RequestTransaction);
  const auto& tx = step.transaction;
  CHECK(tx.format.baud == baud && rs485SerialFormatValid(tx.format));
  CHECK(tx.txLength == expected.size() &&
        std::memcmp(tx.txBytes, expected.data(), tx.txLength) == 0);
  CHECK(tx.completionMode == Rs485CompletionMode::FixedLength && tx.expectedLength == 7);
  CHECK(tx.rxCapacity == 192 && tx.discardBeforeTransmit && tx.responseTimeoutMs == 100);
  CHECK(owner.dispatch(step, now, cmd.deadline.expiresAtMs));
  noStep(binding);
}

void begin(Binding& binding, Owner& owner, const DeviceCommand& cmd, uint64_t now,
           const Request& expected, uint32_t baud = 115200) {
  CHECK(binding.start(cmd, now).status == Start::Accepted);
  dispatch(binding, owner, cmd, now, expected, baud);
}

void deliver(Binding& binding, Owner& owner, const uint8_t* bytes, size_t length, uint64_t now,
             Reason reason = Reason::Complete) {
  Rs485BusObservation obs;
  CHECK(owner.receive(bytes, length, now, obs));
  CHECK(obs.reason == reason);
  binding.acceptObservation(obs, now);
}

DeviceMeasurementResult collect(Binding& binding, const DeviceCommand& cmd, uint64_t now,
                                Result expected) {
  CHECK(binding.poll(now) == Poll::Complete);
  DeviceMeasurementResult out;
  CHECK(binding.takeResult(out));
  CHECK(out.requestId == cmd.requestId && out.deviceId == cmd.deviceId && out.kind == cmd.kind);
  CHECK(out.deadline.active == cmd.deadline.active &&
        out.deadline.expiresAtMs == cmd.deadline.expiresAtMs);
  CHECK(out.status == expected);
  CHECK(out.readingCount == (cmd.kind == Kind::Probe ? 0 : binding.readingCatalog().readingCount));
  const bool reading = out.readingCount == 1;
  CHECK(out.validMask == (reading && expected == Result::Ok ? 1u : 0u));
  CHECK(out.staleMask == (reading && expected == Result::Stale ? 1u : 0u));
  CHECK(out.errorMask == (reading && (expected == Result::Timeout ||
                                      expected == Result::Cancelled || expected == Result::Failed)
                              ? 1u
                              : 0u));
  DeviceMeasurementResult duplicate;
  CHECK(!binding.takeResult(duplicate));
  CHECK(binding.poll(now) == Poll::Idle);
  return out;
}

void twoBindingsAndPassiveReads() {
  Owner owner;
  Binding distance(11, DeviceModel::SEN0591, REALTIME, FACTORY);
  Binding temperature(22, DeviceModel::SEN0592, MeasurementKind::Temperature, FACTORY);
  configure(distance, 1, 9600);
  configure(temperature, 2);
  CHECK(distance.readingCatalog().readingCount == 1 &&
        temperature.readingCatalog().readingCount == 1);
  CHECK(std::strcmp(distance.readingCatalog().readings[0].unit, "mm") == 0);
  CHECK(std::strcmp(temperature.readingCatalog().readings[0].unit, "degC") == 0);
  CHECK(std::strcmp(healthStateName(HealthState::Unknown), "unknown") == 0);
  CHECK(std::strcmp(healthStateName(distance.snapshot().health), "initializing") == 0);
  CHECK(std::strcmp(presenceStateName(distance.snapshot().presence), "unknown") == 0);
  CHECK(status(distance).link.state == DriverState::UNINIT);

  const auto probe = command(101, 11, Kind::Probe);
  begin(distance, owner, probe, 10, BAUD_TX, 9600);
  deliver(distance, owner, BAUD_RX, sizeof(BAUD_RX), 20);
  CHECK(collect(distance, probe, 20, Result::Ok).parserError == Parser::None);
  const auto probed = status(distance);
  CHECK(probed.lastProbe && probed.lastProbe->rawWord == 3 && probed.lastProbe->known);
  CHECK(distance.sample(20).raw.freshness == Freshness::NeverSampled);
  ErrorDetail error{};
  CHECK(distance.applyRuntimeConfig({1, 115200, 100, 0}, 20, error));

  // Hold one terminal while the second selected SKU uses the same owner.
  const auto first = command(102, 11, Kind::Measure, 0x100000077ULL);
  const auto peer = command(103, 22, Kind::Measure);
  begin(distance, owner, first, 30, DISTANCE_TX);
  deliver(distance, owner, DISTANCE_RX, sizeof(DISTANCE_RX), 40);
  begin(temperature, owner, peer, 50, PEER_TEMP_TX);
  deliver(temperature, owner, PEER_TEMP_RX, sizeof(PEER_TEMP_RX), 65);
  CHECK(distance.start(command(201, 11, Kind::Probe), 66).status == Start::Busy);
  CHECK(collect(distance, first, 70, Result::Ok).values[0] == 751);
  CHECK(collect(temperature, peer, 70, Result::Ok).values[0] == 30.5);
  const auto peerReading = temperature.sample(75).raw;
  CHECK(peerReading.measurement.unit == MeasurementUnit::DegreesCelsius &&
        peerReading.measurement.rawWord == 305);
  CHECK(peerReading.measurement.model == DeviceModel::SEN0592 &&
        peerReading.measurement.address == 2);
  CHECK(peerReading.measurement.completedMs == 65 && peerReading.ageMs == 10);

  const auto cached = command(104, 11, Kind::ReadLast);
  const auto cachedPeer = status(temperature);
  CHECK(cachedPeer.readingCount == 1 && cachedPeer.validMask == 1 && cachedPeer.values[0] == 30.5);
  CHECK(distance.start(cached, 80).status == Start::Accepted);
  noStep(distance);
  CHECK(collect(distance, cached, 80, Result::Ok).values[0] == 751);
  const auto retained = distance.sample(80).usable;
  CHECK(retained.measurement.rawWord == 0x02EF &&
        retained.measurement.unit == MeasurementUnit::Millimetres);
  CHECK(retained.measurement.model == DeviceModel::SEN0591 && retained.measurement.address == 1);
  CHECK(retained.measurement.completedMs == 40 && retained.ageMs == 40);
  const auto aged = command(105, 11, Kind::ReadLast);
  CHECK(distance.start(aged, 1041).status == Start::Accepted);
  collect(distance, aged, 1041, Result::Stale);
  CHECK(distance.sample(1041).usable.freshness == Freshness::Stale);
  CHECK(status(distance).link.state == DriverState::READY);
  CHECK(distance.snapshot().lastSuccessUptimeMs == 40 &&
        temperature.snapshot().lastSuccessUptimeMs == 65);
  CHECK(owner.transfers == 3 && owner.fc03Transfers == 3 && owner.fc06Transfers == 0);
}

void admissionsAndCancellation() {
  Owner owner;
  Binding b(11, DeviceModel::SEN0591, REALTIME, FACTORY);
  const auto cmd = command(91, 11, Kind::Measure, 500);
  CHECK(b.start(cmd, 0).status == Start::Rejected);
  ErrorDetail error{};
  CHECK(b.applyRuntimeConfig({2, 9600, 100, 0}, 0, error));
  CHECK(b.start(cmd, 0).status == Start::Rejected);
  CHECK(b.bind(error));
  const auto configured = status(b);
  CHECK(configured.address == 2 && configured.baud == 9600);
  for (auto address : {0, 248, 254, 255}) {
    CHECK(!b.applyRuntimeConfig({static_cast<uint8_t>(address), 9600, 100, 0}, 1, error));
    CHECK(error.code == ErrorCode::InvalidArgument);
  }
  for (auto baud : {4800u, 14400u, 76800u}) {
    CHECK(!b.applyRuntimeConfig({1, baud, 100, 0}, 1, error));
    CHECK(error.code == ErrorCode::InvalidArgument);
  }
  for (auto format : {Rs485SerialFormat{115200, 7, Rs485Parity::None, 1},
                      Rs485SerialFormat{115200, 8, Rs485Parity::Even, 1},
                      Rs485SerialFormat{115200, 8, Rs485Parity::None, 2}}) {
    Binding invalid(11, DeviceModel::SEN0591, REALTIME, format);
    CHECK(!invalid.bind(error) && error.code == ErrorCode::InvalidArgument);
    CHECK(invalid.start(cmd, 1).status == Start::Rejected);
    noStep(invalid);
  }
  CHECK(b.start(command(0, 11, Kind::Measure), 1).status == Start::Rejected);
  CHECK(b.start(command(1, 12, Kind::Measure), 1).status == Start::Rejected);
  CHECK(b.start(command(1, 11, Kind::Measure, 1), 1).status == Start::Rejected);
  auto inactive = cmd;
  inactive.deadline.active = false;
  CHECK(b.start(inactive, 1).status == Start::Rejected);
  CHECK(b.start(command(1, 11, static_cast<Kind>(255)), 1).status == Start::Unsupported);
  CHECK(b.applyRuntimeConfig({1, 115200, 100, 0}, 1, error));
  CHECK(error.code == ErrorCode::Ok && error.backendCode == 0);
  noStep(b);
  CHECK(b.start(cmd, 2).status == Start::Accepted);
  CHECK(b.start(command(92, 11, Kind::Probe), 3).status == Start::Busy);
  b.setEnabled(false, 4);
  CHECK(b.start(cmd, 5).status == Start::Disabled);
  CHECK(std::strcmp(healthStateName(b.snapshot().health), "disabled") == 0);
  CHECK(std::strcmp(presenceStateName(b.snapshot().presence), "disabled") == 0);
  b.setEnabled(true, 6);
  CHECK(b.start(cmd, 6).status == Start::Busy);
  CHECK(collect(b, cmd, 6, Result::Cancelled).error.code == ErrorCode::Unavailable);
  CHECK(b.snapshot().errorCount == 0);
  CHECK(b.start(cmd, 7).status == Start::Accepted);
  CHECK(b.poll(499) == Poll::Running);
  collect(b, cmd, 500, Result::Timeout);
  CHECK(owner.transfers == 0 && status(b).link.totalFailures == 0);

  const auto live = command(93, 11, Kind::Measure);
  begin(b, owner, live, 510, DISTANCE_TX);
  b.cancel(511, {});
  deliver(b, owner, DISTANCE_RX, sizeof(DISTANCE_RX), 512);
  CHECK(collect(b, live, 513, Result::Cancelled).error.code == ErrorCode::Busy);
  CHECK(b.sample(513).raw.freshness == Freshness::NeverSampled);
  CHECK(status(b).link.totalSuccess == 0);
  const auto invalidated = command(94, 11, Kind::Probe);
  CHECK(b.start(invalidated, 520).status == Start::Accepted);
  b.onBusInvalidated(521, {});
  CHECK(collect(b, invalidated, 522, Result::Failed).error.code == ErrorCode::Unavailable);
  const auto custom = command(95, 11, Kind::Measure);
  CHECK(b.start(custom, 530).status == Start::Accepted);
  b.cancel(531, {ErrorCode::InternalFault, 42});
  const auto cancelled = collect(b, custom, 532, Result::Cancelled);
  CHECK(cancelled.error.code == ErrorCode::InternalFault && cancelled.error.backendCode == 42);
  const auto busFault = command(96, 11, Kind::Measure);
  CHECK(b.start(busFault, 540).status == Start::Accepted);
  b.onBusInvalidated(541, {ErrorCode::InternalFault, 43});
  const auto failed = collect(b, busFault, 542, Result::Failed);
  CHECK(failed.error.code == ErrorCode::InternalFault && failed.error.backendCode == 43);
  CHECK(owner.transfers == 1 && owner.fc06Transfers == 0);
}

void retryYieldsToPeer() {
  Owner owner;
  Binding first(11, DeviceModel::SEN0591, REALTIME, FACTORY);
  Binding peer(22, DeviceModel::SEN0592, MeasurementKind::Temperature, FACTORY);
  configure(first, 1, 115200, 1);
  configure(peer, 2, 38400);
  const auto cmd = command(30, 11, Kind::Measure, 500);
  begin(first, owner, cmd, 0, DISTANCE_TX);
  deliver(first, owner, nullptr, 0, 100, Reason::ResponseTimeout);
  Rs485ModuleStep wait;
  CHECK(first.poll(100) == Poll::Running && first.takeStep(wait));
  CHECK(wait.action == Rs485ModuleAction::WaitUntil && wait.wakeUptimeMs == 175);
  CHECK(!owner.dispatch(wait, 100, 500));
  const auto other = command(31, 22, Kind::Measure, 160);
  begin(peer, owner, other, 110, PEER_TEMP_TX, 38400);
  deliver(peer, owner, PEER_TEMP_RX, sizeof(PEER_TEMP_RX), 145);
  CHECK(first.poll(174) == Poll::Running && first.takeStep(wait));
  CHECK(wait.action == Rs485ModuleAction::WaitUntil);
  dispatch(first, owner, cmd, 175, DISTANCE_TX);
  deliver(first, owner, DISTANCE_RX, sizeof(DISTANCE_RX), 190);
  CHECK(collect(first, cmd, 190, Result::Ok).completedUptimeMs == 190);
  CHECK(collect(peer, other, 190, Result::Ok).completedUptimeMs == 145);
  CHECK(status(first).link.totalFailures == 1 && status(first).link.totalSuccess == 1);
  CHECK(status(peer).link.totalFailures == 0 && peer.sample(190).usable.ageMs == 45);
  CHECK(owner.fc03Transfers == 3 && owner.transfers == 3 && owner.fc06Transfers == 0);
}

void runtimeChangeDuringCommand() {
  Owner owner;
  Binding b(11, DeviceModel::SEN0591, MeasurementKind::Temperature, FACTORY);
  configure(b, 1);
  const auto first = command(10, 11, Kind::Measure);
  CHECK(b.start(first, 1).status == Start::Accepted);
  ErrorDetail error{};
  CHECK(b.applyRuntimeConfig({2, 57600, 100, 1}, 1, error));
  // Main snapshots the command config at start. Runtime diagnostics already
  // report the new config while this exchange still belongs to address 1.
  const auto changed = status(b);
  CHECK(changed.address == 2 && changed.baud == 57600 && changed.responseTimeoutMs == 100 &&
        changed.retries == 1);
  constexpr Request temperatureTx{1, 3, 1, 2, 0, 1, 0x24, 0x36};
  constexpr uint8_t temperatureRx[]{1, 3, 2, 1, 0x2C, 0xB8, 0x09};
  dispatch(b, owner, first, 2, temperatureTx);
  deliver(b, owner, temperatureRx, sizeof(temperatureRx), 12);
  CHECK(collect(b, first, 12, Result::Ok).values[0] == 30);
  CHECK(b.sample(13).usable.freshness == Freshness::NeverSampled);
  const auto second = command(11, 11, Kind::Measure);
  begin(b, owner, second, 15, PEER_TEMP_TX, 57600);
  deliver(b, owner, PEER_TEMP_RX, sizeof(PEER_TEMP_RX), 26);
  CHECK(collect(b, second, 26, Result::Ok).values[0] == 30.5);
  CHECK(b.sample(30).usable.measurement.address == 2 && owner.fc03Transfers == 2);
}

void missesAndRecovery() {
  Owner owner;
  Binding b(11, DeviceModel::SEN0591, REALTIME, FACTORY);
  configure(b, 1);
  uint32_t i = 0;
  for (auto rate : {9600u, 19200u, 38400u, 57600u, 115200u}) {
    ErrorDetail error{};
    CHECK(b.applyRuntimeConfig({1, rate, 100, 0}, 10 * i, error));
    ++i;
    const auto cmd = command(i, 11, Kind::Measure);
    begin(b, owner, cmd, 10 * i, DISTANCE_TX, rate);
    deliver(b, owner, DISTANCE_RX, sizeof(DISTANCE_RX), 10 * i + 1);
    collect(b, cmd, 10 * i + 1, Result::Ok);
  }
  for (uint32_t miss = 1; miss <= 3; ++miss) {
    const auto cmd = command(50 + miss, 11, Kind::Measure);
    const uint64_t now = 200 * miss;
    begin(b, owner, cmd, now, DISTANCE_TX);
    deliver(b, owner, nullptr, 0, now + 100, Reason::ResponseTimeout);
    collect(b, cmd, now + 100, miss == 2 ? Result::Stale : Result::Timeout);
    const auto s = status(b);
    CHECK(s.consecutiveMissCount == miss && s.parserErrorCount == 0);
    CHECK(s.device.lastAttemptUptimeMs == now + 100 && s.device.lastErrorUptimeMs == now + 100);
    CHECK(s.device.lastSuccessUptimeMs == 51 &&
          b.sample(now + 100).usable.measurement.completedMs == 51);
    if (miss >= 2) {
      CHECK(std::strcmp(presenceStateName(s.device.presence), "absent") == 0);
      CHECK(std::strcmp(healthStateName(s.device.health), miss == 2 ? "degraded" : "fault") == 0);
      CHECK(s.staleCount == 1);
    }
  }
  const auto cmd = command(60, 11, Kind::Measure);
  begin(b, owner, cmd, 800, DISTANCE_TX);
  deliver(b, owner, DISTANCE_RX, sizeof(DISTANCE_RX), 820);
  collect(b, cmd, 820, Result::Ok);
  const auto s = status(b);
  CHECK(std::strcmp(presenceStateName(s.device.presence), "present") == 0);
  CHECK(std::strcmp(healthStateName(s.device.health), "ok") == 0);
  CHECK(s.consecutiveMissCount == 0 && s.link.state == DriverState::READY);
  CHECK(s.device.lastErrorUptimeMs == 700 && s.device.lastSuccessUptimeMs == 820);
  CHECK(s.device.errorCount == 3 && s.device.timeoutCount == 3);
  CHECK(owner.fc03Transfers == 9 && owner.transfers == 9 && owner.fc06Transfers == 0);
}

void parserProjection() {
  Owner owner;
  Binding b(11, DeviceModel::SEN0591, REALTIME, FACTORY);
  configure(b, 1);
  const auto seed = command(70, 11, Kind::Measure);
  begin(b, owner, seed, 0, DISTANCE_TX);
  deliver(b, owner, DISTANCE_RX, sizeof(DISTANCE_RX), 10);
  collect(b, seed, 10, Result::Ok);

  struct Case {
    Parser expected;
    size_t size;
    uint8_t bytes[9];
  };
  // Count and length faults carry valid CRCs over the complete supplied buffer.
  const Case cases[]{
      {Parser::BadCrc, 7, {1, 3, 2, 2, 0xEF, 0xF8, 0}},
      {Parser::UnexpectedAddress, 7, {2, 3, 2, 2, 0xEF, 0xBC, 0xA8}},
      {Parser::UnexpectedFunction, 7, {1, 4, 2, 2, 0xEF, 0xF9, 0xDC}},
      {Parser::ValueCountMismatch, 9, {1, 3, 4, 0, 1, 0, 2, 0x2A, 0x32}},
      {Parser::ValueCountMismatch, 8, {1, 3, 2, 2, 0xEF, 0, 0xA9, 0x82}},
      {Parser::BadCrc, 8, {1, 3, 2, 2, 0xEF, 0xF8, 0xA8, 0xFF}},  // Valid prefix plus surplus.
      {Parser::TruncatedFrame, 3, {1, 3, 2}},
      {Parser::ExceptionResponse, 5, {1, 0x83, 2, 0xC0, 0xF1}},
      {Parser::OutOfRangeValue, 7, {1, 3, 2, 0, 0, 0xB8, 0x44}},
  };
  uint32_t n = 0;
  for (const auto& c : cases) {
    const auto cmd = command(71 + n, 11, Kind::Measure);
    const uint64_t now = 200 + n * 200;
    const auto before = status(b);
    begin(b, owner, cmd, now, DISTANCE_TX);
    Rs485BusObservation obs;
    const bool complete = owner.receive(c.bytes, c.size, now + 1, obs);
    if (c.size < 7) {
      CHECK(!complete && b.poll(now + 99) == Poll::Running);
      CHECK(owner.receive(nullptr, 0, now + 100, obs));
      CHECK(obs.reason == Reason::ResponseTimeout);
    } else {
      CHECK(complete && obs.reason == Reason::Complete);
    }
    CHECK(obs.rxLength == c.size && obs.rxCount == c.size);
    b.acceptObservation(obs, obs.completedUptimeMs);
    b.acceptObservation(obs, obs.completedUptimeMs + 1);
    const auto result = collect(b, cmd, obs.completedUptimeMs + 1, Result::Failed);
    CHECK(result.parserError == c.expected && result.completedUptimeMs == obs.completedUptimeMs);
    const auto s = status(b);
    CHECK(s.lastParserError == c.expected && s.consecutiveMissCount == 0);
    CHECK(s.parserErrorCount == ++n && s.device.errorCount == n && s.device.timeoutCount == 0);
    CHECK(s.device.lastAttemptUptimeMs == obs.completedUptimeMs &&
          s.device.lastErrorUptimeMs == obs.completedUptimeMs);
    CHECK(s.device.lastSuccessUptimeMs == 10);
    CHECK(b.sample(now + 110).usable.measurement.completedMs == 10);
    if (c.expected == Parser::ExceptionResponse || c.expected == Parser::OutOfRangeValue) {
      CHECK(s.link.state == DriverState::READY &&
            s.link.totalSuccess == before.link.totalSuccess + 1);
      CHECK(s.device.health == HealthState::Degraded);
    }
    if (c.expected == Parser::ExceptionResponse) {
      CHECK(result.error.code == ErrorCode::Rs485ParserError);
      CHECK(result.error.backendCode == 2 && b.sample(now + 110).raw.measurement.completedMs == 10);
    }
    if (c.expected == Parser::OutOfRangeValue) {
      CHECK(b.sample(now + 110).raw.measurement.rawWord == 0);
      CHECK(b.sample(now + 110).raw.freshness == Freshness::Unusable);
    }
  }
  // Exercise a broken callback separately from byte sequences available on main.
  const auto invalid = command(99, 11, Kind::Probe);
  begin(b, owner, invalid, 2200, BAUD_TX);
  Rs485BusObservation obs;
  CHECK(owner.receive(BAUD_RX, sizeof(BAUD_RX), 2210, obs));
  obs.rxBytes = nullptr;
  b.acceptObservation(obs, 2210);
  CHECK(collect(b, invalid, 2210, Result::Failed).parserError == Parser::UnsupportedFrame);
  CHECK(owner.transfers == 11 && owner.fc03Transfers == 11 && owner.fc06Transfers == 0);
}
}  // namespace

int main() {
  testFieldCoreMainOwner();
  twoBindingsAndPassiveReads();
  admissionsAndCancellation();
  retryYieldsToPeer();
  runtimeChangeDuringCommand();
  missesAndRecovery();
  parserProjection();
}
