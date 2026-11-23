#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

using Frame = std::array<std::uint8_t, 8>;
using Reply = std::array<std::uint8_t, 7>;
constexpr auto STANDARD = AddressPolicy::StandardUnicast;
constexpr auto ISOLATED = AddressPolicy::DFRobotIsolated;

struct Change {
  Setting setting;
  SettingValue target;
  std::uint16_t reg;
  std::uint16_t before;
  std::uint8_t verifyAddress;
  std::uint32_t verifyBps;
};

// Independent FC06 cases from OPERATIONS.csv. Seed each from an actual FC03 reply.
constexpr Change CHANGES[] = {
    {Setting::Address, {SettingUnit::Address, 5}, 0x0200, 1, 5, 115200},
    {Setting::Baud, {SettingUnit::BaudCode, 3}, 0x0201, 9, 1, 9600},
    {Setting::SwitchPolarity, {SettingUnit::PolarityCode, 0}, 0x0205, 1, 1, 115200},
    {Setting::SwitchThreshold, {SettingUnit::Millimetres, 750}, 0x0206, 100, 1, 115200},
    {Setting::Angle, {SettingUnit::AngleLevel, 2}, 0x0208, 4, 1, 115200},
    {Setting::UartUnit, {SettingUnit::UartUnitCode, 1}, 0x0209, 0, 1, 115200},
    {Setting::Noise, {SettingUnit::NoiseLevel, 4}, 0x021A, 1, 1, 115200},
    {Setting::Range, {SettingUnit::RangeLevel, 3}, 0x021F, 1, 1, 115200},
};

