#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>

#include <SEN059x/SEN059x.h>

namespace {
using namespace SEN059x;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

constexpr auto SHORT_MODEL = DeviceModel::SEN0591;
constexpr auto LONG_MODEL = DeviceModel::SEN0592;
constexpr auto DISTANCE = MeasurementKind::ProcessedDistance;
constexpr Endpoint ENDPOINT{1, 115200};
constexpr Status TIMEOUT{Err::TIMEOUT, -19, "response deadline"};
using Frame = std::array<std::uint8_t, 8>;
using Reply = std::array<std::uint8_t, 7>;
// DFRobot examples 1 and 5, plus synthetic words and an addressed FC03 exception.
constexpr Reply DISTANCE_754{1, 3, 2, 2, 0xF2, 0x38, 0xA1};
constexpr Reply BAUD_9600{1, 3, 2, 0, 3, 0xF8, 0x45};
constexpr Reply DISTANCE_3001{1, 3, 2, 0x0B, 0xB9, 0x7E, 0xC6};
constexpr Reply FOREIGN{2, 3, 2, 0x0B, 0xB9, 0x3A, 0xC6};
constexpr std::array<std::uint8_t, 5> EXCEPTION{1, 0x83, 0xE1, 0x81, 0x78};

void checkEmpty(const DeviceHealth& device) {
  const auto health = device.healthSnapshot();
  CHECK(device.state() == DriverState::UNINIT && health.state == DriverState::UNINIT);
  CHECK(!device.isOnline() && !health.initialized && health.consecutiveFailures == 0);
  CHECK(health.totalFailures == 0 && health.totalSuccess == 0 && health.lastError.ok());
  CHECK(!health.lastErrorTimeValid && !health.lastOkTimeValid);
}

HealthAttempt start(DeviceHealth& device) {
  HealthAttempt attempt;
  CHECK(device.beginAttempt(attempt));
  return attempt;
}

void checkTransitions() {
  DeviceHealth device, peer;
  HealthAttempt empty;
  CHECK(!device.beginAttempt(empty));
  CHECK(!device.bind(SHORT_MODEL, 1, 0));
  CHECK(device.bind(SHORT_MODEL, 1) && peer.bind(LONG_MODEL, 2, 1));
  checkEmpty(device);
  const auto peerAttempt = start(peer);
  CHECK(!device.recordFailure(peerAttempt, TIMEOUT, 0, true));
  for (unsigned i = 1; i <= 5; ++i) {
    const auto attempt = start(device);
    CHECK(!device.beginAttempt(empty));  // The first exchange is still outstanding.
    CHECK(!device.recordReply(peerAttempt, 0, true));
    CHECK(device.recordFailure(attempt, TIMEOUT, i - 1, true));
    CHECK(!device.recordFailure(attempt, TIMEOUT, 50, true));
    CHECK(!device.recordReply(attempt, 51, true));
    const auto health = device.healthSnapshot();
    CHECK(health.state == (i < 5 ? DriverState::DEGRADED : DriverState::OFFLINE));
    CHECK(!health.initialized && !device.isOnline() && !health.lastOkTimeValid);
    CHECK(health.lastErrorTimeValid && health.lastErrorMs == i - 1);
    CHECK(health.totalFailures == i && health.consecutiveFailures == i && health.totalSuccess == 0);
  }
  checkEmpty(peer);
  CHECK(peer.recordFailure(peerAttempt, TIMEOUT, 0, true));
  CHECK(peer.state() == DriverState::OFFLINE && !peer.isOnline());
  const auto recovery = start(device);
  CHECK(!device.bind(SHORT_MODEL, 1, 0));  // Rejected settings preserve the outstanding token.
  for (auto code : {Err::OK, Err::DISALLOWED, Err::ABORTED, Err::LINK_LOST, Err::EXCEPTION}) {
    CHECK(!device.recordFailure(recovery, {code, 0, "not a link failure"}, 10, true));
  }
  constexpr auto LAST_TICK = std::numeric_limits<std::uint32_t>::max();
  CHECK(device.recordReply(recovery, LAST_TICK, true));
  auto health = device.healthSnapshot();
  CHECK(health.initialized && device.isOnline() && health.state == DriverState::READY);
  CHECK(health.totalSuccess == 1 && health.totalFailures == 5 && health.consecutiveFailures == 0);
  CHECK(health.lastOkTimeValid && health.lastOkMs == LAST_TICK);
  CHECK(health.lastErrorTimeValid && health.lastErrorMs == 4 && health.lastError.detail == -19);
  CHECK(device.recordFailure(start(device), TIMEOUT, 0, true));
  CHECK(device.state() == DriverState::DEGRADED && device.isOnline());
  const auto pending = start(device);
  device.resetStatistics();
  health = device.healthSnapshot();
  CHECK(health.totalFailures == 0 && health.totalSuccess == 0 && health.consecutiveFailures == 1);
  CHECK(health.state == DriverState::DEGRADED && health.initialized && device.isOnline());
  CHECK(health.lastOkTimeValid && health.lastOkMs == LAST_TICK);
  CHECK(health.lastErrorTimeValid && health.lastErrorMs == 0 &&
        health.lastError.code == Err::TIMEOUT);
  CHECK(health.lastError.detail == -19 && std::strcmp(health.lastError.msg, TIMEOUT.msg) == 0);

  CHECK(device.recordFailure(pending, TIMEOUT, 0, false));
  for (unsigned i = 1; i < 260; ++i) {
    CHECK(device.recordFailure(start(device), TIMEOUT, i, false));
  }
  health = device.healthSnapshot();
  CHECK(health.initialized && !device.isOnline() && health.state == DriverState::OFFLINE);
  CHECK(health.totalFailures == 260 && health.consecutiveFailures == 255);
  CHECK(!health.lastErrorTimeValid && health.lastOkTimeValid && health.lastOkMs == LAST_TICK);
  CHECK(device.recordReply(start(device), 0, true));
  health = device.healthSnapshot();
  CHECK(device.isOnline() && health.consecutiveFailures == 0 && health.lastOkMs == 0);
  CHECK(health.lastOkTimeValid && !health.lastErrorTimeValid && health.lastError.detail == -19);
  CHECK(device.recordFailure(start(device), TIMEOUT, 42, true));
  CHECK(device.recordReply(start(device), 0, false));
  health = device.healthSnapshot();
  CHECK(!health.lastOkTimeValid && health.lastErrorTimeValid && health.lastErrorMs == 42);
  CHECK(peer.healthSnapshot().totalFailures == 1 && peer.healthSnapshot().totalSuccess == 0);
  CHECK(peer.recordFailure(start(peer), {Err::IO, -7, "RX failed"}, 8, true));
  CHECK(peer.healthSnapshot().totalFailures == 2 &&
        peer.healthSnapshot().lastError.code == Err::IO);
  CHECK(peer.healthSnapshot().lastError.detail == -7 && peer.healthSnapshot().lastErrorMs == 8);
}

// One scripted bus owner for address 1. Only send() represents a transfer. Closed foreign frames
// are ignored before passing a complete, current addressed attempt to the real decoder.
struct Script {
  DeviceHealth health;
  MeasurementCache cache;
  Configuration config;
  Stats stats;
  SampleRequest sample;
  HealthAttempt attempt;
  Frame tx{};
  unsigned transfers = 0;

