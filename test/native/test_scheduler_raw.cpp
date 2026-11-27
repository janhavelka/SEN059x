#include "SchedulerFixture.h"

namespace {
using namespace SEN059x;
using namespace SEN059x::example;
using scheduler_test::Timeline;

// Published one-word requests and replies, plus a synthetic three-word reply with the
// same data as DFRobot examples 1..3. A physical block response is unverified on either SKU.
const Timeline::Bytes BLOCK{1, 3, 1, 0, 0, 3, 0x04, 0x37};
const Timeline::Bytes WORDS{1, 3, 6, 2, 0xF2, 2, 0xEF, 1, 0x2C, 0x29, 0x42};
const Timeline::Bytes TEMP{1, 3, 1, 2, 0, 1, 0x24, 0x36};
const Timeline::Bytes TEMP_30{1, 3, 2, 1, 0x2C, 0xB8, 0x09};
const Timeline::Bytes BAUD{1, 3, 2, 1, 0, 1, 0xD4, 0x72};
const Timeline::Bytes CODE_3{1, 3, 2, 0, 3, 0xF8, 0x45};
const Timeline::Bytes EXCEPTION{1, 0x83, 2, 0xC0, 0xF1};

void checkBudgets(DeviceModel model) {
  Timeline t;
  std::uint64_t budget = 47, ticket = 99;
  CHECK(!t.owner.readBudget(0, 0x100, 3, budget));
  CHECK(!t.owner.bind(0, model, 255, 115200, AddressPolicy::DFRobotIsolated));
  CHECK(!t.owner.submitRawRead(0, 0x100, 3, 1, 0, 2000000, ticket));
  CHECK(t.owner.bind(0, model, 1, 115200));
  const std::pair<std::uint16_t, std::uint16_t> bad[]{
      {0x100, 0}, {0x100, 126}, {0x101, 3}, {0xFFFF, 2}};
  for (const auto& [reg, count] : bad) {
    CHECK(!t.owner.readBudget(0, reg, count, budget) && budget == 47);
    CHECK(!t.owner.submitRawRead(0, reg, count, 1, 0, 2000000, ticket) && ticket == 99);
  }
  CHECK(t.port.writes.empty() && t.owner.stats().failCount == 0);
  // At 115200 baud: TX 695 us, eleven-byte RX 955 us, closure 1750 us, turnaround
  // 100 us, margin 1000 us, processing 750 + 140 + 140 ms (both SKU manuals table 1).
  CHECK(t.owner.readBudget(0, 0x100, 3, budget) && budget == 1034500);
  CHECK(t.owner.readBudget(0, 0x102, 1, budget) && budget == 144153);
  // Nine-byte RX 782 us; two configuration words receive 2 x 50 ms of owner allowance.
  CHECK(t.owner.readBudget(0, 0x205, 2, budget) && budget == 104327);
  t.port.time = (1ull << 32) * 1000 + 27;
  CHECK(t.owner.nowUs() == t.port.time && t.owner.nowMs() == 0);
}

void checkCacheIsolation(DeviceModel model) {
  Timeline t;
  CHECK(t.owner.bind(0, model, 1, 115200));
  const auto sampleTicket = t.submit(0, MeasurementKind::Temperature, 42, 0);
  t.send(0, TEMP);
  t.drain(700);
  t.reply(9000, TEMP_30);
  const auto sample = t.collect(sampleTicket).sampleHistory;
  CHECK(sample.raw.measurement.rawWord == 300 && sample.raw.measurement.completedMs == 9);
  std::uint64_t ticket = 0;
  CHECK(t.owner.submitSettingRead(0, Setting::Baud, 43, 0, 500000, ticket));
  t.send(10000, BAUD);
  t.drain(10700);
  t.reply(15000, CODE_3);
  const auto setting = t.collect(ticket).settingHistory;
  CHECK(setting.trusted && setting.observed && setting.observed->completedMs == 15);

  struct Case {
    std::uint16_t reg, count;
    const Timeline::Bytes& request;
    const Timeline::Bytes& reply;
    std::array<std::uint16_t, 3> expected;
  };
  const Case cases[]{{0x0102, 1, TEMP, CODE_3, {3}},
                     {0x0201, 1, BAUD, TEMP_30, {300}},
                     {0x0100, 3, BLOCK, WORDS, {754, 751, 300}}};
  for (const auto& item : cases) {
    const auto start = t.owner.nowUs() + 1000;
    CHECK(t.owner.submitRawRead(0, item.reg, item.count, 44, 1, 2000000, ticket));
    t.send(start, item.request);
    t.drain(start + 700);
    t.reply(start + 2000, item.reply);
    const auto result = t.collect(ticket);
    CHECK(result.status && result.attempts == 1 && !result.settingObservation);
    CHECK(result.sampleHistory.raw.sampleGeneration == 0 && !result.settingHistory.observed);
    CHECK(result.exchange.wordCount == item.count);
    CHECK(std::equal(item.expected.begin(), item.expected.begin() + item.count,
                     result.exchange.words.begin()));
  }
  const auto after = t.owner.snapshot(0, MeasurementKind::Temperature, t.owner.nowMs(), 1000);
  CHECK(after.bindingGeneration == sample.bindingGeneration);
  CHECK(after.raw.sampleGeneration == sample.raw.sampleGeneration);
  CHECK(after.usable.sampleGeneration == sample.usable.sampleGeneration);
  CHECK(after.raw.measurement.rawWord == 300 && after.usable.measurement.completedMs == 9);
  const auto baud = t.owner.settingSnapshot(0, Setting::Baud);
  CHECK(t.owner.endpointTrusted(0) && baud.trusted && baud.observed->value.rawWord == 3);
  CHECK(baud.observed->completedMs == 15 && t.owner.physicalAttempts() == 5);
  CHECK(t.owner.health(0).totalSuccess == 5);

  // Two failed typed tickets remain available after rebinding clears the current caches.
  const auto failedSample = t.submit(0, MeasurementKind::Temperature, 45, 0);
  CHECK(t.owner.submitSettingRead(0, Setting::Baud, 46, 0, 500000, ticket));
  t.send(30000, TEMP);
  t.drain(30700);
  t.reply(35000, EXCEPTION);
  t.send(35000, BAUD);
  t.drain(35700);
  t.reply(38000, EXCEPTION);
  CHECK(t.owner.bind(0, model, 1, 115200));
  const auto failed = t.collect(failedSample);
  CHECK(failed.status.code == Err::EXCEPTION &&
        failed.measurement.quality == MeasurementQuality::Unobserved);
  CHECK(failed.sampleHistory.usable.ageMs == 26 &&
        failed.sampleHistory.usable.measurement.completedMs == 9);
  const auto config = t.collect(ticket);
  CHECK(config.status.code == Err::EXCEPTION && !config.settingObservation);
  CHECK(config.settingHistory.trusted && config.settingHistory.observed->completedMs == 15);
}

void checkBlockFailure(DeviceModel model, Err failure) {
  Timeline t;
  CHECK(t.owner.bind(0, model, 1, 115200));
  std::uint64_t ticket = 0;
  CHECK(t.owner.submitRawRead(0, 0x100, 3, 7, 0, 1034500, ticket));
  t.send(0, BLOCK);
  t.drain(700);
  if (failure == Err::TIMEOUT) {
    t.poll(1034500);
  } else {
    t.reply(502000, failure == Err::EXCEPTION ? EXCEPTION : TEMP_30);
  }
  const auto result = t.collect(ticket);
  CHECK(result.status.code == failure && result.exchange.wordCount == 0 && result.attempts == 1);
  if (failure == Err::EXCEPTION) {
    CHECK(result.status.detail == 2 && t.owner.settled());
  } else {
    CHECK(!t.owner.settled());
    t.port.deliver(TEMP_30);
    CHECK(!t.owner.recover());  // Wrong count, despite valid address, CRC and independent origin.
    t.port.deliver(WORDS);
    CHECK(t.owner.recover());
  }
  t.poll(2100000);
  CHECK(t.port.writes.size() == 1 && t.owner.stats().retryCount == 0);
  CHECK(t.owner.snapshot(0, MeasurementKind::Temperature, 2100, 1000).raw.sampleGeneration == 0);
}
}  // namespace

int main() {
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    checkBudgets(model);
    checkCacheIsolation(model);
    for (auto failure : {Err::EXCEPTION, Err::TIMEOUT, Err::FRAME_ERROR}) {
      checkBlockFailure(model, failure);
    }
  }
}
