// clang-format off: Include the public header first to check self-containment.
#include <SEN059x/MeasurementCache.h>
// clang-format on

#include <array>
#include <cstdio>
#include <cstdlib>
#include <optional>

namespace {

using namespace SEN059x;
using Kind = MeasurementKind;
using Unit = MeasurementUnit;
using Reply = std::array<std::uint8_t, 7>;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

// Only an explicit read transmits. One scripted bus serves either device and gives the
// cache the retained request and the owner's completion time.
struct ScriptedOwner {
  SampleRequest request;
  unsigned transfers = 0;
  std::uint32_t nowMs = 0;
  ReadResponseError error{};

  void start(MeasurementCache& cache, Kind kind) {
    std::array<std::uint8_t, 8> tx{};
    CHECK(cache.buildRead(kind, tx.data(), tx.size(), request) == tx.size());
    const auto reg = static_cast<std::uint16_t>(kind);
    CHECK(tx[0] == request.context().address && tx[1] == 3 && tx[2] == (reg >> 8) &&
          tx[3] == (reg & 0xFF) && tx[4] == 0 && tx[5] == 1);
    ++transfers;
  }

  template <std::size_t N>
  Status reply(MeasurementCache& cache, const std::array<std::uint8_t, N>& bytes,
               bool timeValid = true) {
    return cache.complete(request, bytes.data(), bytes.size(), nowMs, timeValid, error);
  }
};

struct Example {
  Kind kind;
  Reply rx;
  std::uint16_t raw;
  Unit unit;
  double value;
};

// DFRobot examples 1-3, followed by the synthetic echo row from OPERATIONS.csv.
constexpr Example CASES[] = {
    {Kind::ProcessedDistance, {1, 3, 2, 2, 0xF2, 0x38, 0xA1}, 754, Unit::Millimetres, 754},
    {Kind::RealTimeDistance, {1, 3, 2, 2, 0xEF, 0xF8, 0xA8}, 751, Unit::Millimetres, 751},
    {Kind::Temperature, {1, 3, 2, 1, 0x2C, 0xB8, 0x09}, 300, Unit::DegreesCelsius, 30},
    {Kind::EchoTime, {1, 3, 2, 0x16, 0x76, 0x37, 0xC2}, 5750, Unit::Microseconds, 5750},
};

void checkNeverSampled(const SampleHistory& history) {
  CHECK(history.sampleGeneration == 0 && history.freshness == Freshness::NeverSampled);
  CHECK(!history.ageMs && !history.measurement.timeValid && !history.measurement.value);
}

void checkUsablePair(const SampleSnapshot& snapshot, DeviceModel model, std::uint8_t address,
                     const Example& example, std::uint32_t completedMs, bool timeValid,
                     Freshness freshness, std::optional<std::uint32_t> ageMs) {
  CHECK(snapshot.bindingGeneration != 0);
  CHECK(snapshot.raw.sampleGeneration != 0);
  CHECK(snapshot.raw.sampleGeneration == snapshot.usable.sampleGeneration);
  for (const auto* history : {&snapshot.raw, &snapshot.usable}) {
    const auto& reading = history->measurement;
    CHECK(reading.model == model && reading.address == address && reading.kind == example.kind);
    CHECK(reading.rawWord == example.raw && reading.unit == example.unit);
    CHECK(reading.quality == MeasurementQuality::Usable && reading.value == example.value);
    CHECK(reading.timeValid == timeValid && reading.completedMs == completedMs);
    CHECK(history->freshness == freshness && history->ageMs == ageMs);
  }
}

void checkOperationRows(DeviceModel model) {
  MeasurementCache cache;
  ScriptedOwner bus;
  CHECK(cache.bind(model, 1));
  CHECK(bus.transfers == 0);
  std::array<std::uint64_t, 4> generations{};
  for (const auto& example : CASES) {
    const auto empty = cache.snapshot(example.kind, bus.nowMs, 10);
    checkNeverSampled(empty.raw);
    checkNeverSampled(empty.usable);
    bus.start(cache, example.kind);
    CHECK(bus.reply(cache, example.rx));
    CHECK(bus.error == ReadResponseError::NONE);
    generations[bus.transfers - 1] =
        cache.snapshot(example.kind, bus.nowMs, 0).raw.sampleGeneration;
    ++bus.nowMs;
  }
  // Reading a later register must preserve each earlier register's time and sample identity.
  std::uint32_t completedMs = 0;
  for (const auto& example : CASES) {
    const auto retained = cache.snapshot(example.kind, bus.nowMs, 4);
    CHECK(retained.raw.sampleGeneration == generations[completedMs]);
    checkUsablePair(retained, model, 1, example, completedMs, true, Freshness::Fresh,
                    bus.nowMs - completedMs);
    ++completedMs;
  }
  CHECK(bus.transfers == 4);
}

constexpr auto SHORT_MODEL = DeviceModel::SEN0591;
constexpr auto LONG_MODEL = DeviceModel::SEN0592;
// Example 3's bytes interpreted under a processed-distance request.
constexpr Example SAME_DISTANCE{Kind::ProcessedDistance, CASES[2].rx, 300, Unit::Millimetres, 300};

void checkRepeatedReplyAndFailures() {
  ScriptedOwner bus;
  MeasurementCache shortDevice, longDevice;
  CHECK(shortDevice.bind(SHORT_MODEL, 1));
  CHECK(longDevice.bind(LONG_MODEL, 2));
  auto desiredKind = SAME_DISTANCE.kind;  // Local application selection for the next acquisition.
  CHECK(bus.transfers == 0);

  bus.start(shortDevice, desiredKind);
  desiredKind = Kind::Temperature;  // Selection can change while the distance reply is outstanding.
  checkNeverSampled(shortDevice.snapshot(desiredKind, bus.nowMs, 10).raw);
  CHECK(bus.transfers == 1);
  CHECK(bus.reply(shortDevice, SAME_DISTANCE.rx));
  const auto initial = shortDevice.snapshot(SAME_DISTANCE.kind, 0, 0);
  checkUsablePair(initial, SHORT_MODEL, 1, SAME_DISTANCE, 0, true, Freshness::Fresh, 0);
  bus.nowMs = 8;
  bus.start(shortDevice, desiredKind);
  CHECK(bus.reply(shortDevice, SAME_DISTANCE.rx));  // Same bytes, a different acquisition.
  for (std::uint32_t nowMs : {10u, 11u}) {
    checkUsablePair(shortDevice.snapshot(Kind::Temperature, nowMs, 10), SHORT_MODEL, 1, CASES[2], 8,
                    true, Freshness::Fresh, nowMs - 8);
    checkUsablePair(shortDevice.snapshot(SAME_DISTANCE.kind, nowMs, 10), SHORT_MODEL, 1,
                    SAME_DISTANCE, 0, true, nowMs == 10 ? Freshness::Fresh : Freshness::Stale,
                    nowMs);
  }
  CHECK(bus.transfers == 2);

  const auto checkRetained = [&] {
    const auto current = shortDevice.snapshot(SAME_DISTANCE.kind, bus.nowMs, 10);
    CHECK(current.bindingGeneration == initial.bindingGeneration);
    CHECK(current.raw.sampleGeneration == initial.raw.sampleGeneration);
    checkUsablePair(current, SHORT_MODEL, 1, SAME_DISTANCE, 0, true, Freshness::Stale, bus.nowMs);
  };
  bus.nowMs = 12;
  bus.start(shortDevice, SAME_DISTANCE.kind);
  constexpr std::array<std::uint8_t, 5> exception{1, 0x83, 0xE1, 0x81, 0x78};
  const auto status = bus.reply(shortDevice, exception);
  CHECK(status.code == Err::EXCEPTION && status.detail == 0xE1);
  CHECK(bus.error == ReadResponseError::EXCEPTION_RESPONSE);
  checkRetained();
  bus.nowMs = 13;
  bus.start(shortDevice, SAME_DISTANCE.kind);
  CHECK(shortDevice.discard(bus.request));  // Owner timeout, no response to decode.
  checkRetained();
  bus.nowMs = 14;
  bus.start(shortDevice, SAME_DISTANCE.kind);
  auto corrupt = SAME_DISTANCE.rx;
  corrupt.back() ^= 1;
  CHECK(bus.reply(shortDevice, corrupt).code == Err::CRC_ERROR);
  CHECK(bus.error == ReadResponseError::CRC_MISMATCH);
  checkRetained();

  bus.nowMs = 17;
  constexpr Example longDistance{
      Kind::ProcessedDistance, {2, 3, 2, 0x0B, 0xB9, 0x3A, 0xC6}, 3001, Unit::Millimetres, 3001};
  bus.start(longDevice, longDistance.kind);
  CHECK(bus.reply(longDevice, longDistance.rx));
  bus.nowMs = 20;
  bus.start(shortDevice, SAME_DISTANCE.kind);
  constexpr Reply outOfProfile{1, 3, 2, 0x0B, 0xB9, 0x7E, 0xC6};
  CHECK(bus.reply(shortDevice, outOfProfile));
  bus.nowMs = 21;
  const auto latest = shortDevice.snapshot(SAME_DISTANCE.kind, bus.nowMs, 10);
  CHECK(latest.bindingGeneration == initial.bindingGeneration);
  CHECK(latest.raw.sampleGeneration != 0 &&
        latest.raw.sampleGeneration != initial.raw.sampleGeneration);
  CHECK(latest.raw.freshness == Freshness::Unusable && latest.raw.ageMs == 1);
  const auto& raw = latest.raw.measurement;
  CHECK(raw.model == SHORT_MODEL && raw.address == 1 && raw.kind == SAME_DISTANCE.kind);
  CHECK(raw.rawWord == 3001 && raw.unit == Unit::Millimetres && !raw.value);
  CHECK(raw.quality == MeasurementQuality::OutOfProfile && raw.timeValid && raw.completedMs == 20);
  CHECK(latest.usable.sampleGeneration == initial.usable.sampleGeneration);
  CHECK(latest.usable.freshness == Freshness::Stale && latest.usable.ageMs == 21);
  const auto& usable = latest.usable.measurement;
  CHECK(usable.model == SHORT_MODEL && usable.address == 1 && usable.kind == SAME_DISTANCE.kind);
  CHECK(usable.quality == MeasurementQuality::Usable && usable.unit == Unit::Millimetres);
  CHECK(usable.rawWord == 300 && usable.value == 300 && usable.timeValid &&
        usable.completedMs == 0);
  checkUsablePair(longDevice.snapshot(longDistance.kind, bus.nowMs, 10), LONG_MODEL, 2,
                  longDistance, 17, true, Freshness::Fresh, 4);
  CHECK(bus.transfers == 7);
}

void checkClockBoundaries() {
  ScriptedOwner bus;
  MeasurementCache cache;
  CHECK(cache.bind(SHORT_MODEL, 1));
  const auto& echo = CASES[3];
  bus.start(cache, echo.kind);
  CHECK(bus.reply(cache, echo.rx, false));
  checkUsablePair(cache.snapshot(echo.kind, 0, 0), SHORT_MODEL, 1, echo, 0, false,
                  Freshness::UnknownTime, std::nullopt);
  bus.nowMs = 0xFFFFFFFCu;
  bus.start(cache, echo.kind);
  CHECK(bus.reply(cache, echo.rx));
  checkUsablePair(cache.snapshot(echo.kind, bus.nowMs, 0), SHORT_MODEL, 1, echo, bus.nowMs, true,
                  Freshness::Fresh, 0);
  checkUsablePair(cache.snapshot(echo.kind, bus.nowMs + 1, 0), SHORT_MODEL, 1, echo, bus.nowMs,
                  true, Freshness::Stale, 1);
  checkUsablePair(cache.snapshot(echo.kind, 4, 8), SHORT_MODEL, 1, echo, bus.nowMs, true,
                  Freshness::Fresh, 8);
  checkUsablePair(cache.snapshot(echo.kind, 5, 8), SHORT_MODEL, 1, echo, bus.nowMs, true,
                  Freshness::Stale, 9);
  CHECK(bus.transfers == 2);
}

}  // namespace

int main() {
  checkOperationRows(DeviceModel::SEN0591);
  checkOperationRows(DeviceModel::SEN0592);
  checkRepeatedReplyAndFailures();
  checkClockBoundaries();
}
