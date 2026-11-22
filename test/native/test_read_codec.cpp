// clang-format off: Include the public header first to check self-containment.
#include <SEN059x/ReadCodec.h>
// clang-format on

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

using namespace SEN059x;
using ReadError = ReadResponseError;
using RequestFrame = std::array<std::uint8_t, 8>;
using RawWords = std::array<std::uint16_t, 3>;

constexpr auto READ = FunctionCode::ReadHoldingRegisters;
constexpr auto WRITE = FunctionCode::WriteSingleRegister;
constexpr auto STANDARD = AddressPolicy::StandardUnicast;
constexpr auto ISOLATED = AddressPolicy::DFRobotIsolated;
constexpr auto MODEL = DeviceModel::SEN0591;
constexpr auto INVALID_POLICY = static_cast<AddressPolicy>(0xFF);
constexpr RequestContext PRIOR_CONTEXT{DeviceModel::SEN0592, 250, WRITE, 0x0206, 750, ISOLATED};
constexpr std::array<std::uint16_t, 5> PRIOR_WORDS{0x1234, 0xABCD, 0xCDEF, 0x9876, 0x5678};
constexpr std::uint8_t BYTE_CANARY = 0xA5;

// Report failed checks in Release builds too.
#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

// The four published pairs come from modbus_vectors.csv. The remaining pairs
// use OPERATIONS.csv words and independently calculated CRC-16/MODBUS values.
struct ReadCase {
  std::uint16_t startRegister;
  std::uint16_t count;
  RequestFrame tx;
  std::array<std::uint8_t, 11> rx;
  RawWords words;
};

constexpr ReadCase PROCESSED{0x0100,
                             1,
                             {0x01, 0x03, 0x01, 0x00, 0x00, 0x01, 0x85, 0xF6},
                             {0x01, 0x03, 0x02, 0x02, 0xF2, 0x38, 0xA1},
                             {0x02F2}};
constexpr ReadCase MEASUREMENT_BLOCK{
    0x0100,
    3,
    {0x01, 0x03, 0x01, 0x00, 0x00, 0x03, 0x04, 0x37},
    {0x01, 0x03, 0x06, 0x02, 0xF2, 0x02, 0xEF, 0x01, 0x2C, 0x29, 0x42},
    {0x02F2, 0x02EF, 0x012C}};

