// clang-format off: Include the public header first to check self-containment.
#include <SEN059x/WriteCodec.h>
// clang-format on

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#include "../../internal/WriteCodecInternal.h"

#include <SEN059x/SEN059x.h>

namespace {

using namespace SEN059x;
using WriteError = WriteResponseError;
using WriteFrame = std::array<std::uint8_t, 8>;

constexpr auto WRITE = FunctionCode::WriteSingleRegister;
constexpr auto STANDARD = AddressPolicy::StandardUnicast;
constexpr auto ISOLATED = AddressPolicy::DFRobotIsolated;
constexpr auto INVALID_POLICY = static_cast<AddressPolicy>(0xFF);
constexpr DeviceModel MODELS[] = {DeviceModel::SEN0591, DeviceModel::SEN0592};
constexpr RequestContext PRIOR_CONTEXT{DeviceModel::SEN0592, 254, WRITE, 0x0206, 750, ISOLATED};
constexpr std::uint8_t BYTE_CANARY = 0xA5;

// Keep checks enabled in Release builds.
#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

struct WriteCase {
  std::uint16_t reg;
  std::uint16_t word;
  WriteFrame frame;
};

// DFRobot examples 4 and 6 supply these identical request and reply bytes.
constexpr WriteCase SET_ADDRESS{0x0200, 5, {0x01, 0x06, 0x02, 0x00, 0x00, 0x05, 0x48, 0x71}};
constexpr WriteCase SET_BAUD{0x0201, 3, {0x01, 0x06, 0x02, 0x01, 0x00, 0x03, 0x99, 0xB3}};

// The remaining words come from OPERATIONS.csv with independently computed CRCs.
constexpr WriteCase WRITES[] = {
    SET_ADDRESS,
    SET_BAUD,
    {0x0205, 0, {0x01, 0x06, 0x02, 0x05, 0x00, 0x00, 0x98, 0x73}},
    {0x0206, 750, {0x01, 0x06, 0x02, 0x06, 0x02, 0xEE, 0xE9, 0x5F}},
    {0x0208, 2, {0x01, 0x06, 0x02, 0x08, 0x00, 0x02, 0x88, 0x71}},
    {0x0209, 1, {0x01, 0x06, 0x02, 0x09, 0x00, 0x01, 0x99, 0xB0}},
    {0x021A, 4, {0x01, 0x06, 0x02, 0x1A, 0x00, 0x04, 0xA8, 0x76}},
    {0x021F, 3, {0x01, 0x06, 0x02, 0x1F, 0x00, 0x03, 0xF9, 0xB5}},
};

constexpr WriteFrame VENDOR_FRAME{0xFF, 0x06, 0x02, 0x00, 0x00, 0x05, 0x5D, 0xAF};

struct Result {
  WriteError error;
  Err code;
  std::int32_t detail = 0;
};

constexpr Result OK_SHAPE{WriteError::NONE, Err::OK};
constexpr Result INVALID_ARGUMENT{WriteError::INVALID_ARGUMENT, Err::INVALID_CONFIG};
constexpr Result TOO_SHORT{WriteError::FRAME_TOO_SHORT, Err::FRAME_ERROR};
constexpr Result WRONG_LENGTH{WriteError::LENGTH_MISMATCH, Err::FRAME_ERROR};
constexpr Result WRONG_CRC{WriteError::CRC_MISMATCH, Err::CRC_ERROR};
constexpr Result WRONG_ADDRESS{WriteError::UNEXPECTED_ADDRESS, Err::FRAME_ERROR};
constexpr Result WRONG_FUNCTION{WriteError::UNEXPECTED_FUNCTION, Err::FRAME_ERROR};
constexpr Result WRONG_REGISTER{WriteError::REGISTER_MISMATCH, Err::FRAME_ERROR};
constexpr Result WRONG_VALUE{WriteError::VALUE_MISMATCH, Err::FRAME_ERROR};
constexpr Result KNOWN_EXCEPTION{WriteError::EXCEPTION_RESPONSE, Err::EXCEPTION, 0x02};
constexpr Result UNKNOWN_EXCEPTION{WriteError::EXCEPTION_RESPONSE, Err::EXCEPTION, 0xA5};

bool sameContext(const RequestContext& actual, const RequestContext& expected) {
  return actual.model == expected.model && actual.address == expected.address &&
         actual.function == expected.function && actual.startRegister == expected.startRegister &&
         actual.countOrWriteWord == expected.countOrWriteWord &&
         actual.addressPolicy == expected.addressPolicy;
}

void checkReply(const std::uint8_t* rx, std::size_t rxLength, const RequestContext& expected,
                Result result) {
  auto retained = expected;
  auto error = result.error == WriteError::NONE ? WriteError::VALUE_MISMATCH : WriteError::NONE;
  const auto status = parseWriteSingleRegister(rx, rxLength, retained, error);
  CHECK(status.code == result.code);
  CHECK(status.detail == result.detail);
  CHECK(error == result.error);
  CHECK(sameContext(retained, expected));
}

void checkAddressed(const RequestContext& expected, const WriteFrame& frame, std::size_t capacity) {
  alignas(std::uint16_t) std::array<std::uint8_t, 11> tx;
  tx.fill(BYTE_CANARY);
  auto expectedTx = tx;
  std::copy(frame.begin(), frame.end(), expectedTx.begin() + 1);
  auto retained = PRIOR_CONTEXT;
  CHECK(detail::encodeAddressedWriteSingleRegister(
            expected.model, expected.address, expected.startRegister, expected.countOrWriteWord,
            tx.data() + 1, capacity, retained, expected.addressPolicy) == frame.size());
  CHECK(tx == expectedTx);
  CHECK(sameContext(retained, expected));
  checkReply(frame.data(), frame.size(), retained, OK_SHAPE);

  // A local transmit echo has the same shape as the published replies.
  checkReply(tx.data() + 1, frame.size(), retained, OK_SHAPE);
  CHECK(tx == expectedTx);
}

void checkAddressedRejected(const RequestContext& input, std::size_t capacity = 8) {
  std::array<std::uint8_t, 10> tx;
  tx.fill(BYTE_CANARY);
  const auto saved = tx;
  auto retained = PRIOR_CONTEXT;
  CHECK(detail::encodeAddressedWriteSingleRegister(input.model, input.address, input.startRegister,
                                                   input.countOrWriteWord, tx.data() + 1, capacity,
                                                   retained, input.addressPolicy) == 0);
  CHECK(tx == saved);
  CHECK(sameContext(retained, PRIOR_CONTEXT));
}

void checkIsolatedRejected(std::uint16_t reg, std::size_t capacity) {
  std::array<std::uint8_t, 10> tx;
  tx.fill(BYTE_CANARY);
  const auto saved = tx;
  CHECK(detail::encodeIsolatedWriteSingleRegister(reg, 5, tx.data() + 1, capacity) == 0);
  CHECK(tx == saved);
}

void checkAddressPolicies(DeviceModel model) {
  constexpr struct {
    bool standard;
    bool isolated;
    WriteFrame frame;
  } cases[] = {
      {false, false, {0x00, 0x06, 0x02, 0x00, 0x00, 0x05, 0x49, 0xA0}},
      {true, true, SET_ADDRESS.frame},
      {true, true, {0xF7, 0x06, 0x02, 0x00, 0x00, 0x05, 0x5C, 0xE7}},
      {false, true, {0xF8, 0x06, 0x02, 0x00, 0x00, 0x05, 0x5C, 0x18}},
      {false, true, {0xFE, 0x06, 0x02, 0x00, 0x00, 0x05, 0x5C, 0x7E}},
      {false, false, VENDOR_FRAME},
  };
  for (const auto& test : cases) {
    for (const auto policy : {STANDARD, ISOLATED}) {
      const RequestContext request{model,           test.frame[0],    WRITE,
                                   SET_ADDRESS.reg, SET_ADDRESS.word, policy};
      const bool admitted = policy == STANDARD ? test.standard : test.isolated;
      if (admitted) {
        checkAddressed(request, test.frame, test.frame.size() + 1);
      } else {
        checkAddressedRejected(request);
        checkReply(test.frame.data(), test.frame.size(), request, INVALID_ARGUMENT);
      }
    }
  }

  constexpr std::uint8_t extendedException[] = {0xFE, 0x86, 0x02, 0xF3, 0x91};
  const RequestContext isolated{model, 254, WRITE, SET_ADDRESS.reg, SET_ADDRESS.word, ISOLATED};
  checkReply(extendedException, sizeof extendedException, isolated, KNOWN_EXCEPTION);
}

void checkRejectedArguments(DeviceModel model) {
  constexpr WriteCase rejectedRegisters[] = {
      {0x0100, 5, {0x01, 0x06, 0x01, 0x00, 0x00, 0x05, 0x48, 0x35}},
      {0x0101, 5, {0x01, 0x06, 0x01, 0x01, 0x00, 0x05, 0x19, 0xF5}},
      {0x0102, 5, {0x01, 0x06, 0x01, 0x02, 0x00, 0x05, 0xE9, 0xF5}},
      {0x010A, 5, {0x01, 0x06, 0x01, 0x0A, 0x00, 0x05, 0x68, 0x37}},
      {0x0202, 5, {0x01, 0x06, 0x02, 0x02, 0x00, 0x05, 0xE9, 0xB1}},
  };
  for (const auto& test : rejectedRegisters) {
    const RequestContext request{model, 1, WRITE, test.reg, test.word, STANDARD};
    checkAddressedRejected(request);
    checkIsolatedRejected(test.reg, 8);
    checkReply(test.frame.data(), test.frame.size(), request, INVALID_ARGUMENT);
  }

  const RequestContext valid{model, 1, WRITE, SET_ADDRESS.reg, SET_ADDRESS.word, STANDARD};
  auto invalidModel = valid;
  invalidModel.model = static_cast<DeviceModel>(0xFF);
  auto invalidPolicy = valid;
  invalidPolicy.addressPolicy = INVALID_POLICY;
  for (const auto& request : {invalidModel, invalidPolicy}) {
    checkAddressedRejected(request);
    checkReply(SET_ADDRESS.frame.data(), SET_ADDRESS.frame.size(), request, INVALID_ARGUMENT);
  }

  auto readContext = valid;
  readContext.function = FunctionCode::ReadHoldingRegisters;
  checkReply(SET_ADDRESS.frame.data(), SET_ADDRESS.frame.size(), readContext, INVALID_ARGUMENT);
  checkReply(SET_ADDRESS.frame.data(), SET_ADDRESS.frame.size(), {}, INVALID_ARGUMENT);
  checkReply(nullptr, SET_ADDRESS.frame.size(), valid, INVALID_ARGUMENT);

  for (std::size_t capacity = 0; capacity < SET_ADDRESS.frame.size(); ++capacity) {
    checkAddressedRejected(valid, capacity);
    checkIsolatedRejected(SET_ADDRESS.reg, capacity);
  }
  auto retained = PRIOR_CONTEXT;
  CHECK(detail::encodeAddressedWriteSingleRegister(model, 1, SET_ADDRESS.reg, SET_ADDRESS.word,
                                                   nullptr, 8, retained) == 0);
  CHECK(sameContext(retained, PRIOR_CONTEXT));
  CHECK(detail::encodeIsolatedWriteSingleRegister(SET_ADDRESS.reg, SET_ADDRESS.word, nullptr, 8) ==
        0);
}

void checkRejectedReplies(DeviceModel model) {
  // CRC-valid mismatches vary both high and low halves independently.
  static constexpr struct {
    std::initializer_list<std::uint8_t> rx;
    Result result;
  } cases[] = {
      {{0x01, 0x06, 0x02, 0x00, 0x00, 0x05, 0x71, 0x48}, WRONG_CRC},
      {{0x01, 0x06, 0x02, 0x01, 0x00, 0x05, 0x19, 0xB1}, WRONG_REGISTER},
      {{0x01, 0x06, 0x03, 0x00, 0x00, 0x05, 0x49, 0x8D}, WRONG_REGISTER},
      {{0x01, 0x06, 0x02, 0x00, 0x00, 0x06, 0x08, 0x70}, WRONG_VALUE},
      {{0x01, 0x06, 0x02, 0x00, 0x01, 0x05, 0x49, 0xE1}, WRONG_VALUE},
      {{0x05, 0x06, 0x02, 0x00, 0x00, 0x05, 0x49, 0xF5}, WRONG_ADDRESS},
      {{0x01, 0x03, 0x02, 0x00, 0x00, 0x05, 0x84, 0x71}, WRONG_FUNCTION},
      {{0x01, 0x83, 0x02, 0xC0, 0xF1}, WRONG_FUNCTION},
      {{0x01, 0x06, 0x02, 0xA2, 0x61}, TOO_SHORT},
      {{0x01, 0x06, 0x02, 0x00, 0x00, 0xB8, 0x88}, TOO_SHORT},
      // Both the printed English CRC and a repaired CRC leave a short frame.
      {{0x01, 0x06, 0x02, 0x00, 0x05, 0x48, 0x71}, WRONG_CRC},
      {{0x01, 0x06, 0x02, 0x00, 0x05, 0x78, 0x8B}, TOO_SHORT},
      {{0x01, 0x06, 0x02, 0x00, 0x00, 0x05, 0x00, 0x71, 0x36}, WRONG_LENGTH},
      {{0x01, 0x06, 0x02, 0x00, 0x00, 0x05, 0x48, 0x71, 0x00}, WRONG_LENGTH},
      {{0x01, 0x86, 0x81, 0x82}, TOO_SHORT},
      {{0x01, 0x86, 0x02, 0x00, 0xE1, 0x51}, WRONG_LENGTH},
      {{0x01, 0x86, 0x02, 0xA1, 0xC3}, WRONG_CRC},
      {{0x02, 0x86, 0x02, 0x33, 0xA1}, WRONG_ADDRESS},
      {{0x01, 0x86, 0x02, 0xC3, 0xA1}, KNOWN_EXCEPTION},
      {{0x01, 0x86, 0xA5, 0x82, 0x1B}, UNKNOWN_EXCEPTION},
  };
  const RequestContext request{model, 1, WRITE, SET_ADDRESS.reg, SET_ADDRESS.word, STANDARD};
  checkReply(SET_ADDRESS.frame.data(), 0, request, TOO_SHORT);
  checkReply(VENDOR_FRAME.data(), VENDOR_FRAME.size(), request, WRONG_ADDRESS);
  for (const auto& test : cases) {
    checkReply(test.rx.begin(), test.rx.size(), request, test.result);
  }
}

void checkFixedDestination() {
  std::array<std::uint8_t, 11> tx;
  tx.fill(BYTE_CANARY);
  auto expectedTx = tx;
  std::copy(VENDOR_FRAME.begin(), VENDOR_FRAME.end(), expectedTx.begin() + 1);
  CHECK(detail::encodeIsolatedWriteSingleRegister(SET_ADDRESS.reg, SET_ADDRESS.word, tx.data() + 1,
                                                  VENDOR_FRAME.size() + 1) == VENDOR_FRAME.size());
  CHECK(tx == expectedTx);
}

}  // namespace

int main() {
  for (const auto model : MODELS) {
    for (const auto& test : WRITES) {
      checkAddressed({model, 1, WRITE, test.reg, test.word, STANDARD}, test.frame,
                     test.frame.size());
    }
    // The private codec accepts raw words. The typed builder will apply model limits.
    constexpr WriteFrame raw{0x01, 0x06, 0x02, 0x08, 0xFF, 0xFF, 0x08, 0x00};
    checkAddressed({model, 1, WRITE, 0x0208, 0xFFFF, STANDARD}, raw, raw.size());
    checkAddressPolicies(model);
    checkRejectedArguments(model);
    checkRejectedReplies(model);
  }
  checkFixedDestination();
}
