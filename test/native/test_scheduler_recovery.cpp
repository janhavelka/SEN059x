#include <cstddef>
#include <cstdint>
#include <vector>

#include "SchedulerFixture.h"

namespace {
using namespace SEN059x;
using namespace SEN059x::example;
using scheduler_test::Timeline;

using Bytes = std::vector<std::uint8_t>;
constexpr auto DISTANCE = MeasurementKind::RealTimeDistance;
constexpr auto TEMPERATURE = MeasurementKind::Temperature;
const Bytes DISTANCE_REQUEST{1, 3, 1, 1, 0, 1, 0xD4, 0x36};
const Bytes TEMP_REQUEST{1, 3, 1, 2, 0, 1, 0x24, 0x36};
// DFRobot example 3 is also the FC03 response to a distance read of 300 mm.
const Bytes LATE_DISTANCE{1, 3, 2, 1, 0x2C, 0xB8, 0x09};
const Bytes LATE_EXCEPTION{1, 0x83, 2, 0xC0, 0xF1};
const Bytes TEMP_20_C{1, 3, 2, 0, 0xC8, 0xB9, 0xD2};
const Bytes TEMP_25_C{1, 3, 2, 0, 0xFA, 0x38, 0x07};
const Bytes PEER_REQUEST{2, 3, 1, 1, 0, 1, 0xD4, 0x05};
const Bytes PEER_REPLY{2, 3, 2, 2, 0xEF, 0xBC, 0xA8};

struct Fixture : Timeline {
  explicit Fixture(DeviceModel model = DeviceModel::SEN0591) {
    CHECK(owner.bind(0, model, 1, 115200));
    CHECK(owner.bind(1, DeviceModel::SEN0592, 2, 115200));
  }
  ScheduledResult take(std::uint64_t ticket, std::uint64_t id, std::uint32_t step, Err code) {
    const auto result = collect(ticket);
    CHECK(result.exchange.request.operationId == id);
    CHECK(result.step == step && result.status.code == code);
    return result;
  }
  SampleSnapshot sample(std::size_t slot, MeasurementKind kind) const {
    return owner.snapshot(slot, kind, static_cast<std::uint32_t>(port.time / 1000), 1000);
  }
};

void timeoutThenTemperature(DeviceModel model) {
  Fixture f(model);
  const auto seed = f.submit(0, TEMPERATURE, 36, 2);
  f.send(0, TEMP_REQUEST);
  f.drain(695);
  f.reply(10000, TEMP_20_C);
  f.take(seed, 36, 2, Err::OK);
  const auto before = f.sample(0, TEMPERATURE);
  CHECK(before.usable.measurement.value == 20 && before.raw.measurement.completedMs == 10);

  const auto distance = f.submit(0, DISTANCE, 37, 3, 160000);
  f.send(15000, DISTANCE_REQUEST);
  f.drain(15695);
  f.poll(160000);
  const auto failed = f.take(distance, 37, 3, Err::TIMEOUT);
  CHECK(failed.attempts == 1 && !failed.queueExpired && failed.exchange.txDrained);
  CHECK(failed.exchange.request.bindingGeneration == before.bindingGeneration);
  CHECK(failed.exchange.request.context.model == model);
  CHECK(!f.owner.settled() && f.owner.stats().timeouts == 1);
  const auto next = f.submit(0, TEMPERATURE, 38, 4);
  f.poll(170000);
  CHECK(f.port.writes.size() == 2 && !f.owner.recover());

  // Each has a complete FC03-shaped prefix. Source, closure, address and CRC still matter.
  f.port.deliver(LATE_DISTANCE, AckOrigin::LocalEcho);
  CHECK(!f.owner.recover());
  f.port.deliver(LATE_DISTANCE, AckOrigin::Unknown);
  CHECK(!f.owner.recover());
  f.port.deliver(PEER_REPLY);
  CHECK(!f.owner.recover());
  f.port.deliver({1, 3, 2, 1, 0x2C, 0xB8, 0x08});  // Alter the high CRC byte only.
  CHECK(!f.owner.recover());
  f.port.deliver(LATE_DISTANCE, AckOrigin::Device, false);
  CHECK(!f.owner.recover());
  f.port.deliver(LATE_DISTANCE);
  f.port.frameStarted = false;  // A CRC-valid suffix of an already fragmented frame.
  CHECK(!f.owner.recover());
  f.poll(180000);
  CHECK(!f.owner.settled() && f.port.writes.size() == 2);
  f.port.deliver(LATE_DISTANCE);
  CHECK(f.owner.recover() && f.owner.settled());
  const auto after = f.sample(0, TEMPERATURE);
  CHECK(after.raw.sampleGeneration == before.raw.sampleGeneration);
  CHECK(after.usable.sampleGeneration == before.usable.sampleGeneration);
  CHECK(after.raw.measurement.rawWord == 200 && after.usable.measurement.completedMs == 10);
  CHECK(f.sample(0, DISTANCE).raw.freshness == Freshness::NeverSampled);
  CHECK(f.owner.health(0).totalFailures == 1 && f.owner.health(0).totalSuccess == 1);
  CHECK(f.owner.stats().failCount == 1 && f.owner.stats().successCount == 1);
  CHECK(f.owner.physicalAttempts() == 2 && f.port.writes.size() == 2);
  ScheduledResult pending;
  CHECK(!f.owner.take(next, pending));
  f.send(181000, TEMP_REQUEST);
  f.drain(181695);
  f.reply(191000, TEMP_25_C);
  const auto result = f.take(next, 38, 4, Err::OK);
  CHECK(result.exchange.request.context.startRegister == 0x0102);
  CHECK(f.sample(0, TEMPERATURE).usable.measurement.value == 25);
  CHECK(f.sample(0, TEMPERATURE).raw.measurement.completedMs == 191);
  CHECK(f.owner.stats().successCount == 2 && f.owner.stats().failCount == 1);
  CHECK(f.owner.stats().retryCount == 0 && f.owner.physicalAttempts() == 3);
}

void cancelDuringDrainAndRebind() {
  Fixture f;
  const auto oldGeneration = f.sample(0, DISTANCE).bindingGeneration;
  const auto old = f.submit(0, DISTANCE, 40, 3);
  const auto peer = f.submit(1, DISTANCE, 41, 0);
  f.port.drained = false;
  f.send(0, DISTANCE_REQUEST);
  f.poll(300);
  CHECK(f.owner.cancel(old));
  f.owner.disable(0);
  CHECK(f.owner.bind(0, DeviceModel::SEN0592, 1, 115200));
  const auto rebound = f.submit(0, TEMPERATURE, 40, 3);
  CHECK(rebound != old && !f.owner.cancel(old));
  const auto newGeneration = f.sample(0, TEMPERATURE).bindingGeneration;
  CHECK(newGeneration != oldGeneration);
  f.poll(600);
  CHECK(!f.owner.recover() && f.port.transmitting && f.port.reads == 0);
  f.port.time = 695;
  f.port.drained = true;
  CHECK(!f.owner.recover() && !f.port.transmitting);
  f.poll(160000);
  CHECK(f.port.writes.size() == 1 && !f.owner.settled());
  f.port.deliver(LATE_DISTANCE);
  CHECK(f.owner.recover());
  CHECK(f.owner.health(0).totalSuccess == 0 && f.owner.health(0).totalFailures == 0);
  CHECK(f.sample(0, DISTANCE).raw.freshness == Freshness::NeverSampled);
  CHECK(f.sample(0, TEMPERATURE).usable.freshness == Freshness::NeverSampled);

  f.send(161000, PEER_REQUEST);
  const auto cancelled = f.take(old, 40, 3, Err::ABORTED);
  CHECK(cancelled.attempts == 1 && cancelled.exchange.request.bindingGeneration == oldGeneration);
  CHECK(cancelled.exchange.request.context.model == DeviceModel::SEN0591);
  f.drain(161695);
  f.reply(210000, PEER_REPLY);
  f.take(peer, 41, 0, Err::OK);
  CHECK(f.owner.health(1).totalSuccess == 1);
  CHECK(f.sample(1, DISTANCE).usable.measurement.value == 751);
  f.send(211000, TEMP_REQUEST);
  CHECK(!f.owner.cancel(old));
  f.drain(211695);
  f.reply(230000, TEMP_25_C);
  const auto fresh = f.take(rebound, 40, 3, Err::OK);
  CHECK(fresh.exchange.request.bindingGeneration == newGeneration);
  CHECK(fresh.exchange.request.context.model == DeviceModel::SEN0592);
  CHECK(f.sample(0, TEMPERATURE).raw.measurement.completedMs == 230);
  CHECK(f.owner.stats().successCount == 2 && f.owner.stats().failCount == 1);
  CHECK(f.owner.stats().timeouts == 0 && f.owner.physicalAttempts() == 3);
}

void separateFramesCannotPublish() {
  Fixture f;
  const auto ticket = f.submit(0, TEMPERATURE, 42, 0);
  const auto peer = f.submit(1, DISTANCE, 43, 0);
  f.send(0, TEMP_REQUEST);
  f.drain(695);
  f.port.deliver({1, 3, 2});
  f.port.frameClosed = false;
  f.poll(5200);
  // The driver first reports the intervening RTU boundary on the second batch.
  // Joining the two would produce the CRC-valid 30.0 degree manufacturer frame.
  f.reply(8000, {1, 0x2C, 0xB8, 0x09});
  const auto result = f.take(ticket, 42, 0, Err::FRAME_ERROR);
  CHECK(result.exchange.wireGapError && !f.owner.settled());
  CHECK(result.measurement.quality == MeasurementQuality::Unobserved);
  CHECK(f.sample(0, TEMPERATURE).raw.freshness == Freshness::NeverSampled);
  CHECK(f.sample(0, TEMPERATURE).usable.freshness == Freshness::NeverSampled);
  CHECK(f.owner.health(0).totalSuccess == 0 && f.owner.health(0).totalFailures == 1);
  f.poll(9000);
  CHECK(f.port.writes.size() == 1 && f.owner.stats().frameErrors == 1);
  f.port.deliver(TEMP_25_C);
  CHECK(f.owner.recover());
  f.send(10000, PEER_REQUEST);
  f.drain(10695);
  f.reply(150000, PEER_REPLY);
  f.take(peer, 43, 0, Err::OK);
}

void retainedObservationAfterRebind() {
  Fixture f;
  const auto first = f.submit(0, TEMPERATURE, 44, 0);
  const auto second = f.submit(0, TEMPERATURE, 45, 0);
  f.send(0, TEMP_REQUEST);
  f.drain(695);
  f.reply(10000, TEMP_20_C);
  f.send(10000, TEMP_REQUEST);
  f.drain(10695);
  f.reply(22000, {1, 3, 2, 0, 0xFB, 0xF9, 0xC7});  // Raw 251, off the half-degree grid.
  CHECK(f.owner.bind(0, DeviceModel::SEN0592, 1, 9600));
  CHECK(f.sample(0, TEMPERATURE).raw.freshness == Freshness::NeverSampled);

  const auto newer = f.take(second, 45, 0, Err::OK).measurement;
  CHECK(newer.model == DeviceModel::SEN0591 && newer.rawWord == 251 && !newer.value);
  CHECK(newer.quality == MeasurementQuality::UnexpectedQuantization && newer.completedMs == 22);
  const auto older = f.take(first, 44, 0, Err::OK).measurement;
  CHECK(older.kind == TEMPERATURE && older.unit == MeasurementUnit::DegreesCelsius);
  CHECK(older.rawWord == 200 && older.value == 20 && older.completedMs == 10 && older.timeValid);
}

void shortWriteRequiresIsolation() {
  Fixture f;
  const auto shortTx = f.submit(0, DISTANCE, 50, 1);
  const auto peer = f.submit(1, DISTANCE, 51, 1);
  f.port.writeLimit = 3;
  f.send(0, {1, 3, 1});
  f.drain(1000);
  const auto failed = f.take(shortTx, 50, 1, Err::IO);
  CHECK(failed.attempts == 1 && failed.exchange.txAccepted == 3 && failed.exchange.txDrained);
  CHECK(!f.owner.recoverIsolated());
  f.port.deliver(LATE_DISTANCE);
  CHECK(!f.owner.recover() && !f.owner.settled());
  f.poll(150000);
  CHECK(f.port.writes.size() == 1 && f.owner.health(0).totalFailures == 0);
  f.port.isolationVerified = true;
  CHECK(f.owner.recoverIsolated() && f.owner.settled());
  f.port.writeLimit = 8;
  f.send(151000, PEER_REQUEST);
  f.drain(151695);
  f.reply(200000, PEER_REPLY);
  f.take(peer, 51, 1, Err::OK);
  CHECK(f.owner.stats().failCount == 1 && f.owner.stats().successCount == 1);
  CHECK(f.owner.stats().timeouts == 0 && f.owner.physicalAttempts() == 2);
}

void resetWithFailedDrain() {
  Fixture f;
  const auto active = f.submit(0, DISTANCE, 60, 2);
  const auto queued = f.submit(1, DISTANCE, 61, 2);
  f.send(0, DISTANCE_REQUEST);
  f.port.drainStatus = {Err::IO, -33, "Drain failed"};
  f.poll(500);
  CHECK(f.port.transmitting && !f.owner.settled());
  f.owner.reset();
  const auto failure = f.take(active, 60, 2, Err::IO);
  CHECK(failure.exchange.status.detail == -33 && !failure.exchange.txDrained);
  CHECK(f.take(queued, 61, 2, Err::ABORTED).attempts == 0);
  CHECK(f.owner.bind(0, DeviceModel::SEN0591, 1, 115200));
  const auto next = f.submit(0, TEMPERATURE, 60, 2);
  CHECK(!f.owner.cancel(active));
  f.poll(150000);
  CHECK(!f.owner.recover() && !f.owner.recoverIsolated());
  CHECK(f.port.transmitting && f.port.writes.size() == 1 && f.port.reads == 0);
  CHECK(f.owner.health(0).totalFailures == 0 && f.owner.health(0).totalSuccess == 0);
  f.port.isolationVerified = true;
  CHECK(f.owner.recoverIsolated() && !f.port.transmitting);
  f.port.drainStatus = Ok();
  f.send(151000, TEMP_REQUEST);
  f.drain(151695);
  f.reply(165000, TEMP_20_C);
  f.take(next, 60, 2, Err::OK);
  CHECK(f.owner.health(0).totalSuccess == 1 && f.owner.health(0).totalFailures == 0);
  CHECK(f.owner.stats().failCount == 2 && f.owner.stats().successCount == 1);
  CHECK(f.owner.physicalAttempts() == 2);
}

void retireDuringReceive(bool reset) {
  Fixture f;
  const auto active = f.submit(0, DISTANCE, 70, 2);
  f.send(0, DISTANCE_REQUEST);
  f.drain(695);
  if (reset) {
    f.owner.reset();
  } else {
    f.owner.disable(0);
  }
  const auto cancelled = f.take(active, 70, 2, Err::ABORTED);
  CHECK(cancelled.exchange.txDrained && cancelled.attempts == 1);
  CHECK(f.owner.bind(0, DeviceModel::SEN0592, 1, 115200));
  const auto next = f.submit(0, TEMPERATURE, 70, 2);
  // The old UART-ready callback arrives after the binding and operation have been replaced.
  f.port.deliver(reset ? LATE_EXCEPTION : LATE_DISTANCE);
  f.poll(150000);
  CHECK(f.port.writes.size() == 1 && !f.owner.settled());
  CHECK(f.sample(0, TEMPERATURE).raw.freshness == Freshness::NeverSampled);
  CHECK(!f.owner.cancel(active) && f.owner.recover());
  CHECK(f.owner.health(0).totalSuccess == 0 && f.owner.health(0).totalFailures == 0);
  f.send(151000, TEMP_REQUEST);
  f.drain(151695);
  f.reply(165000, TEMP_20_C);
  f.take(next, 70, 2, Err::OK);
  CHECK(f.owner.stats().failCount == 1 && f.owner.stats().successCount == 1);
  CHECK(f.owner.stats().exceptions == 0 && f.owner.physicalAttempts() == 2);
}
}  // namespace

int main() {
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    timeoutThenTemperature(model);
  }
  cancelDuringDrainAndRebind();
  separateFramesCannotPublish();
  retainedObservationAfterRebind();
  shortWriteRequiresIsolation();
  resetWithFailedDrain();
  retireDuringReceive(false);
  retireDuringReceive(true);
}