constexpr ReadCase READ_CASES[] = {
    PROCESSED,
    {0x0100,
     2,
     {0x01, 0x03, 0x01, 0x00, 0x00, 0x02, 0xC5, 0xF7},
     {0x01, 0x03, 0x04, 0x02, 0xF2, 0x02, 0xEF, 0x1A, 0x94},
     {0x02F2, 0x02EF}},
    MEASUREMENT_BLOCK,
    {0x0101,
     1,
     {0x01, 0x03, 0x01, 0x01, 0x00, 0x01, 0xD4, 0x36},
     {0x01, 0x03, 0x02, 0x02, 0xEF, 0xF8, 0xA8},
     {0x02EF}},
    {0x0101,
     2,
     {0x01, 0x03, 0x01, 0x01, 0x00, 0x02, 0x94, 0x37},
     {0x01, 0x03, 0x04, 0x02, 0xEF, 0x01, 0x2C, 0xCA, 0x33},
     {0x02EF, 0x012C}},
    {0x0102,
     1,
     {0x01, 0x03, 0x01, 0x02, 0x00, 0x01, 0x24, 0x36},
     {0x01, 0x03, 0x02, 0x01, 0x2C, 0xB8, 0x09},
     {0x012C}},
    {0x010A,
     1,
     {0x01, 0x03, 0x01, 0x0A, 0x00, 0x01, 0xA5, 0xF4},
     {0x01, 0x03, 0x02, 0x16, 0x76, 0x37, 0xC2},
     {0x1676}},
    {0x0200,
     1,
     {0x01, 0x03, 0x02, 0x00, 0x00, 0x01, 0x85, 0xB2},
     {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84},
     {0x0001}},
    {0x0200,
     2,
     {0x01, 0x03, 0x02, 0x00, 0x00, 0x02, 0xC5, 0xB3},
     {0x01, 0x03, 0x04, 0x00, 0x01, 0x00, 0x03, 0xEB, 0xF2},
     {0x0001, 0x0003}},
    {0x0201,
     1,
     {0x01, 0x03, 0x02, 0x01, 0x00, 0x01, 0xD4, 0x72},
     {0x01, 0x03, 0x02, 0x00, 0x03, 0xF8, 0x45},
     {0x0003}},
    {0x0205,
     1,
     {0x01, 0x03, 0x02, 0x05, 0x00, 0x01, 0x95, 0xB3},
     {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84},
     {0x0001}},
    {0x0205,
     2,
     {0x01, 0x03, 0x02, 0x05, 0x00, 0x02, 0xD5, 0xB2},
     {0x01, 0x03, 0x04, 0x00, 0x01, 0x00, 0x64, 0xAA, 0x18},
     {0x0001, 0x0064}},
    {0x0206,
     1,
     {0x01, 0x03, 0x02, 0x06, 0x00, 0x01, 0x65, 0xB3},
     {0x01, 0x03, 0x02, 0x00, 0x64, 0xB9, 0xAF},
     {0x0064}},
    {0x0208,
     1,
     {0x01, 0x03, 0x02, 0x08, 0x00, 0x01, 0x04, 0x70},
     {0x01, 0x03, 0x02, 0x00, 0x04, 0xB9, 0x87},
     {0x0004}},
    {0x0208,
     2,
     {0x01, 0x03, 0x02, 0x08, 0x00, 0x02, 0x44, 0x71},
     {0x01, 0x03, 0x04, 0x00, 0x04, 0x00, 0x00, 0xBB, 0xF2},
     {0x0004, 0x0000}},
    {0x0209,
     1,
     {0x01, 0x03, 0x02, 0x09, 0x00, 0x01, 0x55, 0xB0},
     {0x01, 0x03, 0x02, 0x00, 0x00, 0xB8, 0x44},
     {0x0000}},
    {0x021A,
     1,
     {0x01, 0x03, 0x02, 0x1A, 0x00, 0x01, 0xA4, 0x75},
     {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84},
     {0x0001}},
    {0x021F,
     1,
     {0x01, 0x03, 0x02, 0x1F, 0x00, 0x01, 0xB4, 0x74},
     {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84},
     {0x0001}},
};

constexpr RequestContext EXPECTED{MODEL,           1,       READ, PROCESSED.startRegister,
                                  PROCESSED.count, STANDARD};
constexpr RequestContext BLOCK_CONTEXT{
    MODEL, 1, READ, MEASUREMENT_BLOCK.startRegister, MEASUREMENT_BLOCK.count, STANDARD};

bool sameContext(const RequestContext& actual, const RequestContext& expected) {
  return actual.model == expected.model && actual.address == expected.address &&
         actual.function == expected.function && actual.startRegister == expected.startRegister &&
         actual.countOrWriteWord == expected.countOrWriteWord &&
         actual.addressPolicy == expected.addressPolicy;
}

RequestContext checkBuild(const RequestContext& expected, const RequestFrame& frame,
                          std::size_t capacity) {
  std::array<std::uint8_t, 11> tx;
  tx.fill(BYTE_CANARY);
  auto expectedTx = tx;
  std::copy(frame.begin(), frame.end(), expectedTx.begin() + 1);
  auto actual = PRIOR_CONTEXT;
  CHECK(buildReadRegisters(expected.model, expected.address, expected.startRegister,
                           expected.countOrWriteWord, tx.data() + 1, capacity, actual,
                           expected.addressPolicy) == frame.size());
  CHECK(tx == expectedTx);
  CHECK(sameContext(actual, expected));
  return actual;
}

void checkBuildRejected(const RequestContext& input, std::size_t capacity = 8) {
  std::array<std::uint8_t, 10> tx;
  tx.fill(BYTE_CANARY);
  const auto saved = tx;
  auto out = PRIOR_CONTEXT;
  CHECK(buildReadRegisters(input.model, input.address, input.startRegister, input.countOrWriteWord,
                           tx.data() + 1, capacity, out, input.addressPolicy) == 0);
  CHECK(tx == saved);
  CHECK(sameContext(out, PRIOR_CONTEXT));
}

