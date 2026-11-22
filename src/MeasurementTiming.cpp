#include "SEN059x/Measurements.h"

namespace SEN059x {

bool measurementTiming(DeviceModel model, MeasurementKind kind, MeasurementTiming& out) {
  if (model != DeviceModel::SEN0591 && model != DeviceModel::SEN0592) {
    return false;
  }

  // DFRobot table 1. The SEN0592 Chinese edition gives three different minima.
  switch (kind) {
    case MeasurementKind::ProcessedDistance:
      out = model == DeviceModel::SEN0591 ? MeasurementTiming{190, 190, 750}
                                          : MeasurementTiming{190, 300, 750};
      return true;
    case MeasurementKind::RealTimeDistance:
      out = {15, 15, 140};
      return true;
    case MeasurementKind::Temperature:
    case MeasurementKind::EchoTime:
      out = model == DeviceModel::SEN0591 ? MeasurementTiming{5, 5, 140}
                                          : MeasurementTiming{5, 40, 140};
      return true;
  }
  return false;
}

bool rangeLevelTiming(DeviceModel model, MeasurementKind kind, std::uint16_t rangeLevel,
                      MeasurementTiming& out) {
  // DFRobot SEN0591 table 2, register 0x021F. Both editions use these intervals.
  struct Interval {
    std::uint16_t minMs;
    std::uint16_t maxMs;
  };
  static constexpr Interval PROCESSED[] = {{190, 500}, {230, 550}, {250, 600}, {280, 650}};
  static constexpr Interval REAL_TIME[] = {{15, 80}, {20, 90}, {25, 100}, {35, 110}};

  if (model != DeviceModel::SEN0591 || rangeLevel < 1 || rangeLevel > 4) {
    return false;
  }
  if (kind != MeasurementKind::ProcessedDistance && kind != MeasurementKind::RealTimeDistance) {
    return false;
  }
  const auto& interval = kind == MeasurementKind::ProcessedDistance ? PROCESSED[rangeLevel - 1]
                                                                    : REAL_TIME[rangeLevel - 1];
  out = {interval.minMs, interval.minMs, interval.maxMs};
  return true;
}

}  // namespace SEN059x
