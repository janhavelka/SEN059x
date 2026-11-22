#include "SEN059x/Measurements.h"

#include "SEN059x/ReadCodec.h"

namespace SEN059x {

std::size_t buildMeasurementRead(DeviceModel model, std::uint8_t address, MeasurementKind kind,
                                 std::uint8_t* tx, std::size_t txCapacity, MeasurementRequest& out,
                                 AddressPolicy addressPolicy) {
  switch (kind) {
    case MeasurementKind::ProcessedDistance:
    case MeasurementKind::RealTimeDistance:
    case MeasurementKind::Temperature:
    case MeasurementKind::EchoTime:
      return buildReadRegisters(model, address, static_cast<std::uint16_t>(kind), 1, tx, txCapacity,
                                out.context_, addressPolicy);
    default:
      return 0;
  }
}

Status decodeMeasurementResponse(const std::uint8_t* rx, std::size_t rxLength,
                                 const MeasurementRequest& expected, std::uint32_t completedMs,
                                 bool timeValid, Measurement& out, ReadResponseError& error) {
  const auto& request = expected.context();
  std::uint16_t word = 0;
  const auto status = validateReadResponseExpected(rx, rxLength, request, &word, 1, error);
  if (!status) {
    return status;
  }

  Measurement result;
  result.model = request.model;
  result.address = request.address;
  result.kind = static_cast<MeasurementKind>(request.startRegister);
  result.rawWord = word;
  result.completedMs = completedMs;
  result.timeValid = timeValid;
  result.quality = MeasurementQuality::Usable;
  switch (result.kind) {
    case MeasurementKind::ProcessedDistance:
    case MeasurementKind::RealTimeDistance: {
      // DFRobot product ratings in mm for 0x0100 and 0x0101 on the selected SKU.
      const bool inProfile = request.model == DeviceModel::SEN0591 ? word >= 20 && word <= 3000
                                                                   : word >= 30 && word <= 5000;
      result.unit = MeasurementUnit::Millimetres;
      if (inProfile) {
        result.value = word;
      } else {
        result.quality = MeasurementQuality::OutOfProfile;
      }
      break;
    }
    case MeasurementKind::Temperature:
      result.unit = MeasurementUnit::DegreesCelsius;
      // 0x0102, both SKUs: table 1 gives unsigned tenths and a five-tenth step.
      // Use the +65 C operating rating as the upper bound for interpretation.
      if (word > 650) {
        result.quality = MeasurementQuality::AmbiguousTemperature;
      } else if (word % 5 != 0) {
        result.quality = MeasurementQuality::UnexpectedQuantization;
      } else {
        result.value = word / 10.0;
      }
      break;
    case MeasurementKind::EchoTime:
      result.unit = MeasurementUnit::Microseconds;
      result.value = word;
      break;
  }
  out = result;
  return status;
}

double approximateEchoDistanceMm(std::uint16_t echoTimeUs) {
  // SEN0591 and SEN0592 table 1, echo time (0x010A).
  return echoTimeUs / 5.75;
}

}  // namespace SEN059x