void checkReply(const std::uint8_t* rx, std::size_t length, const RequestContext& expected,
                const RawWords& values, std::size_t capacity) {
  auto words = PRIOR_WORDS;
  auto expectedWords = words;
  std::copy_n(values.begin(), expected.countOrWriteWord, expectedWords.begin() + 1);
  auto error = ReadError::LENGTH_MISMATCH;
  const auto status =
      validateReadResponseExpected(rx, length, expected, words.data() + 1, capacity, error);
  CHECK(status.ok());
  CHECK(status.detail == 0);
  CHECK(error == ReadError::NONE);
  CHECK(words == expectedWords);
}

struct Failure {
  ReadError error;
  Err code;
  std::int32_t detail = 0;
};

constexpr Failure INVALID_ARGUMENT{ReadError::INVALID_ARGUMENT, Err::INVALID_CONFIG};
constexpr Failure TOO_SHORT{ReadError::FRAME_TOO_SHORT, Err::FRAME_ERROR};
constexpr Failure WRONG_LENGTH{ReadError::LENGTH_MISMATCH, Err::FRAME_ERROR};
constexpr Failure WRONG_CRC{ReadError::CRC_MISMATCH, Err::CRC_ERROR};
constexpr Failure WRONG_ADDRESS{ReadError::UNEXPECTED_ADDRESS, Err::FRAME_ERROR};
constexpr Failure WRONG_FUNCTION{ReadError::UNEXPECTED_FUNCTION, Err::FRAME_ERROR};
constexpr Failure WRONG_COUNT{ReadError::BYTE_COUNT_MISMATCH, Err::FRAME_ERROR};

void checkRejected(const std::uint8_t* rx, std::size_t length, const RequestContext& expected,
                   Failure failure, std::size_t capacity = 3) {
  auto words = PRIOR_WORDS;
  auto error = ReadError::NONE;
  const auto status =
      validateReadResponseExpected(rx, length, expected, words.data() + 1, capacity, error);
  CHECK(status.code == failure.code);
  CHECK(status.detail == failure.detail);
  CHECK(error == failure.error);
  CHECK(words == PRIOR_WORDS);
}

