// clang-format off: Include the public header first to check self-containment.
#include <SEN059x/Measurements.h>
// clang-format on

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <type_traits>

#include <SEN059x/ReadCodec.h>

namespace {

using namespace SEN059x;
using Kind = MeasurementKind;
using Quality = MeasurementQuality;
using Unit = MeasurementUnit;
using Tx = std::array<std::uint8_t, 8>;
using Reply = std::array<std::uint8_t, 7>;

// Raw diagnostics (including one word), settings and block reads retain a RequestContext.
// In C++17, braces can initialize an aggregate even when is_constructible is false.
static_assert(!std::is_aggregate_v<MeasurementRequest>);
static_assert(!std::is_constructible_v<MeasurementRequest, RequestContext>);
static_assert(!std::is_invocable_v<decltype(decodeMeasurementResponse), const std::uint8_t*,
                                   std::size_t, const RequestContext&, std::uint32_t, bool,
                                   Measurement&, ReadResponseError&>);

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

// DFRobot examples 1–3 from modbus_vectors.csv. The echo pair is synthetic,
// using the 0x010A operation in OPERATIONS.csv.
struct Example {
  Kind kind;
  Tx tx;
  Reply rx;
  std::uint16_t word;
  Unit unit;
  double value;
};

constexpr Example EXAMPLES[] = {
    {Kind::ProcessedDistance,
     {1, 3, 1, 0, 0, 1, 0x85, 0xF6},
     {1, 3, 2, 2, 0xF2, 0x38, 0xA1},
     0x02F2,
     Unit::Millimetres,
     754},
    {Kind::RealTimeDistance,
     {1, 3, 1, 1, 0, 1, 0xD4, 0x36},
     {1, 3, 2, 2, 0xEF, 0xF8, 0xA8},
     0x02EF,
     Unit::Millimetres,
     751},
    {Kind::Temperature,
     {1, 3, 1, 2, 0, 1, 0x24, 0x36},
     {1, 3, 2, 1, 0x2C, 0xB8, 0x09},
     0x012C,
     Unit::DegreesCelsius,
     30.0},
    {Kind::EchoTime,
     {1, 3, 1, 0x0A, 0, 1, 0xA5, 0xF4},
     {1, 3, 2, 0x16, 0x76, 0x37, 0xC2},
     0x1676,
     Unit::Microseconds,
     5750},
};

MeasurementRequest requestFor(DeviceModel model, const Example& example) {
  MeasurementRequest request;
  Tx tx{};
  CHECK(buildMeasurementRead(model, 1, example.kind, tx.data(), tx.size(), request) == tx.size());
  CHECK(tx == example.tx);
  const auto& context = request.context();
  CHECK(context.model == model && context.address == 1);
  CHECK(context.function == FunctionCode::ReadHoldingRegisters && context.countOrWriteWord == 1);
  CHECK(context.startRegister == static_cast<std::uint16_t>(example.kind));
  CHECK(context.addressPolicy == AddressPolicy::StandardUnicast);
  return request;
}

Measurement decode(const MeasurementRequest& request, const Reply& rx, std::uint32_t completedMs,
                   bool timeValid) {
  Measurement result;
  auto error = ReadResponseError::INVALID_ARGUMENT;
  const auto status = decodeMeasurementResponse(rx.data(), rx.size(), request, completedMs,
                                                timeValid, result, error);
  CHECK(status.ok() && status.detail == 0 && error == ReadResponseError::NONE);
  CHECK(result.model == request.context().model && result.address == request.context().address);
  CHECK(static_cast<std::uint16_t>(result.kind) == request.context().startRegister);
  CHECK(result.timeValid == timeValid && result.completedMs == completedMs);
  return result;
}

void checkExamples(DeviceModel model) {
  for (const auto& example : EXAMPLES) {
    const auto request = requestFor(model, example);
    for (bool known : {true, false}) {
      const auto result = decode(request, example.rx, 0, known);
      CHECK(result.rawWord == example.word && result.unit == example.unit);
      CHECK(result.quality == Quality::Usable && result.value == example.value);
    }
  }
  // Example 3's identical reply means 300 mm under this retained request.
  const auto distance = decode(requestFor(model, EXAMPLES[0]), EXAMPLES[2].rx, 42, true);
  CHECK(distance.rawWord == 0x012C && distance.unit == Unit::Millimetres && distance.value == 300);
}

// Vary the payload in a complete synthetic single-register reply for boundary tests.
Reply wordReply(std::uint16_t word) {
  Reply rx{1, 3, 2, static_cast<std::uint8_t>(word >> 8), static_cast<std::uint8_t>(word)};
  const auto crc = calcCrc16(rx.data(), 5);
  rx[5] = static_cast<std::uint8_t>(crc);
  rx[6] = static_cast<std::uint8_t>(crc >> 8);
  return rx;
}

struct WordCase {
  std::uint16_t word;
  Quality quality;
  std::optional<double> value;
};

void checkWord(const MeasurementRequest& request, Unit expectedUnit, const WordCase& example) {
  const auto result = decode(request, wordReply(example.word), 12345, true);
  CHECK(result.unit == expectedUnit && result.rawWord == example.word);
  CHECK(result.quality == example.quality && result.value == example.value);
}

void checkBoundaries(DeviceModel model) {
  // Manufacturer product ratings, independent of 0x021F range-level distances.
  for (const auto* distance : {&EXAMPLES[0], &EXAMPLES[1]}) {
    const auto request = requestFor(model, *distance);
    constexpr struct {
      std::uint16_t mm;
      bool within3mRating;
      bool within5mRating;
    } cases[] = {
        {0, false, false},    {19, false, false},     {20, true, false},   {29, true, false},
        {30, true, true},     {3000, true, true},     {3001, false, true}, {5000, false, true},
        {5001, false, false}, {0xFFFF, false, false},
    };
    for (const auto& test : cases) {
      if (model == DeviceModel::SEN0591 ? test.within3mRating : test.within5mRating) {
        checkWord(request, Unit::Millimetres, {test.mm, Quality::Usable, test.mm});
      } else {
        checkWord(request, Unit::Millimetres, {test.mm, Quality::OutOfProfile, {}});
      }
    }
  }

  const auto temperature = requestFor(model, EXAMPLES[2]);
  CHECK(TEMPERATURE_RESOLUTION_C == 0.5);
  checkWord(temperature, Unit::DegreesCelsius, {0, Quality::Usable, 0});
  checkWord(temperature, Unit::DegreesCelsius, {305, Quality::Usable, 30.5});
  checkWord(temperature, Unit::DegreesCelsius, {301, Quality::UnexpectedQuantization, {}});
  checkWord(temperature, Unit::DegreesCelsius, {650, Quality::Usable, 65.0});
  checkWord(temperature, Unit::DegreesCelsius, {651, Quality::AmbiguousTemperature, {}});
  checkWord(temperature, Unit::DegreesCelsius, {655, Quality::AmbiguousTemperature, {}});
  checkWord(temperature, Unit::DegreesCelsius, {0xFFFF, Quality::AmbiguousTemperature, {}});
  const auto echoRequest = requestFor(model, EXAMPLES[3]);
  // Neither manual assigns a missing-echo code to either end of the echo-time word range.
  checkWord(echoRequest, Unit::Microseconds, {0, Quality::Usable, 0});
  checkWord(echoRequest, Unit::Microseconds, {0xFFFF, Quality::Usable, 65535});
  const auto echo = decode(echoRequest, wordReply(5751), 7, true);
  CHECK(echo.unit == Unit::Microseconds && echo.rawWord == 5751 && echo.value == 5751);
  CHECK(std::abs(approximateEchoDistanceMm(echo.rawWord) - 1000.17391304348) < 1e-8);
}

void checkReusedObservation(DeviceModel model) {
  const auto distance = requestFor(model, EXAMPLES[0]);
  auto result = decode(distance, EXAMPLES[0].rx, 42, true);
  CHECK(result.value == 754);

  const auto temperature = requestFor(model, EXAMPLES[2]);
  const auto rx = wordReply(0xFFFF);
  auto error = ReadResponseError::INVALID_ARGUMENT;
  CHECK(decodeMeasurementResponse(rx.data(), rx.size(), temperature, 0, false, result, error));
  CHECK(error == ReadResponseError::NONE && result.kind == Kind::Temperature);
  CHECK(result.rawWord == 0xFFFF && result.unit == Unit::DegreesCelsius);
  CHECK(result.quality == Quality::AmbiguousTemperature && !result.value.has_value());
  CHECK(!result.timeValid && result.completedMs == 0);
}

void checkRejectedRequests(DeviceModel model) {
  auto request = requestFor(model, EXAMPLES[2]);
  const auto prior = request.context();
  Tx tx{};
  tx.fill(0xA5);
  const auto sentinel = tx;
  const auto checkPreserved = [&] {
    const auto& current = request.context();
    CHECK(tx == sentinel && current.model == prior.model && current.address == prior.address);
    CHECK(current.function == prior.function && current.startRegister == prior.startRegister);
    CHECK(current.addressPolicy == prior.addressPolicy &&
          current.countOrWriteWord == prior.countOrWriteWord);
  };
  for (const auto invalid : {0x0103, 0x0200}) {
    CHECK(buildMeasurementRead(model, 2, static_cast<Kind>(invalid), tx.data(), tx.size(),
                               request) == 0);
    checkPreserved();
  }
  CHECK(buildMeasurementRead(model, 248, Kind::ProcessedDistance, tx.data(), tx.size(), request) ==
        0);
  checkPreserved();
  CHECK(buildMeasurementRead(model, 2, Kind::EchoTime, tx.data(), 7, request) == 0);
  checkPreserved();
  CHECK(buildMeasurementRead(model, 2, Kind::EchoTime, nullptr, tx.size(), request) == 0);
  checkPreserved();
  CHECK(buildMeasurementRead(static_cast<DeviceModel>(0), 2, Kind::EchoTime, tx.data(), tx.size(),
                             request) == 0);
  checkPreserved();

  CHECK(buildMeasurementRead(model, 248, Kind::ProcessedDistance, tx.data(), tx.size(), request,
                             AddressPolicy::DFRobotIsolated) == tx.size());
  constexpr Tx isolatedTx{0xF8, 3, 1, 0, 0, 1, 0x91, 0x9F};
  constexpr Reply isolatedRx{0xF8, 3, 2, 2, 0xF2, 0xA4, 0xB5};
  CHECK(tx == isolatedTx && request.context().addressPolicy == AddressPolicy::DFRobotIsolated);
  const auto result = decode(request, isolatedRx, 0xFEDCBA98, true);
  CHECK(result.address == 248 && result.value == 754);
}

void checkRejectedReplies(DeviceModel model) {
  const auto request = requestFor(model, EXAMPLES[0]);
  const auto prior = decode(requestFor(model, EXAMPLES[2]), EXAMPLES[2].rx, 99, true);
  const auto reject = [&](const MeasurementRequest& expected, const std::uint8_t* rx,
                          std::size_t size, Err code, ReadResponseError reason,
                          std::int32_t detail = 0) {
    auto result = prior;
    auto error = ReadResponseError::NONE;
    const auto status = decodeMeasurementResponse(rx, size, expected, 101, false, result, error);
    CHECK(status.code == code && status.detail == detail && error == reason);
    CHECK(result.model == prior.model && result.address == prior.address &&
          result.kind == prior.kind);
    CHECK(result.rawWord == prior.rawWord && result.unit == prior.unit &&
          result.quality == prior.quality);
    CHECK(result.value == prior.value && result.completedMs == prior.completedMs &&
          result.timeValid == prior.timeValid);
  };
  const auto& valid = EXAMPLES[0].rx;
  reject({}, valid.data(), valid.size(), Err::INVALID_CONFIG, ReadResponseError::INVALID_ARGUMENT);
  reject(request, nullptr, 0, Err::INVALID_CONFIG, ReadResponseError::INVALID_ARGUMENT);
  reject(request, valid.data(), 4, Err::FRAME_ERROR, ReadResponseError::FRAME_TOO_SHORT);
  constexpr Reply corrupt{1, 3, 2, 2, 0xF2, 0xA1, 0x38};
  constexpr Reply foreign{2, 3, 2, 2, 0xF2, 0x7C, 0xA1};
  constexpr std::uint8_t padded[]{1, 3, 2, 2, 0xF2, 0x38, 0xA1, 0};
  constexpr std::uint8_t exception[]{1, 0x83, 0xE1, 0x81, 0x78};
  reject(request, corrupt.data(), corrupt.size(), Err::CRC_ERROR, ReadResponseError::CRC_MISMATCH);
  reject(request, foreign.data(), foreign.size(), Err::FRAME_ERROR,
         ReadResponseError::UNEXPECTED_ADDRESS);
  reject(request, padded, sizeof padded, Err::FRAME_ERROR, ReadResponseError::LENGTH_MISMATCH);
  reject(request, exception, sizeof exception, Err::EXCEPTION,
         ReadResponseError::EXCEPTION_RESPONSE, 0xE1);
}

}  // namespace

int main() {
  for (const auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    checkExamples(model);
    checkBoundaries(model);
    checkReusedObservation(model);
    checkRejectedRequests(model);
    checkRejectedReplies(model);
  }
}
