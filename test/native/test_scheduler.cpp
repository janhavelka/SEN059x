#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "SchedulerFixture.h"

namespace {
using namespace SEN059x;
using namespace SEN059x::example;
using scheduler_test::Timeline;
using Bytes = std::vector<std::uint8_t>;

constexpr auto PROCESSED = MeasurementKind::ProcessedDistance;
constexpr auto REALTIME = MeasurementKind::RealTimeDistance;
constexpr auto TEMP = MeasurementKind::Temperature;
constexpr auto ECHO = MeasurementKind::EchoTime;
constexpr auto SHORT_MODEL = DeviceModel::SEN0591;
constexpr auto LONG_MODEL = DeviceModel::SEN0592;

// Address 1 uses the published processed-distance example. The peer fixtures have
// independently calculated RTU CRCs and use the 751 mm word from example 2.
const Bytes PROCESSED_TX{1, 3, 1, 0, 0, 1, 0x85, 0xF6};
const Bytes PROCESSED_RX{1, 3, 2, 2, 0xF2, 0x38, 0xA1};
const Bytes TEMP_TX_3{3, 3, 1, 2, 0, 1, 0x25, 0xD4};
const Bytes EXCEPTION_3{3, 0x83, 2, 0x61, 0x31};
const Bytes REALTIME_TX_4{4, 3, 1, 1, 0, 1, 0xD4, 0x63};
const Bytes REALTIME_RX_4{4, 3, 2, 2, 0xEF, 0x34, 0xA8};
const Bytes ECHO_TX_1{1, 3, 1, 0x0A, 0, 1, 0xA5, 0xF4};
const Bytes ECHO_RX_1{1, 3, 2, 0x0D, 0x7A, 0x3D, 0x37};
const Bytes EXCEPTION_1{1, 0x83, 2, 0xC0, 0xF1};
const Bytes REALTIME_TX_2{2, 3, 1, 1, 0, 1, 0xD4, 0x05};
const Bytes REALTIME_RX_2{2, 3, 2, 2, 0xEF, 0xBC, 0xA8};

void fourBindings() {
  Timeline t;
  constexpr std::array<DeviceModel, 4> models{SHORT_MODEL, LONG_MODEL, SHORT_MODEL, LONG_MODEL};
  constexpr std::array<MeasurementKind, 4> kinds{PROCESSED, ECHO, TEMP, REALTIME};
  constexpr std::array<std::uint64_t, 4> deadlines{800000, 200123, 950000, 1000000};
  std::array<std::uint64_t, 4> tickets{};
  std::array<std::uint64_t, 4> generations{};
  for (std::size_t i = 0; i < tickets.size(); ++i) {
    CHECK(t.owner.bind(i, models[i], static_cast<std::uint8_t>(i + 1), 115200));
    generations[i] = t.owner.snapshot(i, kinds[i], 0, 0).bindingGeneration;
    CHECK(generations[i] != 0);
    tickets[i] = t.submit(i, kinds[i], 70 + i, 20 + i, deadlines[i]);
  }
  // All four result slots are reserved. A rejected rebind would steal D's work if it
  // retired the existing binding before checking address uniqueness.
  std::uint64_t fifth = 123456;
  CHECK(!t.owner.submit(0, TEMP, 74, 24, 1000000, false, fifth));
  CHECK(fifth == 123456);
  CHECK(!t.owner.bind(3, SHORT_MODEL, 1, 9600));
  CHECK(t.port.writes.empty());
  t.send(0, PROCESSED_TX);
  t.drain(695);
  CHECK(t.poll(deadlines[1] - 1) <= deadlines[1]);
  t.poll(deadlines[1]);
  CHECK(t.owner.stats().failCount == 1 && t.owner.stats().timeouts == 0);
  CHECK(t.port.writes.size() == 1 && t.owner.physicalAttempts() == 1);
  t.poll(750000);
  CHECK(t.port.writes.size() == 1 && t.owner.stats().successCount == 0);
  t.reply(754000, PROCESSED_RX);
  t.send(754000, TEMP_TX_3);
  t.drain(754695);
  t.reply(760000, EXCEPTION_3);
  t.send(760000, REALTIME_TX_4);
  t.drain(760695);
  // Consume A after C completed and D is already waiting for its response.
  CHECK(!t.owner.submit(0, TEMP, 74, 24, 1000000, false, fifth));
  std::array<ScheduledResult, 4> results{};
  results[0] = t.collect(tickets[0]);
  t.reply(902000, REALTIME_RX_4);
  for (std::size_t i = 1; i < results.size(); ++i) {
    results[i] = t.collect(tickets[i]);
  }
  t.poll(1000001);
  for (std::size_t i = 0; i < results.size(); ++i) {
    const auto& result = results[i];
    const auto& request = result.exchange.request;
    CHECK(request.operationId == 70 + i && result.step == 20 + i);
    CHECK(request.context.model == models[i] && request.context.address == i + 1);
    CHECK(request.context.startRegister == static_cast<std::uint16_t>(kinds[i]));
    CHECK(request.bindingGeneration == generations[i] && request.deadlineUs == deadlines[i]);
    CHECK(request.publishMeasurement && request.baud == 115200);
    CHECK(result.attempts == (i == 1 ? 0u : 1u));
    CHECK(result.queueExpired == (i == 1));
    const auto health = t.owner.health(i);
    CHECK(health.totalFailures == 0 && health.totalSuccess == (i == 1 ? 0u : 1u));
    const auto sample = t.owner.snapshot(i, kinds[i], 1000, 1000);
    if (i == 1 || i == 2) {
      CHECK(sample.raw.freshness == Freshness::NeverSampled);
      CHECK(sample.usable.freshness == Freshness::NeverSampled);
    } else {
      const bool processed = i == 0;
      const auto word = processed ? 754u : 751u;
      const auto ms = processed ? 754u : 902u;
      CHECK(result.status && result.completedUs == ms * 1000);
      CHECK(sample.raw.measurement.rawWord == word && sample.usable.measurement.value == word);
      CHECK(sample.raw.measurement.model == models[i] && sample.raw.measurement.address == i + 1);
      CHECK(sample.usable.measurement.completedMs == ms && sample.usable.measurement.timeValid);
      CHECK(health.lastOkTimeValid && health.lastOkMs == ms);
    }
  }
  CHECK(results[1].status.code == Err::TIMEOUT && results[1].exchange.txAccepted == 0);
  CHECK(results[2].status.code == Err::EXCEPTION && results[2].status.detail == 2);
  CHECK(t.owner.health(2).lastOkMs == 760);
  const auto stats = t.owner.stats();
  CHECK(stats.successCount == 2 && stats.failCount == 2 && stats.exceptions == 1);
  CHECK(stats.timeouts == 0 && stats.retryCount == 0);
  CHECK(stats.frameErrors == 0 && stats.crcErrors == 0);
  CHECK(t.owner.physicalAttempts() == 3 && t.port.writes.size() == 3);
}

void retryWithPeer(bool expireWaiting) {
  Timeline t;
  constexpr std::uint64_t finish = UINT64_C(0xFFFFFFFE) * 1000;
  constexpr std::uint64_t start = finish - 200000;
  const auto deadline = expireWaiting ? start + 50000 : finish + 20000;
  t.port.time = start;
  CHECK(t.owner.bind(0, LONG_MODEL, 1, 115200));
  CHECK(t.owner.bind(1, SHORT_MODEL, 2, 9600));
  const auto echo = t.submit(0, ECHO, 105, 9, deadline, true);
  const auto peer = t.submit(1, REALTIME, 106, 2, finish + 10000);
  t.send(start, ECHO_TX_1);
  t.drain(start + 695);
  t.reply(start + 5000, EXCEPTION_1);
  ScheduledResult pending{};
  CHECK(!t.owner.take(echo, pending));
  CHECK(t.owner.physicalAttempts() == 1 && t.owner.stats().retryCount == 0);
  CHECK(t.owner.stats().exceptions == 1 && t.owner.stats().failCount == 0);
  // The retry is queued in the first operation slot, but the peer has the next turn.
  // Inspecting it must leave the following dispatch at that same binding.
  const auto next = t.owner.activity();
  CHECK(next.phase == OwnerPhase::Queued && next.request && next.request->operationId == 106);
  t.send(start + 5000, REALTIME_TX_2, 9600);
  t.drain(start + 14000);
  if (expireWaiting) {
    t.poll(deadline);
    CHECK(t.owner.stats().failCount == 1 && t.owner.stats().retryCount == 0);
  }
  t.reply(start + 135000, REALTIME_RX_2);
  CHECK(t.collect(peer).status);
  if (!expireWaiting) {
    t.send(start + 135000, ECHO_TX_1);
    t.drain(start + 135695);
    t.reply(finish, ECHO_RX_1);
  }
  t.poll(finish + 5000);
  const auto result = t.collect(echo);
  CHECK(result.exchange.request.deadlineUs == deadline);
  CHECK(result.exchange.request.operationId == 105);
  CHECK(result.step == 9 && result.attempts == (expireWaiting ? 1u : 2u));
  const auto stats = t.owner.stats();
  CHECK(stats.successCount == (expireWaiting ? 1u : 2u));
  CHECK(stats.failCount == (expireWaiting ? 1u : 0u));
  CHECK(stats.exceptions == 1 && stats.timeouts == 0);
  CHECK(stats.retryCount == (expireWaiting ? 0u : 1u));
  CHECK(t.port.writes.size() == (expireWaiting ? 2u : 3u));
  CHECK(t.owner.physicalAttempts() == t.port.writes.size());
  const auto observation = t.owner.snapshot(0, ECHO, 3, 5);
  if (expireWaiting) {
    CHECK(result.status.code == Err::TIMEOUT && result.exchange.status.code == Err::EXCEPTION);
    CHECK(observation.raw.freshness == Freshness::NeverSampled);
  } else {
    CHECK(result.status && result.completedUs == finish);
    CHECK(observation.usable.measurement.completedMs == 0xFFFFFFFEu);
    CHECK(observation.usable.ageMs == 5 && observation.usable.freshness == Freshness::Fresh);
    CHECK(observation.usable.measurement.unit == MeasurementUnit::Microseconds);
    CHECK(observation.usable.measurement.value == 3450 && t.owner.health(0).totalSuccess == 2);
  }
}

void retryLimit() {
  Timeline t;
  CHECK(t.owner.bind(0, SHORT_MODEL, 1, 115200));
  const auto ticket = t.submit(0, ECHO, 107, 4, 500000, true);
  t.send(0, ECHO_TX_1);
  t.drain(695);
  t.reply(5000, EXCEPTION_1);
  t.send(5000, ECHO_TX_1);
  t.drain(5695);
  t.reply(10000, EXCEPTION_1);
  t.poll(20000);
  const auto result = t.collect(ticket);
  CHECK(result.attempts == 2 && result.status.code == Err::EXCEPTION && result.status.detail == 2);
  CHECK(t.port.writes.size() == 2 && t.owner.stats().retryCount == 1);
  CHECK(t.owner.stats().exceptions == 2 && t.owner.stats().failCount == 1);
  CHECK(t.owner.health(0).totalSuccess == 2 && t.owner.health(0).totalFailures == 0);
}

void retryWriteAcceptsNothing() {
  Timeline t;
  CHECK(t.owner.bind(0, SHORT_MODEL, 1, 115200));
  const auto ticket = t.submit(0, ECHO, 107, 4, 500000, true);
  t.send(0, ECHO_TX_1);
  t.drain(695);
  t.reply(5000, EXCEPTION_1);
  t.port.writeLimit = 0;
  t.poll(6000);

  const auto result = t.collect(ticket);
  CHECK(result.status.code == Err::IO && result.completedUs == 6000 && !result.queueExpired);
  CHECK(result.attempts == 1 && t.owner.physicalAttempts() == 1 && t.owner.settled());
  CHECK(result.exchange.status.code == Err::EXCEPTION && result.exchange.status.detail == 2);
  CHECK(result.exchange.completedUs == 5000 && result.exchange.txAccepted == 8);
  CHECK(result.exchange.txDrained && result.exchange.rxTotal == EXCEPTION_1.size());
  CHECK(t.owner.stats().retryCount == 0 && t.owner.stats().exceptions == 1);
  CHECK(t.owner.stats().failCount == 1 && t.owner.health(0).totalSuccess == 1);
}

void lastSchedulableDeadline() {
  Timeline t;
  t.port.time = UINT64_MAX - 10000;
  CHECK(t.owner.bind(0, SHORT_MODEL, 1, 115200));
  const auto active = t.submit(0, ECHO, 1, 0, UINT64_MAX - 5000);
  t.send(t.port.time, ECHO_TX_1);
  CHECK(t.owner.cancel(active));
  std::uint64_t refused = 123;
  CHECK(t.owner.submit(0, TEMP, 2, 0, UINT64_MAX, false, refused).code == Err::INVALID_CONFIG);
  CHECK(refused == 123);
  const auto queued = t.submit(0, TEMP, 2, 0, UINT64_MAX - 1);
  CHECK(t.poll(t.port.time) == UINT64_MAX - 1);
  CHECK(t.poll(UINT64_MAX - 1) == UINT64_MAX);
  CHECK(t.collect(queued).queueExpired && t.owner.physicalAttempts() == 1);
}

void hostConfigurationFailure() {
  Timeline t;
  CHECK(t.owner.bind(0, SHORT_MODEL, 1, 115200));
  CHECK(t.owner.bind(1, LONG_MODEL, 2, 9600));
  const auto first = t.submit(0, PROCESSED, 108, 0, 500000, true);
  const auto peer = t.submit(1, REALTIME, 109, 0, 500000);
  t.port.configureStatus = {Err::IO, -5, "UART configuration failed"};
  t.poll(0);
  const auto failed = t.collect(first);
  CHECK(failed.status.code == Err::IO && failed.status.detail == -5);
  CHECK(t.port.writes.empty() && failed.attempts == 0 && t.owner.settled());
  CHECK(t.owner.health(0).totalFailures == 0 && t.owner.stats().failCount == 1);
  t.port.configureStatus = Ok();
  t.send(0, REALTIME_TX_2, 9600);
  t.drain(8334);
  t.reply(150000, REALTIME_RX_2);
  CHECK(t.collect(peer).status && t.owner.stats().retryCount == 0);
  CHECK(t.owner.stats().successCount == 1 && t.owner.physicalAttempts() == 1);
}
}  // namespace

int main() {
  fourBindings();
  retryWithPeer(false);
  retryWithPeer(true);
  retryLimit();
  retryWriteAcceptsNothing();
  lastSchedulableDeadline();
  hostConfigurationFailure();
}
