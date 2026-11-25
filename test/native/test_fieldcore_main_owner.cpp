#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FieldCoreMainOwner.h"

#include <SEN059x/SEN059x.h>

namespace {

using namespace FieldCoreMain;
using Reason = Rs485TransportTerminalReason;
static_assert(kTxPin == 47 && kRxPin == 48 && kDeRePin == 21 && kDeActiveHigh);
static_assert(kRs485DefaultBaud == 9600);
static_assert(kRs485TransactionTxCapacity == 8 && kRs485TransactionRxCapacity == 192 &&
              kRs485TransactionSuffixCapacity == 2);

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

// Real-time reading from the SEN0591 and SEN0592 manuals, example 2.
constexpr std::array<std::uint8_t, 7> REPLY{1, 3, 2, 2, 0xEF, 0xF8, 0xA8};

Rs485ModuleStep readStep(SEN059x::RequestContext& context) {
  Rs485ModuleStep step{};
  step.action = Rs485ModuleAction::RequestTransaction;
  auto& tx = step.transaction;
  tx.format = {115200, 8, Rs485Parity::None, 1};
  tx.txLength = static_cast<std::uint16_t>(SEN059x::buildReadRegisters(
      SEN059x::DeviceModel::SEN0591, 1, 0x0101, 1, tx.txBytes, sizeof(tx.txBytes), context));
  CHECK(tx.txLength == 8);
  tx.expectedLength = 7;
  tx.rxCapacity = kRs485TransactionRxCapacity;
  tx.responseTimeoutMs = 100;
  return step;
}

void checkFixedLength() {
  SEN059x::RequestContext context{};
  const auto step = readStep(context);
  Owner owner;
  Rs485BusObservation observation{};
  std::array<std::uint8_t, 9> padded{};
  std::copy(REPLY.begin(), REPLY.end(), padded.begin());
  // CRC-16/MODBUS over the complete valid prefix is zero. The appended 00 00
  // therefore lets the oversized batch reach the parser's exact length check.
  CHECK(owner.dispatch(step, 10, 1000));
  CHECK(!owner.dispatch(step, 11, 1000));
  CHECK(owner.receive(padded.data(), padded.size(), 25, observation));
  CHECK(observation.reason == Reason::Complete && observation.rxLength == padded.size());
  SEN059x::ReadResponseError error{};
  std::uint16_t word = 123;
  CHECK(!SEN059x::validateReadResponseExpected(observation.rxBytes, observation.rxLength, context,
                                               &word, 1, error));
  CHECK(error == SEN059x::ReadResponseError::LENGTH_MISMATCH && word == 123);
  CHECK(owner.fc03Transfers == 1 && owner.fc06Transfers == 0 && owner.transfers == 1);

  // Delay those two surplus bytes until the owner has handed the seven-byte
  // prefix to the caller. Compare reply alone with a split TX echo + reply.
  for (bool echo : {false, true}) {
    Owner replay;
    CHECK(replay.dispatch(step, 50, 1000));
    std::array<std::uint8_t, 12> rest{};
    const std::uint8_t* batch = REPLY.data();
    std::size_t length = REPLY.size();
    if (echo) {
      CHECK(!replay.receive(step.transaction.txBytes, 3, 61, observation));
      std::copy_n(step.transaction.txBytes + 3, 5, rest.begin());
      std::copy(REPLY.begin(), REPLY.end(), rest.begin() + 5);
      batch = rest.data();
      length = rest.size();
    }
    CHECK(replay.receive(batch, length, 70, observation));
    CHECK(observation.reason == Reason::Complete && observation.txCount == 8 &&
          observation.rxLength == 7 && observation.rxCount == 7 &&
          observation.completedUptimeMs == 70 && observation.error.code == ErrorCode::Ok);
    CHECK(std::memcmp(observation.rxBytes, REPLY.data(), REPLY.size()) == 0);
    CHECK(SEN059x::validateReadResponseExpected(observation.rxBytes, observation.rxLength, context,
                                                &word, 1, error));
    CHECK(error == SEN059x::ReadResponseError::NONE && word == 751);
    CHECK(!replay.receive(padded.data() + 7, 2, 71, observation));
    CHECK(replay.fc03Transfers == 1 && replay.fc06Transfers == 0 && replay.transfers == 1);
  }
}

void checkSuffixAndTimeouts() {
  // VibWire's takeStep builds a text transaction with the two-byte CR/LF suffix.
  Rs485ModuleStep step{};
  step.action = Rs485ModuleAction::RequestTransaction;
  auto& tx = step.transaction;
  tx.format.baud = kRs485DefaultBaud;
  tx.txBytes[0] = '0';
  tx.txBytes[1] = '!';
  tx.txLength = 2;
  tx.rxCapacity = kRs485TransactionRxCapacity;
  tx.responseTimeoutMs = 100;
  tx.completionMode = Rs485CompletionMode::Suffix;
  tx.suffixBytes[0] = '\r';
  tx.suffixBytes[1] = '\n';
  tx.suffixLength = 2;

  Owner owner;
  Rs485BusObservation observation{};
  constexpr std::uint8_t text[] = {'1', '\r', '\n', '+', '1', '\r', '\n'};
  CHECK(owner.dispatch(step, 0, 200));
  CHECK(!owner.receive(text, 6, 10, observation));  // embedded CR/LF
  CHECK(owner.receive(text + 6, 1, 20, observation));
  CHECK(observation.reason == Reason::Complete && observation.rxLength == sizeof(text) &&
        std::memcmp(text, observation.rxBytes, sizeof(text)) == 0);

  std::array<std::uint8_t, 193> full{};
  CHECK(owner.dispatch(step, 30, 200));
  CHECK(owner.receive(full.data(), full.size(), 31, observation));
  CHECK(observation.reason == Reason::BufferFull && observation.rxLength == 192);
  tx.interByteTimeoutMs = 5;
  CHECK(owner.dispatch(step, 40, 200));
  CHECK(!owner.receive(text, 1, 41, observation));
  CHECK(owner.receive(nullptr, 0, 46, observation));
  CHECK(observation.reason == Reason::InterByteTimeout && observation.rxLength == 1);
  CHECK(owner.dispatch(step, 50, 200));
  CHECK(!owner.receive(tx.txBytes, 1, 51, observation));  // possible partial echo
  CHECK(!owner.receive(nullptr, 0, 56, observation));
  CHECK(owner.receive(nullptr, 0, 150, observation));
  CHECK(observation.reason == Reason::ResponseTimeout && observation.rxLength == 1);
  CHECK(owner.dispatch(step, 151, 155));
  CHECK(owner.receive(text, sizeof(text), 155, observation));
  CHECK(observation.reason == Reason::DeadlineExpired && observation.rxCount == 0);
}

}  // namespace

void testFieldCoreMainOwner() {
  checkFixedLength();
  checkSuffixAndTimeouts();
}
