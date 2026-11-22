// clang-format off: Compile the public header in isolation.
#include <SEN059x/Measurements.h>
// clang-format on

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

using namespace SEN059x;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

constexpr auto PROCESSED = MeasurementKind::ProcessedDistance;
constexpr auto REAL_TIME = MeasurementKind::RealTimeDistance;
constexpr auto TEMPERATURE = MeasurementKind::Temperature;
constexpr auto ECHO = MeasurementKind::EchoTime;
constexpr auto UNKNOWN_MODEL = static_cast<DeviceModel>(0);
constexpr auto UNKNOWN_REGISTER = static_cast<MeasurementKind>(0x0200);
constexpr MeasurementTiming PRIOR{11, 22, 33};

bool sameTiming(const MeasurementTiming& lhs, const MeasurementTiming& rhs) {
  return lhs.englishMinMs == rhs.englishMinMs && lhs.chineseMinMs == rhs.chineseMinMs &&
         lhs.maxMs == rhs.maxMs;
}

void checkGeneralTiming() {
  // Table 1 in the English and Chinese editions, in register order for each SKU.
  constexpr struct {
    DeviceModel model;
    MeasurementKind kind;
    MeasurementTiming expected;
  } cases[] = {
      {DeviceModel::SEN0591, PROCESSED, {190, 190, 750}},
      {DeviceModel::SEN0591, REAL_TIME, {15, 15, 140}},
      {DeviceModel::SEN0591, TEMPERATURE, {5, 5, 140}},
      {DeviceModel::SEN0591, ECHO, {5, 5, 140}},
      {DeviceModel::SEN0592, PROCESSED, {190, 300, 750}},
      {DeviceModel::SEN0592, REAL_TIME, {15, 15, 140}},
      {DeviceModel::SEN0592, TEMPERATURE, {5, 40, 140}},
      {DeviceModel::SEN0592, ECHO, {5, 40, 140}},
  };
  for (const auto& item : cases) {
    MeasurementTiming out = PRIOR;
    CHECK(measurementTiming(item.model, item.kind, out));
    CHECK(sameTiming(out, item.expected));
  }
  MeasurementTiming out = PRIOR;
  CHECK(!measurementTiming(UNKNOWN_MODEL, PROCESSED, out));
  CHECK(sameTiming(out, PRIOR));
  for (auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    CHECK(!measurementTiming(model, UNKNOWN_REGISTER, out));
    CHECK(sameTiming(out, PRIOR));
  }
}

void checkLevelTiming() {
  // Table 2, SEN0591 0x021F. Each range level lists real-time then processed.
  constexpr struct {
    std::uint16_t level;
    MeasurementTiming realTime;
    MeasurementTiming processed;
  } cases[] = {
      {1, {15, 15, 80}, {190, 190, 500}},
      {2, {20, 20, 90}, {230, 230, 550}},
      {3, {25, 25, 100}, {250, 250, 600}},
      {4, {35, 35, 110}, {280, 280, 650}},
  };
  for (const auto& item : cases) {
    MeasurementTiming out = PRIOR;
    CHECK(rangeLevelTiming(DeviceModel::SEN0591, REAL_TIME, item.level, out));
    CHECK(sameTiming(out, item.realTime));
    CHECK(rangeLevelTiming(DeviceModel::SEN0591, PROCESSED, item.level, out));
    CHECK(sameTiming(out, item.processed));
  }

  MeasurementTiming out = PRIOR;
  for (auto kind : {REAL_TIME, PROCESSED, TEMPERATURE, ECHO}) {
    for (std::uint16_t level = 1; level <= 5; ++level) {
      CHECK(!rangeLevelTiming(DeviceModel::SEN0592, kind, level, out));
      CHECK(sameTiming(out, PRIOR));
    }
  }
  for (auto kind : {TEMPERATURE, ECHO, UNKNOWN_REGISTER}) {
    CHECK(!rangeLevelTiming(DeviceModel::SEN0591, kind, 1, out));
    CHECK(sameTiming(out, PRIOR));
  }
  for (auto level : {0, 5, 65535}) {
    CHECK(!rangeLevelTiming(DeviceModel::SEN0591, PROCESSED, level, out));
    CHECK(sameTiming(out, PRIOR));
  }
  CHECK(!rangeLevelTiming(UNKNOWN_MODEL, REAL_TIME, 1, out));
  CHECK(sameTiming(out, PRIOR));
}

}  // namespace

int main() {
  checkGeneralTiming();
  checkLevelTiming();
}
