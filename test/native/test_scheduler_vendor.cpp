#include "SchedulerFixture.h"

namespace {
using namespace SEN059x;
using scheduler_test::Timeline;

constexpr auto VENDOR = WriteScope::VendorSendOnly;
constexpr auto ISOLATED = AddressPolicy::DFRobotIsolated;
constexpr std::uint64_t SESSION = 63;

struct Case {
  Setting setting;
  SettingValue value;
  std::uint16_t initialWord;
  Endpoint intended;
  Timeline::Bytes vendorFrame;
};

// DFRobot table 2, OPERATIONS.csv and the isolated 0xFF destination. These FC06
// bytes include independently calculated CRCs. Each row starts at address 1, 115200 bps.
const Case CASES[]{
    {Setting::Address, {SettingUnit::Address, 5}, 1, {5, 115200}, {255, 6, 2, 0, 0, 5, 0x5D, 0xAF}},
    {Setting::Baud, {SettingUnit::BaudCode, 3}, 9, {1, 9600}, {255, 6, 2, 1, 0, 3, 0x8C, 0x6D}},
    {Setting::SwitchPolarity,
     {SettingUnit::PolarityCode, 0},
     1,
     {1, 115200},
     {255, 6, 2, 5, 0, 0, 0x8D, 0xAD}},
    {Setting::SwitchThreshold,
     {SettingUnit::Millimetres, 750},
     100,
     {1, 115200},
     {255, 6, 2, 6, 2, 0xEE, 0xFC, 0x81}},
    {Setting::Angle,
     {SettingUnit::AngleLevel, 2},
     4,
     {1, 115200},
     {255, 6, 2, 8, 0, 2, 0x9D, 0xAF}},
    {Setting::UartUnit,
     {SettingUnit::UartUnitCode, 1},
     0,
     {1, 115200},
     {255, 6, 2, 9, 0, 1, 0x8C, 0x6E}},
    {Setting::Noise,
     {SettingUnit::NoiseLevel, 4},
     1,
     {1, 115200},
     {255, 6, 2, 0x1A, 0, 4, 0xBD, 0xA8}},
    {Setting::Range,
     {SettingUnit::RangeLevel, 3},
     1,
     {1, 115200},
     {255, 6, 2, 0x1F, 0, 3, 0xEC, 0x6B}},
};

Timeline::Bytes withCrc(Timeline::Bytes bytes) {
  const auto crc = calcCrc16(bytes.data(), bytes.size());
  bytes.push_back(static_cast<std::uint8_t>(crc));
  bytes.push_back(static_cast<std::uint8_t>(crc >> 8));
  return bytes;
}

void runRead(Timeline& t, const Case& entry, Endpoint at, std::uint16_t replyWord) {
  const auto start = t.port.nowUs();
  std::uint64_t ticket = 0;
  CHECK(t.owner.submitSettingRead(0, entry.setting, 30, 1, start + 60000, ticket, at));
  t.send(start, withCrc({at.address, 3, 2, entry.vendorFrame[3], 0, 1}), at.bps);
  t.drain(start + 10000);  // Covers the slowest test rate of 9600.
  t.reply(start + 22000, withCrc({at.address, 3, 2, static_cast<std::uint8_t>(replyWord >> 8),
                                  static_cast<std::uint8_t>(replyWord)}));
  const auto result = t.collect(ticket);
  CHECK(result.status && result.settingObservation && result.attempts == 1);
  CHECK(result.settingObservation->value.rawWord == replyWord);
  CHECK(result.settingObservation->endpoint.address == at.address &&
        result.settingObservation->endpoint.bps == at.bps);
}

void checkVendorWrite(DeviceModel model, const Case& entry) {
  Timeline t;
  CHECK(t.owner.bind(0, model, 1, 115200, ISOLATED));
  runRead(t, entry, {1, 115200}, entry.initialWord);
  const auto reads = t.port.reads;
  ConfigurationOperation plan;
  CHECK(t.owner.previewWrite(0, entry.setting, entry.value, plan, VENDOR));
  CHECK(plan.scope == VENDOR && plan.original.address == 1 && plan.original.bps == 115200);
  CHECK(plan.intended.address == entry.intended.address && plan.intended.bps == entry.intended.bps);
  CHECK(plan.before.trusted && plan.before.observed && plan.before.observed->completedMs == 22);
  CHECK(t.port.reads == reads && t.port.writes.size() == 1);

  std::uint64_t ticket = 0;
  CHECK(t.owner.grantIsolatedMaintenance(0, SESSION));
  CHECK(t.owner.submitWrite(0, entry.setting, entry.value, SESSION, 31, 2, 60000, ticket, VENDOR));
  t.send(22000, entry.vendorFrame);
  const bool movesEndpoint = entry.setting == Setting::Address || entry.setting == Setting::Baud;
  CHECK(t.owner.endpointTrusted(0) == !movesEndpoint);
  t.drain(22700);
  Timeline::Bytes incidental;
  auto origin = AckOrigin::Unknown;
  std::uint64_t quietStart = 22700;
  if (entry.setting != Setting::Address) {
    // A complete, echo-shaped FC06 arrives after DE release.
    incidental = entry.vendorFrame;
    origin = AckOrigin::LocalEcho;
    t.port.deliver(incidental, origin);
    quietStart = 23500;
    t.poll(quietStart);
    if (entry.setting == Setting::SwitchThreshold) {
      // An unsolicited device frame follows the echo. Both must be retired before reuse.
      const auto stray = withCrc({1, 3, 2, 0, 9});
      incidental.insert(incidental.end(), stray.begin(), stray.end());
      t.port.deliver(stray);
      quietStart += 1600;
      t.poll(quietStart);
      origin = AckOrigin::Unknown;
    }
  }
  t.poll(quietStart + 1749);
  CHECK(!t.owner.settled());
  t.poll(quietStart + 1750);
  CHECK(t.owner.settled() && t.port.pending.empty());
  CHECK(t.owner.health(0).totalSuccess == 1 && t.owner.health(0).totalFailures == 0);
  CHECK(t.port.writes.size() == 2 && t.owner.physicalAttempts() == 2);
  const auto history = t.owner.settingSnapshot(0, entry.setting);
  CHECK(!history.trusted && history.observed &&
        history.observed->value.rawWord == entry.initialWord);
  CHECK(history.observed->completedMs == 22 && t.owner.endpointTrusted(0) == !movesEndpoint);

  // Only this separate command probes the intended endpoint. Leave the original send ticket
  // in scheduler storage while the read updates the live binding and reuses the RX buffer.
  runRead(t, entry, entry.intended, entry.value.word);
  CHECK(t.owner.endpointTrusted(0) && t.owner.settingSnapshot(0, entry.setting).trusted);
  const auto original = t.collect(ticket);
  CHECK(original.status && original.attempts == 1 && original.step == 2 && original.write);
  CHECK(!original.settingObservation && original.completedUs == quietStart + 1750);
  const auto& op = *original.write;
  CHECK(op.scope == VENDOR && op.setting == entry.setting && op.desired.word == entry.value.word);
  CHECK(op.tx == TxProgress::Complete && op.txStatus && op.txStatus->ok());
  CHECK(op.primary && op.primary->ok() && op.cleanup && op.cleanup->ok());
  CHECK(!op.ackStatus && !op.verified && !op.readback && !op.readStatus);
  CHECK(op.original.address == 1 && op.original.bps == 115200);
  CHECK(op.intended.address == entry.intended.address && op.intended.bps == entry.intended.bps);
  CHECK(op.before.trusted && op.before.observed && op.before.observed->completedMs == 22 &&
        op.before.observed->value.rawWord == entry.initialWord);
  const auto& wire = original.exchange;
  CHECK(wire.txAccepted == 8 && wire.txDrained && wire.request.context.address == 255);
  CHECK(wire.rxTotal == incidental.size() && wire.origin == origin && !wire.wordCount);
  const auto prefix = std::min(wire.vendorRx.size(), incidental.size());
  CHECK(wire.rxAccessible == prefix && wire.overflow == (prefix < wire.rxTotal));
  CHECK(std::equal(incidental.begin(), incidental.begin() + prefix, wire.vendorRx.begin()));
  CHECK(t.owner.physicalAttempts() == 3 && t.port.writes.size() == 3 &&
        !t.owner.stats().retryCount);
}

void checkStandardRefusal(DeviceModel model) {
  Timeline t;
  const auto& entry = CASES[0];
  CHECK(t.owner.bind(0, model, 1, 115200));  // Default profile.
  runRead(t, entry, {1, 115200}, entry.initialWord);
  ConfigurationOperation plan;
  CHECK(!t.owner.previewWrite(0, entry.setting, entry.value, plan, VENDOR));
  CHECK(t.owner.grantIsolatedMaintenance(0, SESSION));
  std::uint64_t untouched = 99;
  CHECK(!t.owner.submitWrite(0, entry.setting, entry.value, SESSION, 31, 2, 60000, untouched,
                             VENDOR));
  t.poll(22000);
  CHECK(untouched == 99 && t.port.writes.size() == 1 && t.owner.endpointTrusted(0));
  CHECK(t.owner.settingSnapshot(0, entry.setting).trusted);
}

void checkFailedSettlement(DeviceModel model) {
  Timeline t;
  const auto& entry = CASES[0];
  CHECK(t.owner.bind(0, model, 1, 115200, ISOLATED));
  runRead(t, entry, {1, 115200}, entry.initialWord);
  CHECK(t.owner.grantIsolatedMaintenance(0, SESSION));
  std::uint64_t ticket = 0;
  CHECK(t.owner.submitWrite(0, entry.setting, entry.value, SESSION, 31, 2, 23000, ticket, VENDOR));
  t.send(22000, entry.vendorFrame);
  t.drain(22700);
  t.poll(23000);  // Full TX succeeded. Only 300 us of the required 1750 us have elapsed.
  const auto result = t.collect(ticket);
  CHECK(result.attempts == 1 && result.exchange.status.code == Err::TIMEOUT && result.write);
  const auto& op = *result.write;
  CHECK(op.tx == TxProgress::Complete && op.primary && op.primary->ok());
  CHECK(op.cleanup && op.cleanup->code == Err::TIMEOUT && !op.ackStatus && !op.verified);
  CHECK(!t.owner.settled() && !t.owner.endpointTrusted(0));
  t.port.deliver(entry.vendorFrame, AckOrigin::Device);
  CHECK(!t.owner.recover() && !t.owner.recoverIsolated());
  t.port.isolationVerified = true;
  CHECK(t.owner.recoverIsolated() && t.port.pending.empty());
  CHECK(t.owner.health(0).totalSuccess == 1 && t.owner.health(0).totalFailures == 0);
  CHECK(t.owner.physicalAttempts() == 2 && t.port.writes.size() == 2 &&
        !t.owner.stats().retryCount);
}
}  // namespace

int main() {
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    checkStandardRefusal(model);
    checkFailedSettlement(model);
    for (const auto& entry : CASES) {
      checkVendorWrite(model, entry);
    }
  }
}