template <std::size_t N>
std::array<std::uint8_t, N> withCrc(std::array<std::uint8_t, N> frame) {
  std::uint16_t crc = 0xFFFF;
  for (std::size_t i = 0; i < N - 2; ++i) {
    crc ^= frame[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  frame[N - 2] = static_cast<std::uint8_t>(crc);
  frame[N - 1] = static_cast<std::uint8_t>(crc >> 8);
  return frame;
}

Frame request(std::uint8_t address, std::uint8_t function, std::uint16_t reg, std::uint16_t word) {
  return withCrc(Frame{address, function, static_cast<std::uint8_t>(reg >> 8),
                       static_cast<std::uint8_t>(reg), static_cast<std::uint8_t>(word >> 8),
                       static_cast<std::uint8_t>(word), 0, 0});
}

Reply wordReply(std::uint8_t address, std::uint16_t word) {
  return withCrc(Reply{address, 3, 2, static_cast<std::uint8_t>(word >> 8),
                       static_cast<std::uint8_t>(word), 0, 0});
}

constexpr Endpoint INITIAL{1, 115200};

bool sameEndpoint(Endpoint first, Endpoint second) {
  return first.address == second.address && first.bps == second.bps;
}

void checkObservation(const Configuration& config, Setting setting, Endpoint at, std::uint16_t word,
                      std::uint32_t completedMs, bool trusted) {
  const auto latest = config.snapshot(setting);
  CHECK(latest.trusted == trusted);
  CHECK(latest.observed);
  CHECK(latest.observed->value.setting == setting);
  CHECK(latest.observed->value.address == at.address);
  CHECK(latest.observed->value.rawWord == word);
  CHECK(sameEndpoint(latest.observed->endpoint, at));
  CHECK(latest.observed->completedMs == completedMs);
}

// Simulated owner. The produced frame and the host UART settings are checked independently.
struct Script {
  Configuration config;
  Frame frame{};
  ConfigurationToken write, read;

  void observe(Endpoint at, const Change& change, std::uint16_t word, std::uint32_t ms) {
    CHECK(config.buildRead(at, change.setting, frame.data(), frame.size(), read) == frame.size());
    CHECK(frame == request(at.address, 3, change.reg, 1));
    const auto reply = wordReply(at.address, word);
    CHECK(config.completeRead(read, at, reply.data(), reply.size(), ms));
    checkObservation(config, change.setting, at, word, ms, true);
    CHECK(config.endpointTrusted());
  }

  void seed(DeviceModel model, const Change& change, Endpoint at = INITIAL,
            AddressPolicy policy = STANDARD) {
    CHECK(config.bind(model, at, policy));
    CHECK(!config.endpointTrusted());
    CHECK(!config.snapshot(change.setting).observed);
    CHECK(!config.snapshot(change.setting).trusted);
    observe(at, change, change.before, 0);
  }

  void preview(const Change& change, WriteScope scope = WriteScope::Addressed) {
    const auto original = config.endpoint();
    const Endpoint intended{change.verifyAddress, change.verifyBps};
    CHECK(config.buildWrite(change.setting, change.target, scope, frame.data(), frame.size(),
                            write) == frame.size());
    const auto destination = scope == WriteScope::Addressed ? original.address : 0xFF;
    CHECK(frame == request(destination, 6, change.reg, change.target.word));
    const auto& op = config.operation();
    CHECK(op.scope == scope && op.setting == change.setting);
    CHECK(op.desired.unit == change.target.unit && op.desired.word == change.target.word);
    CHECK(sameEndpoint(op.original, original) && sameEndpoint(op.intended, intended));
    CHECK(!op.verified && !op.readback && !op.txStatus && !op.ackStatus);
    checkObservation(config, change.setting, original, change.before, 0, true);
    CHECK(config.endpointTrusted());
  }

  void beginVerification(const Change& change) {
    CHECK(config.buildVerification(write, frame.data(), frame.size(), read) == frame.size());
    CHECK(frame == request(change.verifyAddress, 3, change.reg, 1));
  }

  void verify(const Change& change) {
    const Endpoint intended{change.verifyAddress, change.verifyBps};
    beginVerification(change);
    const auto reply = wordReply(intended.address, change.target.word);
    CHECK(config.completeRead(read, intended, reply.data(), reply.size(), 25));
    checkObservation(config, change.setting, intended, change.target.word, 25, true);
    CHECK(config.endpointTrusted() && sameEndpoint(config.endpoint(), intended));
    const auto& op = config.operation();
    CHECK(op.verified && op.primary && op.primary->ok() && op.readStatus && op.readStatus->ok());
    CHECK(op.readback && op.readback->value.rawWord == change.target.word);
    CHECK(sameEndpoint(op.readback->endpoint, intended) && op.readback->completedMs == 25);
    CHECK(op.before.trusted && op.before.observed &&
          op.before.observed->value.rawWord == change.before);
    CHECK(sameEndpoint(op.before.observed->endpoint, op.original) &&
          op.before.observed->completedMs == 0);
  }
};

void checkChanges(DeviceModel model, WriteScope scope) {
  for (const auto& change : CHANGES) {
    Script s;
    const bool addressed = scope == WriteScope::Addressed;
    s.seed(model, change, INITIAL, addressed ? STANDARD : ISOLATED);
    s.preview(change, scope);
    const auto writeFrame = s.frame;
    CHECK(!s.config.buildVerification(s.write, s.frame.data(), s.frame.size(), s.read));
    CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
    const auto sent = s.config.operation();
    CHECK(sent.tx == TxProgress::Complete && sent.txStatus && sent.txStatus->ok());
    checkObservation(s.config, change.setting, INITIAL, change.before, 0, false);
    const bool movesEndpoint = change.reg == 0x0200 || change.reg == 0x0201;
    CHECK(s.config.endpointTrusted() == !movesEndpoint);
    if (addressed) {
      CHECK(s.config.recordAck(s.write, writeFrame.data(), writeFrame.size(), AckOrigin::Device));
      const auto& op = s.config.operation();
      CHECK(op.ackStatus && op.ackStatus->ok() && op.ackOrigin == AckOrigin::Device);
      CHECK(!op.verified && !op.readback);
      CHECK(s.config.endpointTrusted() == !movesEndpoint);
      checkObservation(s.config, change.setting, INITIAL, change.before, 0, false);
      s.verify(change);
    } else {
      // A local echo and an unsolicited addressed ACK are equally insufficient for a 0xFF send.
      CHECK(
          !s.config.recordAck(s.write, writeFrame.data(), writeFrame.size(), AckOrigin::LocalEcho));
      auto stray = request(1, 6, change.reg, change.target.word);
      CHECK(!s.config.recordAck(s.write, stray.data(), stray.size(), AckOrigin::Device));
      CHECK(!s.config.missAck(s.write, {Err::TIMEOUT, 0, "no reply"}));
      CHECK(!s.config.buildVerification(s.write, s.frame.data(), s.frame.size(), s.read));
      const auto sentOnly = s.config.operation();
      CHECK(sentOnly.primary && sentOnly.primary->ok() && !sentOnly.ackStatus &&
            !sentOnly.readStatus && !sentOnly.verified);
      checkObservation(s.config, change.setting, INITIAL, change.before, 0, false);
      const Endpoint at{change.verifyAddress, change.verifyBps};
      s.observe(at, change, change.target.word, 25);
      const auto& op = s.config.operation();
      CHECK(op.scope == WriteScope::VendorSendOnly && op.tx == TxProgress::Complete);
      CHECK(op.primary && op.primary->ok() && !op.readback && !op.verified);
      CHECK(op.before.trusted && op.before.observed &&
            op.before.observed->value.rawWord == change.before);
    }
  }
}

void checkLostAddressAck(DeviceModel model) {
  const auto& change = CHANGES[0];
  Script s;
  s.seed(model, change);
  const auto previousRead = s.read;
  s.preview(change);
  CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
  const Status lost{Err::TIMEOUT, -7, "ACK timeout"};
  CHECK(s.config.missAck(s.write, lost).code == Err::TIMEOUT);
  const auto pending = s.config.operation();
  CHECK(pending.ackStatus && pending.ackStatus->code == Err::TIMEOUT &&
        pending.ackStatus->detail == -7);
  CHECK(!pending.verified && !pending.readback && pending.desired.word == 5);
  const auto oldBytes = wordReply(1, 1);
  CHECK(!s.config.completeRead(previousRead, INITIAL, oldBytes.data(), oldBytes.size(), 14));
  CHECK(!s.config.endpointTrusted());
  checkObservation(s.config, change.setting, INITIAL, 1, 0, false);
  s.verify(change);
  CHECK(s.config.operation().ackStatus->code == Err::TIMEOUT);
  CHECK(!pending.verified && pending.desired.word == 5);
}

void checkEchoes(DeviceModel model) {
  const auto& change = CHANGES[0];
  for (auto origin : {AckOrigin::Unknown, AckOrigin::LocalEcho}) {
    Script s;
    s.seed(model, change);
    s.preview(change);
    CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
    CHECK(s.config.recordAck(s.write, s.frame.data(), s.frame.size(), origin));
    const auto& op = s.config.operation();
    CHECK(op.ackStatus && op.ackStatus->ok() && op.ackOrigin == origin);
    CHECK(op.ackError == WriteResponseError::NONE && !op.verified && !s.config.endpointTrusted());
    checkObservation(s.config, change.setting, INITIAL, 1, 0, false);
  }
  for (const auto diagnostic :
       {WriteResponseError::REGISTER_MISMATCH, WriteResponseError::VALUE_MISMATCH,
        WriteResponseError::UNEXPECTED_ADDRESS, WriteResponseError::EXCEPTION_RESPONSE}) {
    Script s;
    s.seed(model, change);
    s.preview(change);
    CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
    auto reply = request(diagnostic == WriteResponseError::UNEXPECTED_ADDRESS ? 5 : 1, 6,
                         diagnostic == WriteResponseError::REGISTER_MISMATCH ? 0x0201 : 0x0200,
                         diagnostic == WriteResponseError::VALUE_MISMATCH ? 4 : 5);
    auto exception = withCrc(std::array<std::uint8_t, 5>{1, 0x86, 0x91, 0, 0});
    const bool isException = diagnostic == WriteResponseError::EXCEPTION_RESPONSE;
    const auto rc =
        s.config.recordAck(s.write, isException ? exception.data() : reply.data(),
                           isException ? exception.size() : reply.size(), AckOrigin::Device);
    CHECK(rc.code == (isException ? Err::EXCEPTION : Err::FRAME_ERROR));
    const auto& op = s.config.operation();
    CHECK(op.ackStatus && op.ackStatus->code == rc.code && op.ackError == diagnostic);
    CHECK(!isException || op.ackStatus->detail == 0x91);
    CHECK(!op.verified && !op.readback && !s.config.endpointTrusted());
    checkObservation(s.config, change.setting, INITIAL, 1, 0, false);
    if (isException) {
      CHECK(s.config.finishWrite(s.write, rc).code == Err::EXCEPTION);
      CHECK(s.config.recordCleanup(s.write, Ok()));
      const auto& settled = s.config.operation();
      CHECK(settled.primary && settled.primary->code == Err::EXCEPTION &&
            settled.primary->detail == 0x91);
      CHECK(settled.ackStatus->detail == 0x91 && settled.cleanup && settled.cleanup->ok());
      CHECK(!settled.verified && !settled.readback);
    }
  }
}

void checkTxProgress(DeviceModel model) {
  const auto& change = CHANGES[0];
  for (auto scope : {WriteScope::Addressed, WriteScope::VendorSendOnly}) {
    for (auto progress : {TxProgress::Zero, TxProgress::Possible, TxProgress::Partial}) {
      Script s;
      s.seed(model, change, INITIAL, scope == WriteScope::VendorSendOnly ? ISOLATED : STANDARD);
      s.preview(change, scope);
      const bool unsent = progress == TxProgress::Zero;
      const Status cause = unsent ? Status{Err::DISALLOWED, 5, "refused"}
                                  : Status{Err::IO, -15, "write interrupted"};
      CHECK(s.config.recordTx(s.write, progress, cause).code == cause.code);
      const auto& op = s.config.operation();
      CHECK(op.tx == progress && op.txStatus && op.txStatus->code == cause.code &&
            op.txStatus->detail == cause.detail);
      CHECK(op.desired.word == 5 && !op.verified && !op.readback);
      CHECK(s.config.endpointTrusted() == unsent);
      checkObservation(s.config, change.setting, INITIAL, 1, 0, unsent);
      if (unsent || scope == WriteScope::VendorSendOnly) {
        CHECK(!s.config.buildVerification(s.write, s.frame.data(), s.frame.size(), s.read));
      } else {
        s.verify(change);
      }
    }
  }
}

void checkProvenZeroAfterPossible(DeviceModel model, WriteScope scope) {
  Script s;
  const auto& address = CHANGES[0];
  s.seed(model, address, INITIAL, scope == WriteScope::VendorSendOnly ? ISOLATED : STANDARD);
  const Status unsent{Err::IO, -17, "driver confirmed zero bytes sent"};
  // A successful driver call can still report that none of the FC06 was sent.
  for (bool initiallyPossible : {false, true}) {
    s.preview(address, scope);
    if (initiallyPossible) {
      CHECK(s.config.recordTx(s.write, TxProgress::Possible, Ok()));
      CHECK(!s.config.endpointTrusted());
    }
    CHECK(s.config.recordTx(s.write, TxProgress::Zero, Ok()).code == Err::IO);
    const auto& op = s.config.operation();
    CHECK(op.tx == TxProgress::Zero && op.txStatus && op.txStatus->ok());
    CHECK(op.primary && op.primary->code == Err::IO && !op.verified);
    CHECK(s.config.endpointTrusted() && !s.config.possibleEndpoint());
    checkObservation(s.config, address.setting, INITIAL, 1, 0, true);
  }

  s.preview(address, scope);
  CHECK(s.config.recordTx(s.write, TxProgress::Possible, Ok()));
  CHECK(!s.config.endpointTrusted());
  checkObservation(s.config, address.setting, INITIAL, 1, 0, false);
  CHECK(s.config.recordTx(s.write, TxProgress::Zero, unsent).code == Err::IO);
  CHECK(s.config.endpointTrusted() && !s.config.possibleEndpoint());
  checkObservation(s.config, address.setting, INITIAL, 1, 0, true);
  CHECK(s.config.operation().tx == TxProgress::Zero && s.config.operation().primary &&
        s.config.operation().primary->detail == -17);

  // A second write that sent zero bytes must preserve uncertainty left by the first.
  s.preview(address, scope);
  CHECK(s.config.recordTx(s.write, TxProgress::Partial, Ok()));
  CHECK(s.config.recordTx(s.write, TxProgress::Zero, unsent).code == Err::INVALID_CONFIG);
  CHECK(s.config.finishWrite(s.write, {Err::ABORTED, 0, "partial send"}).code == Err::ABORTED);
  CHECK(s.config.buildWrite(address.setting, {SettingUnit::Address, 7}, scope, s.frame.data(),
                            s.frame.size(), s.write) == s.frame.size());
  CHECK(s.config.recordTx(s.write, TxProgress::Possible, Ok()));
  CHECK(s.config.possibleEndpoint() && s.config.possibleEndpoint()->address == 7);
  const Endpoint tentative{7, 115200};
  CHECK(s.config.buildRead(tentative, address.setting, s.frame.data(), s.frame.size(), s.read) ==
        s.frame.size());
  CHECK(s.config.recordTx(s.write, TxProgress::Zero, unsent).code == Err::IO);
  const auto stray = wordReply(7, 7);
  CHECK(s.config.completeRead(s.read, tentative, stray.data(), stray.size(), 37).code ==
        Err::INVALID_CONFIG);
  CHECK(!s.config.endpointTrusted());
  CHECK(s.config.possibleEndpoint() && s.config.possibleEndpoint()->address == 5);
  checkObservation(s.config, address.setting, INITIAL, 1, 0, false);
}

void checkReadFailures(DeviceModel model) {
  const auto& baud = CHANGES[1];
  const Endpoint intended{1, 9600};
  const auto reply = wordReply(1, 3);
  Script s;
  s.seed(model, baud);
  s.preview(baud);
  CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
  s.beginVerification(baud);
  // The exact same bytes decode as code 3 at 115200. Only a read at 9600 can verify this change.
  CHECK(s.config.completeRead(s.read, INITIAL, reply.data(), reply.size(), 15).code ==
        Err::INVALID_CONFIG);
  CHECK(!s.config.operation().verified && !s.config.endpointTrusted());
  checkObservation(s.config, baud.setting, INITIAL, 9, 0, false);
  CHECK(s.config.completeRead(s.read, intended, reply.data(), reply.size(), 16));
  checkObservation(s.config, baud.setting, intended, 3, 16, true);

  Script silent;
  silent.seed(model, baud);
  silent.preview(baud);
  CHECK(silent.config.recordTx(silent.write, TxProgress::Complete, Ok()));
  silent.beginVerification(baud);
  const Status deadline{Err::TIMEOUT, -19, "new baud silent"};
  CHECK(silent.config.failRead(silent.read, deadline).code == Err::LINK_LOST);
  const auto& op = silent.config.operation();
  CHECK(op.primary && op.primary->code == Err::LINK_LOST && op.readStatus);
  CHECK(op.readStatus->code == Err::TIMEOUT && op.readStatus->detail == -19);
  CHECK(!op.readback && !op.verified && !silent.config.endpointTrusted());
  CHECK(sameEndpoint(op.original, INITIAL) && sameEndpoint(op.intended, intended));
  checkObservation(silent.config, baud.setting, INITIAL, 9, 0, false);
  CHECK(silent.config.recordCleanup(silent.write, {Err::IO, -23, "host restore failed"}).code ==
        Err::IO);
  const auto& cleaned = silent.config.operation();
  CHECK(cleaned.cleanup && cleaned.cleanup->code == Err::IO && cleaned.cleanup->detail == -23);
  CHECK(cleaned.primary->code == Err::LINK_LOST && cleaned.readStatus->detail == -19);

  Script exceptional;
  exceptional.seed(model, baud);
  exceptional.preview(baud);
  CHECK(exceptional.config.recordTx(exceptional.write, TxProgress::Complete, Ok()));
  exceptional.beginVerification(baud);
  auto exception = withCrc(std::array<std::uint8_t, 5>{1, 0x83, 0x84, 0, 0});
  auto status = exceptional.config.completeRead(exceptional.read, intended, exception.data(),
                                                exception.size(), 30);
  CHECK(status.code == Err::EXCEPTION && status.detail == 0x84);
  const auto& failed = exceptional.config.operation();
  CHECK(failed.readError == ReadResponseError::EXCEPTION_RESPONSE && failed.readStatus);
  CHECK(failed.readStatus->code == Err::EXCEPTION && failed.readStatus->detail == 0x84);
  CHECK(!failed.readback && !failed.verified);
  CHECK(exceptional.config.endpointTrusted() &&
        sameEndpoint(exceptional.config.endpoint(), intended));
  CHECK(!exceptional.config.possibleEndpoint());
  checkObservation(exceptional.config, baud.setting, INITIAL, 9, 0, false);
}

void checkVendorRecoveryException(DeviceModel model) {
  const auto& address = CHANGES[0];
  const Endpoint intended{5, 115200};
  Script s;
  s.seed(model, address, INITIAL, ISOLATED);
  s.preview(address, WriteScope::VendorSendOnly);
  CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
  for (auto replyAddress : {1, 5}) {
    CHECK(s.config.buildRead(intended, address.setting, s.frame.data(), s.frame.size(), s.read) ==
          s.frame.size());
    const auto reply = withCrc(
        std::array<std::uint8_t, 5>{static_cast<std::uint8_t>(replyAddress), 0x83, 2, 0, 0});
    const auto status = s.config.completeRead(s.read, intended, reply.data(), reply.size(), 20);
    if (replyAddress == 1) {
      CHECK(status.code == Err::FRAME_ERROR && !s.config.endpointTrusted());
      CHECK(sameEndpoint(s.config.endpoint(), INITIAL) && s.config.possibleEndpoint());
    } else {
      CHECK(status.code == Err::EXCEPTION && status.detail == 2 && s.config.endpointTrusted());
      CHECK(sameEndpoint(s.config.endpoint(), intended) && !s.config.possibleEndpoint());
    }
    checkObservation(s.config, address.setting, INITIAL, 1, 0, false);
    const auto& send = s.config.operation();
    CHECK(send.primary && send.primary->ok() && !send.readStatus && !send.readback &&
          !send.verified);
  }
  s.observe(intended, address, 5, 25);
}

void checkInterruptedWrite(DeviceModel model) {
  const auto& address = CHANGES[0];
  const auto& range = CHANGES[7];
  Script s;
  s.seed(model, address);
  s.observe(INITIAL, range, range.before, 11);
  s.preview(address);
  CHECK(s.config.recordTx(s.write, TxProgress::Possible, Ok()));
  checkObservation(s.config, address.setting, INITIAL, 1, 0, false);
  CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
  s.beginVerification(address);
  s.config.interrupt();
  const auto& op = s.config.operation();
  CHECK(op.tx == TxProgress::Complete && !op.verified && op.primary);
  CHECK(op.primary->code == Err::LINK_LOST);
  CHECK(!s.config.endpointTrusted());
  checkObservation(s.config, address.setting, INITIAL, 1, 0, false);
  checkObservation(s.config, range.setting, INITIAL, range.before, 11, false);
  const auto late = wordReply(5, 5);
  CHECK(!s.config.completeRead(s.read, {5, 115200}, late.data(), late.size(), 19));
  CHECK(!s.config.buildVerification(s.write, s.frame.data(), s.frame.size(), s.read));
  CHECK(!op.verified && op.primary->code == Err::LINK_LOST);
}

void checkMismatchAndInterruption(DeviceModel model) {
  const auto& change = CHANGES[5];  // Unknown UART unit words remain observable.
  Script s;
  s.seed(model, change);
  s.preview(change);
  CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
  s.beginVerification(change);
  const auto mismatch = wordReply(1, 0x8142);
  CHECK(s.config.completeRead(s.read, INITIAL, mismatch.data(), mismatch.size(), 31).code ==
        Err::CONFIG_MISMATCH);
  const auto result = s.config.operation();
  CHECK(result.primary && result.primary->code == Err::CONFIG_MISMATCH);
  CHECK(result.readback && !result.readback->value.known && result.desired.word == 1);
  CHECK(!result.verified && result.readStatus && result.readStatus->ok());
  CHECK(s.config.endpointTrusted());
  checkObservation(s.config, change.setting, INITIAL, 0x8142, 31, true);
  // Start a new, independent FC03 then invalidate the binding before its reply.
  CHECK(s.config.buildRead(INITIAL, change.setting, s.frame.data(), s.frame.size(), s.read) ==
        s.frame.size());
  s.config.interrupt();
  CHECK(!s.config.endpointTrusted());
  checkObservation(s.config, change.setting, INITIAL, 0x8142, 31, false);
  const auto desired = wordReply(1, 1);
  CHECK(!s.config.completeRead(s.read, INITIAL, desired.data(), desired.size(), 32));
  checkObservation(s.config, change.setting, INITIAL, 0x8142, 31, false);
  s.observe(INITIAL, change, 0, 34);
}

void checkExtended(DeviceModel model, std::uint8_t original, std::uint8_t intended) {
  const Change change{
      Setting::Address, {SettingUnit::Address, intended}, 0x0200, original, intended, 115200};
  Script s;
  s.seed(model, change, {original, 115200}, ISOLATED);
  s.preview(change);
  CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
  s.verify(change);
}

void checkRecoveryAfterRefusal(DeviceModel model) {
  Script s;
  const auto& address = CHANGES[0];
  s.seed(model, address, INITIAL, ISOLATED);
  s.preview(address, WriteScope::VendorSendOnly);
  CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));
  const auto vendor = s.config.operation();
  const auto& other = CHANGES[3];
  CHECK(s.config.buildWrite(other.setting, other.target, WriteScope::Addressed, s.frame.data(),
                            s.frame.size(), s.write) == s.frame.size());
  CHECK(s.config.recordTx(s.write, TxProgress::Zero, {Err::DISALLOWED, 0, "refused"}).code ==
        Err::DISALLOWED);
  CHECK(!s.config.endpointTrusted());
  checkObservation(s.config, address.setting, INITIAL, 1, 0, false);
  s.observe({5, 115200}, address, 5, 51);
  const auto& refused = s.config.operation();
  CHECK(refused.primary && refused.primary->code == Err::DISALLOWED && !refused.verified);
  CHECK(vendor.primary && vendor.primary->ok() && !vendor.verified);
}