void checkFrames() {
  constexpr std::uint8_t crcInput[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(calcCrc16(crcInput, sizeof crcInput) == 0x4B37);
  CHECK(calcCrc16(crcInput, 0) == 0xFFFF);
  CHECK(calcCrc16(nullptr, 1) == 0xFFFF);

  for (const auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    for (const auto& test : READ_CASES) {
      const RequestContext expected{model, 1, READ, test.startRegister, test.count, STANDARD};
      const auto request = checkBuild(expected, test.tx, test.tx.size());
      const std::size_t rxLength = 5 + 2 * test.count;
      CHECK(calcCrc16(test.tx.data(), test.tx.size()) == 0);
      CHECK(calcCrc16(test.rx.data(), rxLength) == 0);
      alignas(std::uint16_t) std::uint8_t rx[13]{};
      std::copy_n(test.rx.begin(), rxLength, rx + 1);
      checkReply(rx + 1, rxLength, request, test.words, test.count);
      std::copy_n(test.rx.begin(), rxLength, rx + 2);
      checkReply(rx + 2, rxLength, request, test.words, test.words.size());
    }

    // FC03 replies omit the register address. The caller retains both identities.
    RequestFrame tx{};
    RequestContext realtime;
    CHECK(buildReadRegisters(model, 1, 0x0101, 1, tx.data(), tx.size(), realtime) == tx.size());
    CHECK(realtime.startRegister == 0x0101);
    const RequestContext expected{model,           1,       READ, PROCESSED.startRegister,
                                  PROCESSED.count, STANDARD};
    const auto processed = checkBuild(expected, PROCESSED.tx, PROCESSED.tx.size() + 1);
    checkReply(PROCESSED.rx.data(), 7, realtime, PROCESSED.words, 1);
    checkReply(PROCESSED.rx.data(), 7, processed, PROCESSED.words, 1);

    // With the default policy, rejection at 248 preserves the previous request.
    const auto priorTx = tx;
    const auto priorRequest = realtime;
    CHECK(buildReadRegisters(model, 248, 0x0100, 1, tx.data(), tx.size(), realtime) == 0);
    CHECK(tx == priorTx);
    CHECK(sameContext(realtime, priorRequest));
  }
}

void checkAddresses() {
  constexpr struct {
    std::uint8_t address;
    bool standard;
    bool isolated;
    std::array<std::uint8_t, 2> requestCrc;
    std::array<std::uint8_t, 2> replyCrc;
  } cases[] = {
      {0, false, false, {0x84, 0x27}, {0x05, 0x61}},
      {1, true, true, {0x85, 0xF6}, {0x38, 0xA1}},
      {247, true, true, {0x91, 0x60}, {0xF0, 0xB4}},
      {248, false, true, {0x91, 0x9F}, {0xA4, 0xB5}},
      {254, false, true, {0x91, 0xF9}, {0x2C, 0xB5}},
      {255, false, false, {0x90, 0x28}, {0x11, 0x75}},
  };
  for (const auto& test : cases) {
    const RequestFrame tx{test.address,      0x03, 0x01, 0x00, 0x00, 0x01, test.requestCrc[0],
                          test.requestCrc[1]};
    const std::uint8_t rx[] = {test.address,    0x03, 0x02, 0x02, 0xF2, test.replyCrc[0],
                               test.replyCrc[1]};
    CHECK(isValidAddress(test.address) == test.standard);
    for (const auto policy : {STANDARD, ISOLATED}) {
      const bool admitted = policy == STANDARD ? test.standard : test.isolated;
      CHECK(isValidAddress(test.address, policy) == admitted);
      const RequestContext expected{MODEL, test.address, READ, 0x0100, 1, policy};
      if (admitted) {
        const auto request = checkBuild(expected, tx, tx.size());
        checkReply(rx, sizeof rx, request, PROCESSED.words, 1);
      } else {
        checkBuildRejected(expected);
        checkRejected(rx, sizeof rx, expected, INVALID_ARGUMENT);
      }
    }
    if (test.address >= 248 && test.isolated) {
      checkRejected(rx, sizeof rx, EXPECTED, WRONG_ADDRESS);
    }
  }
}

void checkRejectedArguments() {
  constexpr RequestContext invalidSpans[] = {
      {},
      {static_cast<DeviceModel>(0xFF), 1, READ, 0x0100, 1, STANDARD},
      {MODEL, 1, READ, 0x0100, 1, INVALID_POLICY},
      {MODEL, 1, READ, 0x0100, 0, STANDARD},
      {MODEL, 1, READ, 0x0100, 126, STANDARD},
      {MODEL, 1, READ, 0xFFFF, 2, STANDARD},
      {MODEL, 1, READ, 0x0103, 1, STANDARD},
      {MODEL, 1, READ, 0x0101, 3, STANDARD},
      {MODEL, 1, READ, 0x0201, 5, STANDARD},
  };
  for (const auto& test : invalidSpans) {
    checkBuildRejected(test);
    checkRejected(PROCESSED.rx.data(), 7, test, INVALID_ARGUMENT);
  }
  CHECK(!isValidAddress(1, INVALID_POLICY));
  checkBuildRejected(EXPECTED, 0);
  checkBuildRejected(EXPECTED, 7);
  auto retained = PRIOR_CONTEXT;
  CHECK(buildReadRegisters(MODEL, 1, 0x0100, 1, nullptr, 8, retained) == 0);
  CHECK(sameContext(retained, PRIOR_CONTEXT));

  constexpr RequestContext write{MODEL, 1, WRITE, 0x0100, 1, STANDARD};
  checkRejected(PROCESSED.rx.data(), 7, write, INVALID_ARGUMENT);
  checkRejected(nullptr, 0, EXPECTED, INVALID_ARGUMENT);
  checkRejected(PROCESSED.rx.data(), 0, EXPECTED, INVALID_ARGUMENT, 0);
  checkRejected(MEASUREMENT_BLOCK.rx.data(), 11, BLOCK_CONTEXT, INVALID_ARGUMENT, 2);
  auto error = ReadError::NONE;
  CHECK(validateReadResponseExpected(PROCESSED.rx.data(), 7, EXPECTED, nullptr, 1, error).code ==
        Err::INVALID_CONFIG);
  CHECK(error == ReadError::INVALID_ARGUMENT);
}

void checkRejectedReplies() {
  checkRejected(PROCESSED.rx.data(), 0, EXPECTED, TOO_SHORT);
  checkRejected(PROCESSED.rx.data(), 4, EXPECTED, TOO_SHORT);

  constexpr std::uint8_t oversized[256]{};
  checkRejected(oversized, sizeof oversized, EXPECTED, WRONG_LENGTH);
  std::array<std::uint8_t, 255> wireLimit{0x01, 0x03, 250};
  wireLimit[253] = 0x08;
  wireLimit[254] = 0xE8;
  checkRejected(wireLimit.data(), wireLimit.size(), EXPECTED, WRONG_COUNT);

  static constexpr struct {
    std::initializer_list<std::uint8_t> rx;
    Failure failure;
  } cases[] = {
      {{0x01, 0x03, 0x02, 0x02, 0xF2, 0xA1, 0x38}, WRONG_CRC},
      {{0x02, 0x04, 0x02, 0x02, 0xF2, 0x38, 0xA1}, WRONG_CRC},
      {{0x02, 0x03, 0x02, 0x02, 0xF2, 0x7C, 0xA1}, WRONG_ADDRESS},
      {{0x02, 0x04, 0x02, 0x02, 0xF2, 0x7D, 0xD5}, WRONG_ADDRESS},
      {{0x01, 0x04, 0x02, 0x02, 0xF2, 0x39, 0xD5}, WRONG_FUNCTION},
      {{0x01, 0x03, 0x04, 0x02, 0xF2, 0xD8, 0xA0}, WRONG_COUNT},
      {{0x01, 0x03, 0x02, 0xA1, 0x31}, TOO_SHORT},
      {{0x01, 0x03, 0x02, 0x02, 0x71, 0x79}, TOO_SHORT},
      {{0x01, 0x03, 0x02, 0x02, 0xF2, 0x00, 0xA0, 0xD2}, WRONG_LENGTH},
      // A zero trailer preserves the CRC residue and still fails exact length.
      {{0x01, 0x03, 0x02, 0x02, 0xF2, 0x38, 0xA1, 0x00}, WRONG_LENGTH},
      {{0x01, 0x03, 0x02, 0x02, 0xF2, 0x38, 0xA1, 0x01}, WRONG_CRC},
      {{0x01, 0x83, 0xE1, 0x00, 0xB8, 0x60}, WRONG_LENGTH},
      {{0x01, 0x83, 0xE1, 0x78, 0x81}, WRONG_CRC},
      {{0xF8, 0x83, 0xE1, 0x51, 0x49}, WRONG_ADDRESS},
      {{0x01, 0x83, 0xE1, 0x81, 0x78}, {ReadError::EXCEPTION_RESPONSE, Err::EXCEPTION, 0xE1}},
  };
  for (const auto& test : cases) {
    checkRejected(test.rx.begin(), test.rx.size(), EXPECTED, test.failure);
  }
  checkRejected(PROCESSED.rx.data(), 7, BLOCK_CONTEXT, WRONG_COUNT);
  constexpr std::uint8_t blockRefusal[] = {0x01, 0x83, 0x02, 0xC0, 0xF1};
  checkRejected(blockRefusal, sizeof blockRefusal, BLOCK_CONTEXT,
                {ReadError::EXCEPTION_RESPONSE, Err::EXCEPTION, 0x02});
}

}  // namespace

int main() {
  checkFrames();
  checkAddresses();
  checkRejectedArguments();
  checkRejectedReplies();
}
