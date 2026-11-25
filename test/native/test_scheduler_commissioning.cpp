#include "SchedulerFixture.h"

namespace {
using namespace SEN059x;
using namespace SEN059x::example;
using scheduler_test::Timeline;
using Bytes = Timeline::Bytes;

constexpr auto SESSION = 17;
constexpr auto ISOLATED = AddressPolicy::DFRobotIsolated;
constexpr auto STANDARD = AddressPolicy::StandardUnicast;

struct Transition {
  Setting setting;
  SettingValue desired;
  Endpoint from, to;
  std::uint16_t oldWord;
  Bytes seedRequest, oldReply, write, verification, newReply;
  Bytes temperatureRequest, temperatureReply;
};

// DFRobot tables 1/2, Chinese examples 4-6 and OPERATIONS.csv. Extended-address
// requests and their CRCs are independent synthetic cases at the policy boundaries.
const Bytes TEMP_REQUEST{1, 3, 1, 2, 0, 1, 0x24, 0x36};
const Bytes TEMP_REPLY{1, 3, 2, 1, 0x2C, 0xB8, 0x09};
const Bytes ADDRESS_REQUEST{1, 3, 2, 0, 0, 1, 0x85, 0xB2};
const Bytes WORD_ONE{1, 3, 2, 0, 1, 0x79, 0x84};
const Bytes BAUD_REQUEST{1, 3, 2, 1, 0, 1, 0xD4, 0x72};
const Transition ADDRESS{Setting::Address,
                         {SettingUnit::Address, 5},
                         {1, 115200},
                         {5, 115200},
                         1,
                         ADDRESS_REQUEST,
                         WORD_ONE,
                         {1, 6, 2, 0, 0, 5, 0x48, 0x71},
                         {5, 3, 2, 0, 0, 1, 0x84, 0x36},
                         {5, 3, 2, 0, 5, 0x89, 0x87},
                         {5, 3, 1, 2, 0, 1, 0x25, 0xB2},
                         {5, 3, 2, 1, 0x2C, 0x49, 0xC9}};
const Transition BAUD{Setting::Baud,
                      {SettingUnit::BaudCode, 3},
                      {1, 115200},
                      {1, 9600},
                      9,
                      BAUD_REQUEST,
                      {1, 3, 2, 0, 9, 0x78, 0x42},
                      {1, 6, 2, 1, 0, 3, 0x99, 0xB3},
                      BAUD_REQUEST,
                      {1, 3, 2, 0, 3, 0xF8, 0x45},
                      TEMP_REQUEST,
                      TEMP_REPLY};
const Transition EXTEND{Setting::Address,
                        {SettingUnit::Address, 248},
                        {247, 115200},
                        {248, 115200},
                        247,
                        {247, 3, 2, 0, 0, 1, 0x91, 0x24},
                        {247, 3, 2, 0, 247, 0x31, 0xD7},
                        {247, 6, 2, 0, 0, 248, 0x9D, 0x66},
                        {248, 3, 2, 0, 0, 1, 0x91, 0xDB},
                        {248, 3, 2, 0, 248, 0x25, 0xD2},
                        {248, 3, 1, 2, 0, 1, 0x30, 0x5F},
                        {248, 3, 2, 1, 0x2C, 0x24, 0x1D}};
const Transition RESTORE{Setting::Address,
                         {SettingUnit::Address, 1},
                         {254, 115200},
                         {1, 115200},
                         254,
                         {254, 3, 2, 0, 0, 1, 0x91, 0xBD},
                         {254, 3, 2, 0, 254, 0x2D, 0xD0},
                         {254, 6, 2, 0, 0, 1, 0x5D, 0xBD},
                         ADDRESS_REQUEST,
                         WORD_ONE,
                         TEMP_REQUEST,
                         TEMP_REPLY};

DeviceModel otherModel(DeviceModel model) {
  return model == DeviceModel::SEN0591 ? DeviceModel::SEN0592 : DeviceModel::SEN0591;
}

ScheduledResult read(Timeline& t, std::size_t slot, Setting setting, const Bytes& request,
                     const Bytes& reply, std::uint32_t baud = 115200,
                     std::optional<Endpoint> at = std::nullopt) {
  const auto start = t.port.time;
  std::uint64_t ticket = 0;
  CHECK(t.owner.submitSettingRead(slot, setting, 90, 1, start + 100000, ticket, at));
  t.send(start, request, baud);
  t.drain(start + 10000);
  t.reply(start + 20000, reply);
  const auto result = t.collect(ticket);
  CHECK(result.status && result.settingObservation && result.attempts == 1);
  return result;
}

void seed(Timeline& t, DeviceModel model, const Transition& entry,
          AddressPolicy policy = STANDARD) {
  CHECK(t.owner.bind(0, model, entry.from.address, entry.from.bps, policy));
  const auto result = read(t, 0, entry.setting, entry.seedRequest, entry.oldReply, entry.from.bps);
  CHECK(result.settingObservation->value.rawWord == entry.oldWord && t.owner.endpointTrusted(0));
}

std::uint64_t confirm(Timeline& t, const Transition& entry) {
  std::uint64_t ticket = 0;
  CHECK(t.owner.grantIsolatedMaintenance(0, SESSION));
  CHECK(t.owner.submitWrite(0, entry.setting, entry.desired, SESSION, 91, 2, t.port.time + 100000,
                            ticket));
  return ticket;
}

void historical(Timeline& t, const Transition& entry, std::uint32_t seedMs) {
  const auto old = t.owner.settingSnapshot(0, entry.setting);
  const auto current = t.owner.endpoint(0);
  const auto candidate = t.owner.possibleEndpoint(0);
  CHECK(current && current->address == entry.from.address && current->bps == entry.from.bps);
  CHECK(candidate && candidate->address == entry.to.address && candidate->bps == entry.to.bps);
  CHECK(!t.owner.endpointTrusted(0) && !old.trusted && old.observed);
  CHECK(old.observed->value.rawWord == entry.oldWord && old.observed->completedMs == seedMs);
  CHECK(old.observed->endpoint.address == entry.from.address &&
        old.observed->endpoint.bps == entry.from.bps);
}

void refused(Timeline& t, Setting setting, SettingValue value, Err expected) {
  const auto writes = t.port.writes.size();
  const auto reads = t.port.reads;
  std::uint64_t ticket = 999;
  const auto status =
      t.owner.submitWrite(0, setting, value, SESSION, 91, 2, t.port.time + 100000, ticket);
  CHECK(status.code == expected);
  t.poll(t.port.time);
  CHECK(ticket == 999 && t.port.writes.size() == writes && t.port.reads == reads);
}

void changeEndpoint(DeviceModel model, const Transition& entry, AddressPolicy policy) {
  Timeline t;
  seed(t, model, entry, policy);
  CHECK(t.owner.bind(1, otherModel(model), 2, 115200));
  read(t, 1, Setting::Address, {2, 3, 2, 0, 0, 1, 0x85, 0x81}, {2, 3, 2, 0, 2, 0x7D, 0x85});
  CHECK(t.owner.grantIsolatedMaintenance(0, SESSION));
  if (entry.setting == Setting::Address) {
    refused(t, entry.setting, {SettingUnit::Address, 2}, Err::BUS_BUSY);
    refused(t, entry.setting, {SettingUnit::Address, 0}, Err::ILLEGAL_VALUE);
    refused(t, entry.setting, {SettingUnit::Address, 255}, Err::ILLEGAL_VALUE);
  } else {
    t.port.unsupportedBaud = entry.to.bps;
    refused(t, entry.setting, entry.desired, Err::UNSUPPORTED);
    t.port.unsupportedBaud = 0;
  }
  const auto start = t.port.time;
  ConfigurationOperation plan;
  CHECK(t.owner.previewWrite(0, entry.setting, entry.desired, plan));
  CHECK(plan.before.trusted && plan.before.observed &&
        plan.before.observed->value.rawWord == entry.oldWord);
  CHECK(t.port.writes.size() == 2 && t.port.reads == 2);
  std::uint64_t ticket = 0;
  // Static rejection preserved this grant.
  CHECK(
      t.owner.submitWrite(0, entry.setting, entry.desired, SESSION, 91, 2, start + 100000, ticket));
  CHECK(t.owner.bind(2, otherModel(model), entry.to.address, entry.to.bps, policy).code ==
        Err::BUS_BUSY);
  const auto queued = t.submit(0, MeasurementKind::Temperature, 97, 0, start + 100000);
  t.send(start, entry.write, entry.from.bps);
  historical(t, entry, 20);
  t.drain(start + 1000);
  t.reply(start + 5000, entry.write);  // The simulated device ACKs on the old link.
  historical(t, entry, 20);
  t.send(start + 5000, entry.verification, entry.to.bps);
  t.drain(start + 15000);
  // A maintenance declaration made while this endpoint is still unconfirmed cannot authorize
  // a later write after readback identifies the responding endpoint.
  CHECK(t.owner.grantIsolatedMaintenance(0, SESSION));
  t.reply(start + 25000, entry.newReply);
  CHECK(t.owner.endpointTrusted(0));
  const auto current = t.owner.endpoint(0);
  CHECK(current && current->address == entry.to.address && current->bps == entry.to.bps);
  CHECK(!t.owner.possibleEndpoint(0));
  const auto now = t.owner.settingSnapshot(0, entry.setting);
  CHECK(now.trusted && now.observed && now.observed->value.rawWord == entry.desired.word);
  CHECK(now.observed->endpoint.address == entry.to.address &&
        now.observed->endpoint.bps == entry.to.bps);
  CHECK(now.observed->completedMs == 65 && t.owner.settled());
  t.poll(t.port.time);
  const auto stale = t.collect(queued);
  CHECK(stale.status.code == Err::ABORTED && stale.attempts == 0);
  refused(t, entry.setting, entry.desired, Err::DISALLOWED);

  // A normal measurement must now go to the verified endpoint and publish on this binding.
  const auto next = t.submit(0, MeasurementKind::Temperature, 92, 0);
  t.send(t.port.time, entry.temperatureRequest, entry.to.bps);
  t.drain(t.port.time + 10000);
  t.reply(t.port.time + 50000, entry.temperatureReply);
  const auto temperature = t.collect(next);
  CHECK(temperature.status && temperature.measurement.model == model &&
        temperature.measurement.address == entry.to.address && temperature.measurement.value == 30);
  const auto peer = t.owner.settingSnapshot(1, Setting::Address);
  CHECK(t.owner.endpointTrusted(1) && peer.trusted && peer.observed);
  CHECK(peer.observed->value.rawWord == 2 && peer.observed->completedMs == 40);
  CHECK(t.owner.health(1).totalSuccess == 1 && t.owner.health(1).totalFailures == 0);
  CHECK(t.owner.stats().retryCount == 0 && t.port.writes.size() == 5);

  const auto result = t.collect(ticket);
  CHECK(result.status && result.attempts == 2 && result.write);
  const auto& op = *result.write;
  CHECK(op.original.address == entry.from.address && op.original.bps == entry.from.bps);
  CHECK(op.intended.address == entry.to.address && op.intended.bps == entry.to.bps);
  CHECK(op.before.trusted && op.before.observed && op.before.observed->completedMs == 20);
  CHECK(op.tx == TxProgress::Complete && op.txStatus && op.txStatus->ok());
  CHECK(op.ackStatus && op.ackStatus->ok() && op.ackOrigin == AckOrigin::Device);
  CHECK(op.verified && op.primary && op.primary->ok() && op.cleanup && op.cleanup->ok());
  CHECK(op.readStatus && op.readStatus->ok() && op.readback && op.readback->completedMs == 65);
  CHECK(op.readback->value.rawWord == entry.desired.word &&
        op.readback->value.setting == entry.setting);
}

void standardPolicy(DeviceModel model) {
  Timeline t;
  CHECK(!t.owner.bind(0, model, 254, 115200));
  seed(t, model, EXTEND);
  CHECK(t.owner.grantIsolatedMaintenance(0, SESSION));
  refused(t, EXTEND.setting, EXTEND.desired, Err::DISALLOWED);
  CHECK(t.owner.endpointTrusted(0) && t.owner.settingSnapshot(0, EXTEND.setting).trusted);
}

void lostAddressAck(DeviceModel model) {
  Timeline t;
  seed(t, model, ADDRESS);
  const auto original = confirm(t, ADDRESS);
  t.send(20000, ADDRESS.write);
  t.drain(21000);
  t.poll(120000);
  CHECK(!t.owner.settled());
  historical(t, ADDRESS, 20);
  t.port.deliver(ADDRESS.write, AckOrigin::LocalEcho);
  CHECK(!t.owner.recover() && !t.owner.recoverIsolated());
  t.port.isolationVerified = true;
  CHECK(t.owner.recoverIsolated());
  CHECK(t.poll(125000) == UINT64_MAX && t.port.writes.size() == 2);
  historical(t, ADDRESS, 20);
  CHECK(t.owner.bind(1, otherModel(model), ADDRESS.to.address, 115200).code == Err::BUS_BUSY);
  std::uint64_t refusedTicket = 999;
  CHECK(!t.owner.submitSettingRead(0, Setting::Address, 93, 0, 225000, refusedTicket,
                                   Endpoint{6, 115200}));
  CHECK(refusedTicket == 999 && t.port.writes.size() == 2);
  const auto fresh = read(t, 0, Setting::Address, ADDRESS.verification, ADDRESS.newReply,
                          ADDRESS.to.bps, ADDRESS.to);
  CHECK(fresh.settingObservation->completedMs == 145 &&
        fresh.settingObservation->value.rawWord == 5);
  CHECK(t.owner.endpointTrusted(0) && t.owner.settingSnapshot(0, Setting::Address).trusted);
  CHECK(t.owner.endpoint(0) && t.owner.endpoint(0)->address == 5 && !t.owner.possibleEndpoint(0));
  const auto result = t.collect(original);
  CHECK(!result.status && result.attempts == 1 && result.write && !result.write->verified);
  CHECK(result.write->tx == TxProgress::Complete && result.write->ackStatus &&
        result.write->ackStatus->code == Err::TIMEOUT && !result.write->readback);
  CHECK(result.write->primary && !result.write->primary->ok() && result.write->cleanup &&
        result.write->cleanup->code == Err::BUS_BUSY && result.completedUs == 120000);
  CHECK(t.port.writes.size() == 3 && t.owner.stats().retryCount == 0);
}

void mismatch(DeviceModel model) {
  Timeline t;
  seed(t, model, BAUD);
  const auto ticket = confirm(t, BAUD);
  t.send(20000, BAUD.write);
  t.drain(21000);
  t.reply(25000, BAUD.write);
  t.send(25000, BAUD.verification, 9600);
  t.drain(35000);
  t.reply(55000, BAUD.oldReply);  // Device answers at 9600 but reports code 9.
  const auto result = t.collect(ticket);
  CHECK(result.status.code == Err::CONFIG_MISMATCH && result.status.detail == 9 && result.write);
  const auto& op = *result.write;
  CHECK(!op.verified && op.readStatus && op.readStatus->ok() && op.readback);
  CHECK(op.primary && op.primary->code == Err::CONFIG_MISMATCH && op.cleanup && op.cleanup->ok());
  CHECK(op.readback->endpoint.bps == 9600 && op.readback->value.rawWord == 9);
  const auto observed = t.owner.settingSnapshot(0, Setting::Baud);
  CHECK(observed.trusted && observed.observed && observed.observed->completedMs == 55);
  CHECK(t.owner.endpointTrusted(0) && t.owner.settled() && t.port.writes.size() == 3);
}

void wrongEcho(DeviceModel model) {
  Timeline t;
  seed(t, model, ADDRESS);
  const auto ticket = confirm(t, ADDRESS);
  t.send(20000, ADDRESS.write);
  t.drain(21000);
  t.reply(25000, {1, 6, 2, 0, 0, 6, 0x08, 0x70});  // Good CRC, wrong echoed word.
  const auto result = t.collect(ticket);
  CHECK(!result.status && result.attempts == 1 && result.write);
  CHECK(result.write->ackStatus && result.write->ackStatus->code == Err::FRAME_ERROR);
  CHECK(result.write->ackError == WriteResponseError::VALUE_MISMATCH && !result.write->readback);
  historical(t, ADDRESS, 20);
  CHECK(!t.owner.settled() && t.port.writes.size() == 2 && t.owner.stats().retryCount == 0);
}

void writeExpiresBeforeTx(DeviceModel model, WriteScope scope) {
  Timeline t;
  seed(t, model, BAUD, scope == WriteScope::VendorSendOnly ? ISOLATED : STANDARD);
  CHECK(t.owner.bind(1, otherModel(model), 2, 115200));
  CHECK(t.owner.grantIsolatedMaintenance(0, SESSION));
  std::uint64_t write = 0;
  CHECK(t.owner.submitWrite(0, BAUD.setting, BAUD.desired, SESSION, 91, 2, 25000, write, scope));
  t.poll(25000);  // First service after admission, exactly at the exclusive deadline.
  CHECK(t.owner.settled() && !t.port.transmitting && t.port.baud == BAUD.from.bps);
  CHECK(t.port.writes.size() == 1 && t.port.reads == 1 && t.owner.physicalAttempts() == 1);
  CHECK(t.owner.endpointTrusted(0) && !t.owner.possibleEndpoint(0));
  const auto state = t.owner.settingSnapshot(0, BAUD.setting);
  CHECK(state.trusted && state.observed && state.observed->value.rawWord == BAUD.oldWord &&
        state.observed->completedMs == 20);
  refused(t, BAUD.setting, BAUD.desired, Err::DISALLOWED);  // Admission consumed the grant.

  read(t, 1, Setting::Address, {2, 3, 2, 0, 0, 1, 0x85, 0x81}, {2, 3, 2, 0, 2, 0x7D, 0x85});
  const auto result = t.collect(write);
  CHECK(result.queueExpired && result.status.code == Err::TIMEOUT && result.completedUs == 25000);
  CHECK(result.attempts == 0 && result.exchange.txAccepted == 0 && result.write);
  const auto& op = *result.write;
  CHECK(op.scope == scope && op.tx == TxProgress::Zero && op.txStatus &&
        op.txStatus->code == Err::TIMEOUT && op.primary && op.primary->code == Err::TIMEOUT);
  CHECK(op.cleanup && op.cleanup->ok() && !op.ackStatus && !op.readStatus && !op.verified);
  CHECK(t.owner.health(0).totalSuccess == 1 && t.owner.health(0).totalFailures == 0);
  CHECK(t.port.writes.size() == 2 && t.owner.stats().timeouts == 0 &&
        t.owner.stats().failCount == 1 && t.owner.stats().successCount == 2);
}

void silentNewBaud(DeviceModel model) {
  Timeline t;
  seed(t, model, BAUD);
  CHECK(t.owner.bind(1, otherModel(model), 2, 115200));
  const auto ticket = confirm(t, BAUD);
  t.send(20000, BAUD.write);
  std::uint64_t peerTicket = 0;
  CHECK(t.owner.submitSettingRead(1, Setting::Address, 92, 0, 250000, peerTicket));
  t.drain(21000);
  t.reply(25000, BAUD.write);
  t.send(25000, BAUD.verification, 9600);
  t.drain(35000);
  t.poll(120000);
  historical(t, BAUD, 20);
  CHECK(!t.owner.settled() && !t.owner.recover());
  t.poll(150000);  // The peer still has time, but silence cannot settle the retired FC03.
  ScheduledResult pending;
  CHECK(!t.owner.take(peerTicket, pending));
  CHECK(!t.owner.settingSnapshot(1, Setting::Address).observed);
  CHECK(t.port.baud == 9600 && t.port.writes.size() == 3 && t.owner.physicalAttempts() == 3);
  CHECK(t.owner.stats().successCount == 1 && t.owner.stats().failCount == 1);

  t.port.deliver(BAUD.newReply);
  CHECK(t.owner.recover());
  historical(t, BAUD, 20);
  CHECK(t.port.writes.size() == 3);
  t.send(150000, {2, 3, 2, 0, 0, 1, 0x85, 0x81});
  t.drain(151000);
  t.reply(155000, {2, 3, 2, 0, 2, 0x7D, 0x85});
  const auto peer = t.collect(peerTicket);
  CHECK(peer.status && peer.attempts == 1 && peer.settingObservation);
  const auto& observation = *peer.settingObservation;
  CHECK(observation.value.model == otherModel(model) &&
        observation.value.setting == Setting::Address);
  CHECK(observation.value.rawWord == 2 && observation.endpoint.address == 2 &&
        observation.endpoint.bps == 115200 && observation.completedMs == 155);
  CHECK(t.owner.endpointTrusted(1) && t.owner.health(1).totalSuccess == 1);
  historical(t, BAUD, 20);

  const auto result = t.collect(ticket);
  CHECK(result.status.code == Err::LINK_LOST && result.write && result.attempts == 2 &&
        result.completedUs == 120000);
  const auto& op = *result.write;
  CHECK(op.tx == TxProgress::Complete && op.ackOrigin == AckOrigin::Device && op.ackStatus &&
        op.ackStatus->ok());
  CHECK(op.readStatus && op.readStatus->code == Err::TIMEOUT && !op.verified && !op.readback);
  CHECK(op.primary && op.primary->code == Err::LINK_LOST && op.cleanup &&
        op.cleanup->code == Err::BUS_BUSY);
  t.poll(200000);
  CHECK(t.owner.settled() && t.port.writes.size() == 4 && t.owner.physicalAttempts() == 4);
  const auto& stats = t.owner.stats();
  CHECK(stats.successCount == 2 && stats.failCount == 1 && stats.timeouts == 1 &&
        stats.retryCount == 0);
}

void verificationConfigureFailure(DeviceModel model) {
  Timeline t;
  seed(t, model, BAUD);
  const auto ticket = confirm(t, BAUD);
  t.send(20000, BAUD.write);
  t.drain(21000);
  t.port.configureStatus = {Err::IO, 37, "Test driver could not select the new rate"};
  t.reply(25000, BAUD.write);
  const auto result = t.collect(ticket);
  CHECK(result.status.code == Err::LINK_LOST && result.status.detail == 37 && result.write);
  const auto& op = *result.write;
  CHECK(op.ackStatus && op.ackStatus->ok() && op.readStatus && op.readStatus->code == Err::IO &&
        op.readStatus->detail == 37);
  CHECK(op.primary && op.primary->code == Err::LINK_LOST && op.cleanup && op.cleanup->ok());
  CHECK(!op.readback && !op.verified && result.attempts == 1 && t.port.baud == 115200);
  historical(t, BAUD, 20);
  CHECK(t.owner.settled() && t.port.writes.size() == 2);
}

void partialTxAndFailedDrain(DeviceModel model, bool failDrain) {
  Timeline t;
  seed(t, model, ADDRESS);
  const auto ticket = confirm(t, ADDRESS);
  t.port.writeLimit = 3;
  t.send(20000, {1, 6, 2});
  if (failDrain) {
    t.port.drainStatus = {Err::IO, 47, "Test driver drain error"};
  }
  t.poll(21000);
  const auto result = t.collect(ticket);
  CHECK(result.status.code == Err::IO && result.write && result.attempts == 1);
  CHECK(result.write->tx == TxProgress::Partial && result.write->txStatus &&
        result.write->txStatus->code == Err::IO);
  CHECK(result.write->primary && result.write->primary->detail == (failDrain ? 47 : 0));
  CHECK(result.write->cleanup && !result.write->cleanup->ok() && !result.write->readback);
  if (failDrain) {
    CHECK(result.write->cleanup->code == Err::IO && result.write->cleanup->detail == 47);
  }
  CHECK(!t.owner.recover() && !t.owner.recoverIsolated() && t.port.transmitting == failDrain);
  historical(t, ADDRESS, 20);
  t.port.isolationVerified = true;
  CHECK(t.owner.recoverIsolated() && !t.port.transmitting);
  CHECK(t.port.writes.size() == 2 && t.owner.stats().retryCount == 0);
}

void cancelVerificationAndRebind(DeviceModel model) {
  Timeline t;
  seed(t, model, ADDRESS);
  const auto old = confirm(t, ADDRESS);
  t.send(20000, ADDRESS.write);
  t.drain(21000);
  t.reply(25000, ADDRESS.write);
  t.send(25000, ADDRESS.verification);
  t.drain(26000);
  CHECK(t.owner.cancel(old));
  CHECK(t.owner.bind(0, otherModel(model), 5, 115200));
  std::uint64_t next = 0;
  CHECK(t.owner.submitSettingRead(0, Setting::Address, 94, 3, 100000, next));
  t.poll(30000);
  CHECK(!t.owner.settled() && t.port.writes.size() == 3);
  t.port.deliver(ADDRESS.newReply);
  CHECK(t.owner.recover() && !t.owner.endpointTrusted(0));
  CHECK(!t.owner.settingSnapshot(0, Setting::Address).observed &&
        t.owner.health(0).totalSuccess == 0);
  t.send(30000, ADDRESS.verification);
  t.drain(31000);
  t.reply(35000, ADDRESS.newReply);
  const auto fresh = t.collect(next);
  CHECK(fresh.settingObservation && fresh.settingObservation->value.model == otherModel(model) &&
        fresh.settingObservation->completedMs == 35);
  const auto original = t.collect(old);
  CHECK(!original.status && original.write && !original.write->verified &&
        !original.write->readback);
  CHECK(original.write->readStatus && original.write->readStatus->code == Err::ABORTED);
  CHECK(original.write->before.observed && original.write->before.observed->value.model == model);
  CHECK(t.port.writes.size() == 4 && original.attempts == 2 && t.owner.stats().retryCount == 0);
}

void measurementUnits(DeviceModel model) {
  Timeline t;
  CHECK(t.owner.bind(0, model, 1, 115200));
  const Bytes request{1, 3, 2, 9, 0, 1, 0x55, 0xB0};
  const Bytes write{1, 6, 2, 9, 0, 1, 0x99, 0xB0};
  read(t, 0, Setting::UartUnit, request, {1, 3, 2, 0, 0, 0xB8, 0x44});
  struct MeasurementCase {
    MeasurementKind kind;
    MeasurementUnit unit;
    double value;
    Bytes request, reply;
  };
  const MeasurementCase cases[]{
      {MeasurementKind::ProcessedDistance,
       MeasurementUnit::Millimetres,
       754,
       {1, 3, 1, 0, 0, 1, 0x85, 0xF6},
       {1, 3, 2, 2, 0xF2, 0x38, 0xA1}},
      {MeasurementKind::RealTimeDistance,
       MeasurementUnit::Millimetres,
       751,
       {1, 3, 1, 1, 0, 1, 0xD4, 0x36},
       {1, 3, 2, 2, 0xEF, 0xF8, 0xA8}},
      {MeasurementKind::Temperature, MeasurementUnit::DegreesCelsius, 30, TEMP_REQUEST, TEMP_REPLY},
      {MeasurementKind::EchoTime,
       MeasurementUnit::Microseconds,
       5750,
       {1, 3, 1, 0x0A, 0, 1, 0xA5, 0xF4},
       {1, 3, 2, 0x16, 0x76, 0x37, 0xC2}},
  };
  for (unsigned uartWord : {0U, 1U}) {
    if (uartWord == 1) {
      std::uint64_t ticket = 0;
      CHECK(t.owner.grantIsolatedMaintenance(0, SESSION));
      CHECK(t.owner.submitWrite(0, Setting::UartUnit, {SettingUnit::UartUnitCode, 1}, SESSION, 95,
                                1, t.port.time + 100000, ticket));
      t.send(t.port.time, write);
      t.drain(t.port.time + 1000);
      t.reply(t.port.time + 5000, write);
      t.send(t.port.time, request);
      t.drain(t.port.time + 1000);
      t.reply(t.port.time + 5000, WORD_ONE);
      const auto result = t.collect(ticket);
      CHECK(result.status && result.write && result.write->verified);
    }
    const auto setting = t.owner.settingSnapshot(0, Setting::UartUnit);
    CHECK(setting.trusted && setting.observed && setting.observed->value.rawWord == uartWord);
    for (const auto& entry : cases) {
      const auto ticket = t.submit(0, entry.kind, 96, 0, t.port.time + 1000000);
      t.send(t.port.time, entry.request);
      t.drain(t.port.time + 1000);
      t.reply(t.port.time + 400000, entry.reply);
      const auto result = t.collect(ticket);
      CHECK(result.status && result.measurement.unit == entry.unit &&
            result.measurement.value == entry.value);
    }
  }
  CHECK(t.port.writes.size() == 11 && t.owner.stats().retryCount == 0);
}
}  // namespace

int main() {
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    changeEndpoint(model, ADDRESS, STANDARD);
    changeEndpoint(model, BAUD, STANDARD);
    changeEndpoint(model, EXTEND, ISOLATED);
    changeEndpoint(model, RESTORE, ISOLATED);
    standardPolicy(model);
    lostAddressAck(model);
    mismatch(model);
    wrongEcho(model);
    writeExpiresBeforeTx(model, WriteScope::Addressed);
    writeExpiresBeforeTx(model, WriteScope::VendorSendOnly);
    silentNewBaud(model);
    verificationConfigureFailure(model);
    partialTxAndFailedDrain(model, false);
    partialTxAndFailedDrain(model, true);
    cancelVerificationAndRebind(model);
    measurementUnits(model);
  }
}