void checkRecoveryWithPreview(DeviceModel model) {
  Script s;
  const auto& address = CHANGES[0];
  s.seed(model, address, INITIAL, ISOLATED);
  s.preview(address, WriteScope::VendorSendOnly);
  CHECK(s.config.recordTx(s.write, TxProgress::Complete, Ok()));

  const auto& range = CHANGES[7];
  CHECK(s.config.buildWrite(range.setting, range.target, WriteScope::Addressed, s.frame.data(),
                            s.frame.size(), s.write) == s.frame.size());
  const Endpoint recovered{5, 115200};
  s.observe(recovered, address, 5, 51);
  const auto& plan = s.config.operation();
  CHECK(plan.primary && plan.primary->code == Err::ABORTED && plan.tx == TxProgress::Zero);
  CHECK(s.config.recordTx(s.write, TxProgress::Possible, Ok()).code == Err::INVALID_CONFIG);
  CHECK(!s.config.buildVerification(s.write, s.frame.data(), s.frame.size(), s.read));
  CHECK(s.config.buildWrite(range.setting, range.target, WriteScope::Addressed, s.frame.data(),
                            s.frame.size(), s.write) == s.frame.size());
  CHECK(s.frame == request(5, 6, range.reg, range.target.word));
}

}  // namespace

int main() {
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    checkChanges(model, WriteScope::Addressed);
    checkChanges(model, WriteScope::VendorSendOnly);
    checkLostAddressAck(model);
    checkEchoes(model);
    checkTxProgress(model);
    checkProvenZeroAfterPossible(model, WriteScope::Addressed);
    checkProvenZeroAfterPossible(model, WriteScope::VendorSendOnly);
    checkReadFailures(model);
    checkVendorRecoveryException(model);
    checkInterruptedWrite(model);
    checkMismatchAndInterruption(model);
    checkExtended(model, 247, 248);
    checkExtended(model, 254, 1);
    checkRecoveryAfterRefusal(model);
    checkRecoveryWithPreview(model);
  }
}
