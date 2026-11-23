#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

#include "../../examples/common/ExchangeTiming.h"

namespace {

using namespace SEN059x;
using namespace SEN059x::example;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

void checkSilenceLimits() {
  // Modbus Serial Line v1.02 section 2.5.1.1 applied to the eight published sensor rates.
  constexpr struct {
    std::uint32_t baud;
    std::uint32_t longestInFrameUs;
    std::uint32_t shortestClosureUs;
  } cases[] = {
      {4800, 3125, 7292}, {9600, 1562, 3646}, {14400, 1041, 2431}, {19200, 781, 1823},
      {38400, 750, 1750}, {57600, 750, 1750}, {76800, 750, 1750},  {115200, 750, 1750},
  };
  for (const auto& item : cases) {
    std::uint32_t inter = 0;
    std::uint32_t closure = 0;
    CHECK(rtuTiming(item.baud, inter, closure));
    CHECK(inter == item.longestInFrameUs && closure == item.shortestClosureUs);
    CHECK(!rtuTiming(0, inter, closure));
    CHECK(!rtuTiming(19201, inter, closure));
    CHECK(inter == item.longestInFrameUs && closure == item.shortestClosureUs);
  }
}

void checkMeasurementBudgets(DeviceModel model) {
  for (auto kind : {MeasurementKind::ProcessedDistance, MeasurementKind::RealTimeDistance,
                    MeasurementKind::Temperature, MeasurementKind::EchoTime}) {
    std::uint8_t tx[8]{};
    MeasurementRequest request{};
    CHECK(buildMeasurementRead(model, 1, kind, tx, sizeof(tx), request) == 8);
    ExchangeTiming timing{};
    CHECK(exchangeTiming(request.context(), 115200, 100, 1000, std::nullopt, timing));
    CHECK(timing.txUs == 695 && timing.rxUs == 608 && timing.closureUs == 1750);
    const bool processed = kind == MeasurementKind::ProcessedDistance;
    CHECK(timing.budgetUs == (processed ? 754153u : 144153u));
    // A short caller estimate still allows the register's full response time.
    CHECK(exchangeTiming(request.context(), 9600, 100, 1000, 15000, timing));
    CHECK(timing.txUs == 8334 && timing.rxUs == 7292 && timing.closureUs == 3646);
    CHECK(timing.budgetUs == (processed ? 770372u : 160372u));
    CHECK(exchangeTiming(request.context(), 115200, 100, 1000, 1000000, timing));
    CHECK(timing.budgetUs == 1004153);
  }
}

void checkOwnerEstimates() {
  // The assumed 1300 ms execution allowance is an operator choice for the block fixture.
  RequestContext request{DeviceModel::SEN0592, 1, FunctionCode::ReadHoldingRegisters, 0x0100, 3};
  ExchangeTiming timing{};
  CHECK(!exchangeTiming(request, 115200, 100, 1000, std::nullopt, timing));
  CHECK(exchangeTiming(request, 115200, 100, 1000, 1300000, timing));
  CHECK(timing.rxUs == 955 && timing.budgetUs == 1304500);

  request.startRegister = 0x0200;
  request.countOrWriteWord = 1;
  CHECK(!exchangeTiming(request, 115200, 100, 1000, std::nullopt, timing));
  CHECK(exchangeTiming(request, 115200, 100, 1000, 50000, timing));
  CHECK(timing.rxUs == 608 && timing.budgetUs == 54153);

  request.function = FunctionCode::WriteSingleRegister;
  request.countOrWriteWord = 5;
  CHECK(!exchangeTiming(request, 115200, 100, 1000, std::nullopt, timing));
  CHECK(exchangeTiming(request, 115200, 100, 1000, 50000, timing));
  CHECK(timing.rxUs == 695 && timing.budgetUs == 54240);
  const auto maxUs = std::numeric_limits<std::uint32_t>::max();
  CHECK(exchangeTiming(request, 115200, maxUs, maxUs, maxUs, timing));
  const auto largeBudget = 3ull * maxUs + 3140;
  CHECK(timing.budgetUs == largeBudget);

  CHECK(!exchangeTiming(request, 0, 0, 0, 1, timing));
  request.model = static_cast<DeviceModel>(0);
  CHECK(!exchangeTiming(request, 115200, 0, 0, 1, timing));
  request = {DeviceModel::SEN0591, 1, FunctionCode::ReadHoldingRegisters, 0x0100, 0};
  CHECK(!exchangeTiming(request, 115200, 0, 0, 1, timing));
  CHECK(timing.budgetUs == largeBudget);
}

}  // namespace

int main() {
  checkSilenceLimits();
  checkMeasurementBudgets(DeviceModel::SEN0591);
  checkMeasurementBudgets(DeviceModel::SEN0592);
  checkOwnerEstimates();
}
