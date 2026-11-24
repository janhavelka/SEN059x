#include "SchedulerFixture.h"

namespace {
using namespace SEN059x;
using scheduler_test::Timeline;

constexpr auto THRESHOLD = Setting::SwitchThreshold;
// DFRobot table 2 and OPERATIONS.csv: single-word FC03 at 0x0206, threshold in mm.
const Timeline::Bytes THRESHOLD_REQUEST{1, 3, 2, 6, 0, 1, 0x65, 0xB3};
const Timeline::Bytes THRESHOLD_100{1, 3, 2, 0, 0x64, 0xB9, 0xAF};
const Timeline::Bytes THRESHOLD_750{1, 3, 2, 2, 0xEE, 0x39, 0x68};
const Timeline::Bytes EXCEPTION{1, 0x83, 2, 0xC0, 0xF1};

void readAndRetain(DeviceModel model) {
  Timeline t;
  CHECK(t.owner.bind(0, model, 1, 115200));
  CHECK(!t.owner.endpointTrusted(0) && !t.owner.settingSnapshot(0, THRESHOLD).observed);
  std::uint64_t first = 0, second = 0;
  CHECK(t.owner.submitSettingRead(0, THRESHOLD, 70, 3, 50000, first));
  t.send(0, THRESHOLD_REQUEST);
  CHECK(t.owner.submitSettingRead(0, THRESHOLD, 71, 4, 50000, second));
  t.drain(695);
  t.reply(5000, THRESHOLD_100);
  const auto seed = t.owner.settingSnapshot(0, THRESHOLD);
  CHECK(t.owner.endpointTrusted(0) && seed.trusted && seed.observed);
  CHECK(seed.observed->value.rawWord == 100 && seed.observed->completedMs == 5);
  CHECK(!t.owner.settingSnapshot(0, Setting::Noise).observed);
  CHECK(t.port.writes.size() == 1 && t.port.reads == 1);
  t.send(5000, THRESHOLD_REQUEST);
  t.drain(5695);
  t.reply(13000, THRESHOLD_750);
  const auto updated = t.owner.settingSnapshot(0, THRESHOLD);
  CHECK(updated.trusted && updated.observed && updated.observed->value.rawWord == 750);
  CHECK(t.owner.health(0).totalSuccess == 2 && t.owner.stats().successCount == 2);

  CHECK(t.owner.bind(0, model, 1, 9600));
  CHECK(!t.owner.endpointTrusted(0) && !t.owner.settingSnapshot(0, THRESHOLD).observed);
  const auto recent = t.collect(second);
  CHECK(recent.status && recent.settingObservation && recent.settingObservation->completedMs == 13);
  CHECK(recent.settingObservation->value.rawWord == 750);
  const auto original = t.collect(first);
  CHECK(original.status && original.attempts == 1 && original.settingObservation);
  CHECK(original.exchange.request.operationId == 70 && original.step == 3);
  CHECK(!original.exchange.request.publishMeasurement);
  const auto& observation = *original.settingObservation;
  CHECK(observation.endpoint.address == 1 && observation.endpoint.bps == 115200);
  CHECK(observation.value.model == model && observation.value.setting == THRESHOLD);
  CHECK(observation.value.known && observation.value.rawWord == 100 &&
        observation.completedMs == 5);
  CHECK(t.port.writes.size() == 2 && t.port.reads == 2);
}

void timeoutAndException(DeviceModel model) {
  Timeline t;
  std::uint64_t ticket = 123;
  CHECK(!t.owner.submitSettingRead(0, THRESHOLD, 72, 0, 10000, ticket));
  CHECK(t.owner.bind(0, model, 1, 115200));
  CHECK(!t.owner.submitSettingRead(0, static_cast<Setting>(0x0207), 72, 0, 10000, ticket));
  CHECK(ticket == 123 && t.port.writes.empty());
  CHECK(t.owner.submitSettingRead(0, THRESHOLD, 72, 0, 10000, ticket));
  t.send(0, THRESHOLD_REQUEST);
  t.drain(695);
  t.poll(10000);
  const auto timeout = t.collect(ticket);
  CHECK(timeout.status.code == Err::TIMEOUT && !timeout.settingObservation);
  CHECK(!t.owner.settled() && !t.owner.endpointTrusted(0));
  CHECK(t.owner.submitSettingRead(0, THRESHOLD, 73, 0, 30000, ticket));
  t.port.deliver(THRESHOLD_100);
  t.poll(12000);
  CHECK(t.port.writes.size() == 1 && t.owner.recover());
  CHECK(!t.owner.endpointTrusted(0) && !t.owner.settingSnapshot(0, THRESHOLD).observed);
  t.send(12000, THRESHOLD_REQUEST);
  t.drain(12695);
  t.reply(17000, EXCEPTION);
  const auto exception = t.collect(ticket);
  CHECK(exception.status.code == Err::EXCEPTION && exception.status.detail == 2);
  CHECK(!exception.settingObservation && t.owner.endpointTrusted(0));
  const auto state = t.owner.settingSnapshot(0, THRESHOLD);
  CHECK(!state.observed && !state.trusted && t.owner.settled());
  CHECK(t.owner.health(0).totalSuccess == 1 && t.owner.health(0).totalFailures == 1);
  CHECK(t.port.writes.size() == 2 && t.owner.stats().retryCount == 0);
}
}  // namespace

int main() {
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    readAndRetain(model);
    timeoutAndException(model);
  }
}