  void bind(DeviceModel model) {
    CHECK(health.bind(model, ENDPOINT.address));
    CHECK(cache.bind(model, ENDPOINT.address));
    CHECK(config.bind(model, ENDPOINT));
  }

  void send() {
    CHECK(tx[0] == ENDPOINT.address);
    ++transfers;
    CHECK(health.beginAttempt(attempt));
  }

  void sendRead() {
    CHECK(cache.buildRead(DISTANCE, tx.data(), tx.size(), sample) == tx.size());
    send();
  }

  template <std::size_t N>
  std::optional<Status> receive(const std::array<std::uint8_t, N>& rx, std::uint32_t ms) {
    if (rx.front() != sample.context().address) {
      return std::nullopt;
    }
    ReadResponseError error{};
    const auto result = cache.complete(sample, rx.data(), rx.size(), ms, true, error);
    if (result.ok() || result.code == Err::EXCEPTION) {
      CHECK(health.recordReply(attempt, ms, true));
    } else {
      CHECK(health.recordFailure(attempt, result, ms, true));
    }
    stats.recordAttemptError(result);
    return result;
  }
};

void checkReadOutcomes(DeviceModel model) {
  Script s;
  s.bind(model);
  // Pre-TX terminal results can be reported for the logical operation alone.
  for (const auto code : {Err::DISALLOWED, Err::TIMEOUT, Err::ABORTED}) {
    s.stats.recordOperation({code, 0, "unsent job"}, 0);
    checkEmpty(s.health);
  }
  CHECK(s.stats.failCount == 3 && s.stats.timeouts == 0);
  CHECK(s.cache.buildRead(DISTANCE, s.tx.data(), 0, s.sample) == 0);
  CHECK(s.cache.snapshot(DISTANCE, 0, 0).usable.freshness == Freshness::NeverSampled);
  s.stats.reset();
  CHECK(s.transfers == 0);

  s.sendRead();
  CHECK(!s.receive(FOREIGN, 2) && !s.receive(FOREIGN, 3));
  checkEmpty(s.health);
  CHECK(s.cache.discard(s.sample));
  CHECK(s.health.recordFailure(s.attempt, TIMEOUT, 5, true));
  s.stats.recordAttemptError(TIMEOUT);
  s.sendRead();
  s.stats.recordRetry();
  auto result = s.receive(DISTANCE_754, 10);
  CHECK(result && result->ok());
  s.stats.recordOperation(*result, 10);
  CHECK(s.health.healthSnapshot().totalFailures == 1 &&
        s.health.healthSnapshot().totalSuccess == 1);
  CHECK(s.stats.retryCount == 1 && s.stats.timeouts == 1 && s.stats.successCount == 1);
  CHECK(s.stats.failCount == 0 && s.stats.frameErrors == 0 && s.stats.crcErrors == 0);
  const auto sample = s.cache.snapshot(DISTANCE, 10, 0);
  CHECK(sample.usable.measurement.model == model && sample.usable.measurement.timeValid);
  CHECK(sample.usable.measurement.value == 754 && sample.usable.measurement.completedMs == 10);
  CHECK(s.health.isOnline() && s.transfers == 2);

  s.sendRead();
  result = s.receive(EXCEPTION, 11);
  CHECK(result && result->code == Err::EXCEPTION && result->detail == 0xE1);
  s.stats.recordOperation(*result, 11);
  const auto after = s.cache.snapshot(DISTANCE, 11, 10);
  CHECK(after.raw.sampleGeneration == sample.raw.sampleGeneration);
  CHECK(after.usable.sampleGeneration == sample.usable.sampleGeneration);
  CHECK(after.raw.measurement.rawWord == 754 && after.raw.measurement.completedMs == 10);
  CHECK(after.usable.measurement.value == 754 && after.usable.measurement.completedMs == 10);
  CHECK(after.raw.measurement.timeValid && after.usable.measurement.timeValid);
  CHECK(s.health.state() == DriverState::READY && s.health.healthSnapshot().lastOkMs == 11);
  CHECK(s.health.healthSnapshot().totalSuccess == 2 &&
        s.health.healthSnapshot().totalFailures == 1);
  CHECK(s.health.healthSnapshot().lastError.code == Err::TIMEOUT);
  CHECK(s.stats.exceptions == 1 && s.stats.failCount == 1 && s.stats.successCount == 1);
  CHECK(s.stats.retryCount == 1 && s.stats.timeouts == 1);
  CHECK(s.stats.frameErrors == 0 && s.stats.crcErrors == 0);
  CHECK(s.stats.lastError == Err::EXCEPTION && s.stats.lastErrorDetail == 0xE1 && s.transfers == 3);

  s.sendRead();
  result = s.receive(DISTANCE_3001, 12);
  CHECK(result && result->ok());
  const auto next = s.cache.snapshot(DISTANCE, 12, 10);
  CHECK(next.raw.measurement.rawWord == 3001 && next.raw.measurement.completedMs == 12);
  if (model == SHORT_MODEL) {
    CHECK(next.raw.freshness == Freshness::Unusable);
    CHECK(next.usable.sampleGeneration == sample.usable.sampleGeneration);
  } else {
    CHECK(next.raw.freshness == Freshness::Fresh);
    CHECK(next.usable.measurement.value == 3001 && next.usable.measurement.completedMs == 12);
  }
  CHECK(s.health.healthSnapshot().totalSuccess == 3);
  auto badCrc = DISTANCE_754;
  badCrc.back() ^= 1;
  s.sendRead();
  result = s.receive(badCrc, 13);
  CHECK(result && result->code == Err::CRC_ERROR);
  s.sendRead();
  constexpr std::array<std::uint8_t, 3> shortFrame{1, 3, 2};
  result = s.receive(shortFrame, 14);
  CHECK(result && result->code == Err::FRAME_ERROR);
  CHECK(s.stats.crcErrors == 1 && s.stats.frameErrors == 1 && s.stats.timeouts == 1);
  CHECK(s.health.healthSnapshot().totalFailures == 3 &&
        s.health.healthSnapshot().lastErrorMs == 14);
  s.stats.reset();
  CHECK(s.health.healthSnapshot().totalFailures == 3 && s.transfers == 6);
}

void checkRebind(DeviceModel model) {
  Script s;
  s.bind(model);
  s.sendRead();
  auto result = s.receive(DISTANCE_754, 1);
  CHECK(result && result->ok());
  ConfigurationToken configRead;
  CHECK(s.config.buildRead(ENDPOINT, Setting::Baud, s.tx.data(), s.tx.size(), configRead) ==
        s.tx.size());
  s.send();
  CHECK(s.config.completeRead(configRead, ENDPOINT, BAUD_9600.data(), BAUD_9600.size(), 2));
  CHECK(s.health.recordReply(s.attempt, 2, true));
  CHECK(s.config.snapshot(Setting::Baud).trusted && s.config.endpointTrusted());
  s.sendRead();
  const auto oldSample = s.sample;
  const auto oldAttempt = s.attempt;
  s.bind(model);  // The same model and endpoint on another port start a new binding.
  checkEmpty(s.health);
  CHECK(s.cache.snapshot(DISTANCE, 3, 10).usable.freshness == Freshness::NeverSampled);
  CHECK(!s.config.snapshot(Setting::Baud).observed && !s.config.endpointTrusted());
  CHECK(s.transfers == 3);
  s.sendRead();
  ReadResponseError error{};
  CHECK(
      s.cache.complete(oldSample, DISTANCE_754.data(), DISTANCE_754.size(), 4, true, error).code ==
      Err::INVALID_CONFIG);
  CHECK(!s.health.recordReply(oldAttempt, 4, true));
  CHECK(!s.health.recordFailure(oldAttempt, TIMEOUT, 4, true));
  checkEmpty(s.health);
  CHECK(s.cache.snapshot(DISTANCE, 4, 10).raw.freshness == Freshness::NeverSampled);
  result = s.receive(DISTANCE_754, 5);
  CHECK(result && result->ok());
  CHECK(s.config.buildRead(ENDPOINT, Setting::Baud, s.tx.data(), s.tx.size(), configRead) ==
        s.tx.size());
  s.send();
  s.bind(model);  // This time the delayed exchange was a configuration read.
  CHECK(s.config.completeRead(configRead, ENDPOINT, BAUD_9600.data(), BAUD_9600.size(), 6).code ==
        Err::INVALID_CONFIG);
  CHECK(!s.health.recordReply(s.attempt, 6, true) && !s.config.endpointTrusted());
  CHECK(!s.config.snapshot(Setting::Baud).observed && !s.config.snapshot(Setting::Baud).trusted);
  s.sendRead();
  s.health.unbind();
  CHECK(!s.health.recordReply(s.attempt, 7, true));
  CHECK(!s.health.beginAttempt(s.attempt));
  checkEmpty(s.health);
  CHECK(s.transfers == 6);
}

void checkWriteOrigin() {
  Script s;
  for (auto origin : {AckOrigin::LocalEcho, AckOrigin::Unknown, AckOrigin::Device}) {
    s.bind(SHORT_MODEL);
    ConfigurationToken write;
    CHECK(s.config.buildWrite(Setting::Address, {SettingUnit::Address, 5}, WriteScope::Addressed,
                              s.tx.data(), s.tx.size(), write) == s.tx.size());
    s.send();
    CHECK(s.config.recordTx(write, TxProgress::Complete, Ok()));
    CHECK(s.config.recordAck(write, s.tx.data(), s.tx.size(), origin));
    if (s.config.operation().ackOrigin == AckOrigin::Device) {
      CHECK(s.health.recordReply(s.attempt, 9, true));
    } else {
      CHECK(s.health.discard(s.attempt));  // Owner ended the attempt without attributing the bytes.
    }
    CHECK(!s.health.recordReply(s.attempt, 10, true));
    CHECK(s.health.healthSnapshot().totalSuccess == (origin == AckOrigin::Device ? 1u : 0u));
    CHECK(s.health.isOnline() == (origin == AckOrigin::Device));
  }
  CHECK(s.transfers == 3);
}

void checkFailedVerification() {
  Script s;
  s.bind(LONG_MODEL);
  ConfigurationToken write, read;
  CHECK(s.config.buildWrite(Setting::Baud, {SettingUnit::BaudCode, 3}, WriteScope::Addressed,
                            s.tx.data(), s.tx.size(), write) == s.tx.size());
  s.send();
  CHECK(s.config.recordTx(write, TxProgress::Complete, Ok()));
  CHECK(s.config.recordAck(write, s.tx.data(), s.tx.size(), AckOrigin::LocalEcho));
  CHECK(s.health.discard(s.attempt));
  CHECK(s.config.buildVerification(write, s.tx.data(), s.tx.size(), read) == s.tx.size());
  CHECK(s.config.operation().intended.bps == 9600);
  s.send();
  CHECK(s.config.failRead(read, TIMEOUT).code == Err::LINK_LOST);
  const auto& op = s.config.operation();
  CHECK(op.readStatus && op.readStatus->code == Err::TIMEOUT && op.primary);
  CHECK(s.health.recordFailure(s.attempt, *op.readStatus, 20, true));
  s.stats.recordAttemptError(*op.readStatus);
  constexpr Status CLEANUP{Err::IO, -23, "host restore failed"};
  CHECK(s.config.recordCleanup(write, CLEANUP).code == Err::IO);
  CHECK(op.cleanup && op.cleanup->detail == -23 && op.primary->code == Err::LINK_LOST);
  s.stats.recordOperation(*op.primary, 21);
  const auto health = s.health.healthSnapshot();
  CHECK(health.totalFailures == 1 && health.totalSuccess == 0 && health.consecutiveFailures == 1);
  CHECK(health.lastError.code == Err::TIMEOUT && health.lastError.detail == -19);
  CHECK(health.lastErrorMs == 20 && health.lastErrorTimeValid && s.transfers == 2);
  CHECK(s.stats.timeouts == 1 && s.stats.failCount == 1 && s.stats.successCount == 0);
  CHECK(s.stats.lastError == Err::LINK_LOST && s.stats.lastErrorTimestampMs == 21);
}
}  // namespace

int main() {
  CHECK(std::strcmp(driverStateName(DriverState::UNINIT), "UNINIT") == 0);
  CHECK(std::strcmp(driverStateName(DriverState::READY), "READY") == 0);
  CHECK(std::strcmp(driverStateName(DriverState::DEGRADED), "DEGRADED") == 0);
  CHECK(std::strcmp(driverStateName(DriverState::OFFLINE), "OFFLINE") == 0);
  CHECK(std::strcmp(driverStateName(static_cast<DriverState>(255)), "UNKNOWN") == 0);
  checkTransitions();
  for (auto model : {SHORT_MODEL, LONG_MODEL}) {
    checkReadOutcomes(model);
    checkRebind(model);
  }
  checkWriteOrigin();
  checkFailedVerification();
}
