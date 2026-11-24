#include "SchedulerFixture.h"

namespace {
using namespace SEN059x;
using scheduler_test::Timeline;

constexpr auto THRESHOLD = Setting::SwitchThreshold;
// DFRobot table 2 and OPERATIONS.csv: single-word FC03 at 0x0206, threshold in mm.
const Timeline::Bytes THRESHOLD_REQUEST{1, 3, 2, 6, 0, 1, 0x65, 0xB3};
const Timeline::Bytes THRESHOLD_100{1, 3, 2, 0, 0x64, 0xB9, 0xAF};
const Timeline::Bytes THRESHOLD_750{1, 3, 2, 2, 0xEE, 0x39, 0x68};
const Timeline::Bytes WRITE_750{1, 6, 2, 6, 2, 0xEE, 0xE9, 0x5F};
constexpr SettingValue DESIRED{SettingUnit::Millimetres, 750};
const Timeline::Bytes EXCEPTION{1, 0x83, 2, 0xC0, 0xF1};

struct WriteCase {
  Setting setting;
  SettingValue desired;
  std::uint16_t oldWord;
  Timeline::Bytes readRequest, oldReply, writeRequest, newReply;
};

// The six endpoint-preserving FC06 rows in OPERATIONS.csv, with independent RTU CRCs.
const WriteCase THRESHOLD_CASE{THRESHOLD,     DESIRED,   100,          THRESHOLD_REQUEST,
                               THRESHOLD_100, WRITE_750, THRESHOLD_750};
const Timeline::Bytes WORD_0{1, 3, 2, 0, 0, 0xB8, 0x44};
const Timeline::Bytes WORD_1{1, 3, 2, 0, 1, 0x79, 0x84};
const Timeline::Bytes WORD_4{1, 3, 2, 0, 4, 0xB9, 0x87};
const WriteCase OTHER_CASES[]{
    {Setting::SwitchPolarity,
     {SettingUnit::PolarityCode, 0},
     1,
     {1, 3, 2, 5, 0, 1, 0x95, 0xB3},
     WORD_1,
     {1, 6, 2, 5, 0, 0, 0x98, 0x73},
     WORD_0},
    {Setting::Angle,
     {SettingUnit::AngleLevel, 2},
     4,
     {1, 3, 2, 8, 0, 1, 0x04, 0x70},
     WORD_4,
     {1, 6, 2, 8, 0, 2, 0x88, 0x71},
     {1, 3, 2, 0, 2, 0x39, 0x85}},
    {Setting::UartUnit,
     {SettingUnit::UartUnitCode, 1},
     0,
     {1, 3, 2, 9, 0, 1, 0x55, 0xB0},
     WORD_0,
     {1, 6, 2, 9, 0, 1, 0x99, 0xB0},
     WORD_1},
    {Setting::Noise,
     {SettingUnit::NoiseLevel, 4},
     1,
     {1, 3, 2, 0x1A, 0, 1, 0xA4, 0x75},
     WORD_1,
     {1, 6, 2, 0x1A, 0, 4, 0xA8, 0x76},
     WORD_4},
    {Setting::Range,
     {SettingUnit::RangeLevel, 3},
     1,
     {1, 3, 2, 0x1F, 0, 1, 0xB4, 0x74},
     WORD_1,
     {1, 6, 2, 0x1F, 0, 3, 0xF9, 0xB5},
     {1, 3, 2, 0, 3, 0xF8, 0x45}},
};

void previewAddressedSettings(DeviceModel model) {
  Timeline t;
  CHECK(t.owner.bind(0, model, 17, 38400));
  // FC06 cases in OPERATIONS.csv. Only the address and baud words change the readback endpoint.
  struct Case {
    Setting setting;
    SettingValue value;
    Endpoint intended;
  };
  const Case cases[]{
      {Setting::Address, {SettingUnit::Address, 5}, {5, 38400}},
      {Setting::Baud, {SettingUnit::BaudCode, 3}, {17, 9600}},
      {Setting::SwitchPolarity, {SettingUnit::PolarityCode, 0}, {17, 38400}},
      {THRESHOLD, {SettingUnit::Millimetres, 750}, {17, 38400}},
      {Setting::Angle, {SettingUnit::AngleLevel, 2}, {17, 38400}},
      {Setting::UartUnit, {SettingUnit::UartUnitCode, 1}, {17, 38400}},
      {Setting::Noise, {SettingUnit::NoiseLevel, 4}, {17, 38400}},
      {Setting::Range, {SettingUnit::RangeLevel, 3}, {17, 38400}},
  };
  ConfigurationOperation plan;
  for (const auto& entry : cases) {
    CHECK(t.owner.previewWrite(0, entry.setting, entry.value, plan));
    CHECK(plan.scope == WriteScope::Addressed && plan.setting == entry.setting);
    CHECK(plan.desired.unit == entry.value.unit && plan.desired.word == entry.value.word);
    CHECK(plan.original.address == 17 && plan.original.bps == 38400);
    CHECK(plan.intended.address == entry.intended.address &&
          plan.intended.bps == entry.intended.bps);
    CHECK(!plan.before.observed && !plan.before.trusted && plan.tx == TxProgress::Zero);
    CHECK(!plan.txStatus && !plan.ackStatus && !plan.readback && !plan.readStatus);
    CHECK(!plan.primary && !plan.cleanup && !plan.verified);
  }
  CHECK(!t.owner.previewWrite(1, THRESHOLD, {SettingUnit::Millimetres, 750}, plan));
  CHECK(t.owner.previewWrite(0, THRESHOLD, {SettingUnit::BaudCode, 3}, plan).code ==
        Err::INVALID_CONFIG);
  CHECK(t.owner.previewWrite(0, Setting::Noise, {SettingUnit::NoiseLevel, 99}, plan).code ==
        Err::ILLEGAL_VALUE);
  CHECK(t.owner.previewWrite(0, Setting::Address, {SettingUnit::Address, 248}, plan).code ==
        Err::DISALLOWED);
  CHECK(plan.setting == Setting::Range && plan.desired.word == 3 && plan.intended.address == 17);
  t.poll(0);
  CHECK(t.port.writes.empty() && t.port.reads == 0 && t.port.baud == 0 && !t.port.transmitting);
  CHECK(!t.owner.endpointTrusted(0) && t.owner.physicalAttempts() == 0);
}

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
  ConfigurationOperation plan;
  CHECK(t.owner.previewWrite(0, THRESHOLD, {SettingUnit::Millimetres, 750}, plan));
  CHECK(plan.before.trusted && plan.before.observed && plan.before.observed->value.rawWord == 100);
  CHECK(plan.before.observed->completedMs == 5 && !plan.readback && !plan.verified);
  CHECK(t.owner.endpointTrusted(0) && t.owner.settingSnapshot(0, THRESHOLD).trusted);
  CHECK(t.port.writes.size() == 2 && t.port.reads == 1);
  t.reply(13000, THRESHOLD_750);
  const auto updated = t.owner.settingSnapshot(0, THRESHOLD);
  CHECK(updated.trusted && updated.observed && updated.observed->value.rawWord == 750);
  CHECK(plan.before.observed->value.rawWord == 100 && plan.before.observed->completedMs == 5);
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

void seedSetting(Timeline& t, const WriteCase& entry) {
  const auto start = t.port.nowUs();
  std::uint64_t ticket = 0;
  CHECK(t.owner.submitSettingRead(0, entry.setting, 80, 0, start + 10000, ticket));
  t.send(start, entry.readRequest);
  t.drain(start + 695);
  t.reply(start + 5000, entry.oldReply);
  CHECK(t.collect(ticket).status && t.owner.endpointTrusted(0));
}

void expectRefused(Timeline& t, std::uint64_t session, const WriteCase& entry = THRESHOLD_CASE) {
  const auto writes = t.port.writes.size();
  std::uint64_t ticket = 12345;
  const auto status = t.owner.submitWrite(0, entry.setting, entry.desired, session, 81, 1,
                                          t.port.nowUs() + 30000, ticket);
  CHECK(status.code == Err::DISALLOWED);
  t.poll(t.port.nowUs());
  CHECK(ticket == 12345 && t.port.writes.size() == writes);
}

void settleLateThresholdAck(DeviceModel model) {
  Timeline t;
  CHECK(t.owner.bind(0, model, 1, 115200));
  seedSetting(t, THRESHOLD_CASE);
  constexpr std::uint64_t session = 12;
  std::uint64_t ticket = 0;
  CHECK(t.owner.grantIsolatedMaintenance(0, session));
  CHECK(t.owner.submitWrite(0, THRESHOLD, DESIRED, session, 84, 0, 15000, ticket));
  t.send(5000, WRITE_750);
  CHECK(!t.owner.settingSnapshot(0, THRESHOLD).trusted);
  t.drain(5695);
  t.poll(15000);
  CHECK(!t.owner.settled() && t.owner.grantIsolatedMaintenance(0, session));
  expectRefused(t, session);
  t.port.deliver(WRITE_750, AckOrigin::LocalEcho);
  CHECK(!t.owner.recover());
  t.port.deliver(WRITE_750);
  CHECK(t.owner.recover());
  const auto result = t.collect(ticket);
  CHECK(result.status.code == Err::TIMEOUT && result.attempts == 1 && result.write);
  const auto& write = *result.write;
  CHECK(write.tx == TxProgress::Complete && write.ackStatus &&
        write.ackStatus->code == Err::TIMEOUT);
  CHECK(write.primary && write.primary->code == Err::TIMEOUT);
  CHECK(write.cleanup && write.cleanup->code == Err::BUS_BUSY && !write.verified &&
        !write.readback);
  const auto state = t.owner.settingSnapshot(0, THRESHOLD);
  CHECK(t.owner.endpointTrusted(0) && !state.trusted && state.observed);
  CHECK(state.observed->value.rawWord == 100 && state.observed->completedMs == 5);
  CHECK(t.port.writes.size() == 2 && t.owner.stats().retryCount == 0);
}

void maintenanceRevocation(DeviceModel model) {
  Timeline t;
  constexpr std::uint64_t session = 10;
  CHECK(t.owner.bind(0, model, 1, 115200));
  t.owner.grantIsolatedMaintenance(0, session);
  expectRefused(t, session);  // No addressed observation at this endpoint.
  t.owner.maintenanceOff();
  seedSetting(t, THRESHOLD_CASE);
  expectRefused(t, session);

  CHECK(t.owner.grantIsolatedMaintenance(0, session));
  t.owner.maintenanceOff();
  expectRefused(t, session);
  CHECK(t.owner.grantIsolatedMaintenance(0, session));
  t.owner.sessionActivity(session + 1);
  expectRefused(t, session + 1);
  t.owner.sessionActivity(session);
  expectRefused(t, session);
  CHECK(t.owner.grantIsolatedMaintenance(0, session));
  t.poll(t.port.nowUs() + 60000000);
  t.owner.sessionActivity(session);  // An expired grant cannot be revived by activity.
  expectRefused(t, session);

  CHECK(t.owner.grantIsolatedMaintenance(0, session));
  CHECK(t.owner.bind(0, model, 1, 115200));  // New generation, identical endpoint.
  seedSetting(t, THRESHOLD_CASE);
  expectRefused(t, session);
  CHECK(t.owner.grantIsolatedMaintenance(0, session));
  std::uint64_t cancelled = 0;
  CHECK(t.owner.submitWrite(0, THRESHOLD, DESIRED, session, 83, 0, t.port.nowUs() + 30000,
                            cancelled));
  CHECK(t.owner.cancel(cancelled));
  CHECK(t.collect(cancelled).status.code == Err::ABORTED);
  expectRefused(t, session);  // Cancelling before TX still consumes the admitted write's grant.
  CHECK(t.port.writes.size() == 2 && t.owner.physicalAttempts() == 2);
}

void checkModbusDistance(Timeline& t) {
  // Replay DFRobot's 754 mm processed-distance example at each UART-unit setting.
  const auto start = t.port.nowUs();
  const auto ticket = t.submit(0, MeasurementKind::ProcessedDistance, 85, 0, start + 1000000);
  t.send(start, {1, 3, 1, 0, 0, 1, 0x85, 0xF6});
  t.drain(start + 695);
  t.reply(start + 400000, {1, 3, 2, 2, 0xF2, 0x38, 0xA1});
  const auto result = t.collect(ticket);
  CHECK(result.status && result.measurement.unit == MeasurementUnit::Millimetres);
  CHECK(result.measurement.rawWord == 754 && result.measurement.value == 754);
}

void confirmSetting(DeviceModel model, const WriteCase& entry) {
  Timeline t;
  constexpr std::uint64_t session = 11;
  CHECK(t.owner.bind(0, model, 1, 115200));
  seedSetting(t, entry);
  ConfigurationOperation preview;
  CHECK(t.owner.previewWrite(0, entry.setting, entry.desired, preview));
  CHECK(preview.before.trusted && preview.before.observed &&
        preview.before.observed->value.rawWord == entry.oldWord);
  CHECK(t.port.writes.size() == 1 && t.port.reads == 1);
  if (entry.setting == Setting::UartUnit) {
    checkModbusDistance(t);
  }
  const auto previousAttempts = t.owner.physicalAttempts();
  CHECK(t.owner.grantIsolatedMaintenance(0, session));
  if (entry.setting == Setting::Range) {
    const std::uint16_t invalidLevel = model == DeviceModel::SEN0591 ? 5 : 6;
    std::uint64_t refused = 12345;
    CHECK(t.owner
              .submitWrite(0, entry.setting, {SettingUnit::RangeLevel, invalidLevel}, session, 81,
                           1, 30000, refused)
              .code == Err::ILLEGAL_VALUE);
    t.poll(t.port.nowUs());
    CHECK(refused == 12345 && t.port.writes.size() == previousAttempts && t.port.reads == 1);
    const auto state = t.owner.settingSnapshot(0, entry.setting);
    CHECK(state.trusted && state.observed && state.observed->value.rawWord == entry.oldWord);
    CHECK(state.observed->completedMs == 5 && t.owner.endpointTrusted(0));
  }
  t.poll(59005000);
  t.owner.sessionActivity(session);
  const std::uint64_t start = 61005000;
  t.poll(start);  // More than 60 s since grant, only 2 s since same-session activity.
  std::uint64_t ticket = 0;
  CHECK(
      t.owner.submitWrite(0, entry.setting, entry.desired, session, 82, 2, start + 30000, ticket));
  CHECK(t.port.writes.size() == previousAttempts);
  t.send(start, entry.writeRequest);
  t.drain(start + 695);
  const auto historical = t.owner.settingSnapshot(0, entry.setting);
  CHECK(!historical.trusted && historical.observed &&
        historical.observed->value.rawWord == entry.oldWord);
  CHECK(historical.observed->completedMs == 5 && t.owner.endpointTrusted(0));
  t.reply(start + 5000, entry.writeRequest);
  CHECK(!t.owner.settingSnapshot(0, entry.setting).trusted);
  t.send(start + 5000, entry.readRequest);
  t.drain(start + 5695);
  t.reply(start + 10000, entry.newReply);
  const auto current = t.owner.settingSnapshot(0, entry.setting);
  CHECK(current.trusted && current.observed &&
        current.observed->value.rawWord == entry.desired.word);
  CHECK(current.observed->completedMs == 61015);
  expectRefused(t, session, entry);  // One admission consumed the grant.
  CHECK(t.port.writes.size() == previousAttempts + 2 &&
        t.owner.physicalAttempts() == previousAttempts + 2);
  CHECK(t.owner.stats().retryCount == 0 && t.owner.settled());

  if (entry.setting == Setting::UartUnit) {
    checkModbusDistance(t);
  }
  CHECK(t.owner.bind(0, model, 1, 9600));
  const auto result = t.collect(ticket);
  CHECK(result.status && result.step == 2 && result.attempts == 2 && result.write);
  const auto& write = *result.write;
  CHECK(write.setting == entry.setting && write.desired.word == entry.desired.word &&
        write.desired.unit == entry.desired.unit);
  CHECK(write.scope == WriteScope::Addressed && write.verified);
  CHECK(write.original.address == 1 && write.original.bps == 115200);
  CHECK(write.intended.address == 1 && write.intended.bps == 115200);
  CHECK(write.before.trusted && write.before.observed &&
        write.before.observed->value.rawWord == entry.oldWord);
  CHECK(write.before.observed->completedMs == 5);
  CHECK(write.tx == TxProgress::Complete && write.txStatus && write.txStatus->ok());
  CHECK(write.ackStatus && write.ackStatus->ok() && write.ackOrigin == AckOrigin::Device);
  CHECK(write.readStatus && write.readStatus->ok() && write.readback);
  CHECK(write.readback->value.rawWord == entry.desired.word &&
        write.readback->completedMs == 61015);
  CHECK(write.readback->value.model == model && write.readback->value.setting == entry.setting);
  CHECK(write.primary && write.primary->ok() && write.cleanup && write.cleanup->ok());
}
}  // namespace

int main() {
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    previewAddressedSettings(model);
    readAndRetain(model);
    timeoutAndException(model);
    maintenanceRevocation(model);
    confirmSetting(model, THRESHOLD_CASE);
    for (const auto& entry : OTHER_CASES) {
      confirmSetting(model, entry);
    }
    settleLateThresholdAck(model);
  }
}
