#include <array>

#include "SEN059x/Settings.h"

namespace SEN059x {
namespace {

struct BaudRate {
  std::uint32_t bps;
  const char* text;
};

// DFRobot SEN0591 and SEN0592, CN table 2, register 0x0201.
// Both EN lists stop at the erroneous entry for code 5.
constexpr std::array<BaudRate, 8> BAUD_RATES = {{
    {4800, "4800"},
    {9600, "9600"},
    {14400, "14400"},
    {19200, "19200"},
    {38400, "38400"},
    {57600, "57600"},
    {76800, "76800"},
    {115200, "115200"},
}};
constexpr auto FIRST_BAUD_CODE = static_cast<std::uint16_t>(Baud::Bps4800);

const BaudRate* lookupBaud(Baud baud) {
  const auto code = static_cast<std::uint16_t>(baud);
  if (code < FIRST_BAUD_CODE || code >= FIRST_BAUD_CODE + BAUD_RATES.size()) {
    return nullptr;
  }
  return &BAUD_RATES[code - FIRST_BAUD_CODE];
}

}  // namespace

bool baudToValue(Baud baud, std::uint32_t& bps) {
  const auto* rate = lookupBaud(baud);
  if (rate == nullptr) {
    return false;
  }
  bps = rate->bps;
  return true;
}

bool valueToBaud(std::uint32_t bps, Baud& baud) {
  for (std::size_t i = 0; i < BAUD_RATES.size(); ++i) {
    if (BAUD_RATES[i].bps == bps) {
      baud = static_cast<Baud>(FIRST_BAUD_CODE + i);
      return true;
    }
  }
  return false;
}

const char* baudToString(Baud baud) {
  const auto* rate = lookupBaud(baud);
  return rate == nullptr ? nullptr : rate->text;
}

bool settingDescriptor(DeviceModel model, Setting setting, SettingDescriptor& out) {
  if (model != DeviceModel::SEN0591 && model != DeviceModel::SEN0592) {
    return false;
  }

  // DFRobot register table 2, both SKUs. Threshold and UART unit have no published defaults.
  switch (setting) {
    case Setting::Address:
      out = {SettingUnit::Address, SettingMode::Modbus, 1, 254, 1};
      return true;
    case Setting::Baud:
      out = {SettingUnit::BaudCode, SettingMode::Modbus, 2, 9, 9};
      return true;
    case Setting::SwitchPolarity:
      out = {SettingUnit::PolarityCode, SettingMode::SwitchOutput, 0, 1, 1};
      return true;
    case Setting::SwitchThreshold: {
      // The SEN0591 CN entry for 0x0206 resolves the repeated lower bound in EN.
      const std::uint16_t maxMm = model == DeviceModel::SEN0591 ? 3000 : 5000;
      out = {SettingUnit::Millimetres, SettingMode::SwitchOutput, 30, maxMm, std::nullopt};
      return true;
    }
    case Setting::Angle:
      out = {SettingUnit::AngleLevel, SettingMode::Sensor, 1, 4, 4};
      return true;
    case Setting::UartUnit:
      out = {SettingUnit::UartUnitCode, SettingMode::AutomaticOrControlledUart, 0, 1, std::nullopt};
      return true;
    case Setting::Noise:
      out = {SettingUnit::NoiseLevel, SettingMode::Sensor, 1, 5, 1};
      return true;
    case Setting::Range: {
      const std::uint16_t maxLevel = model == DeviceModel::SEN0591 ? 4 : 5;
      out = {SettingUnit::RangeLevel, SettingMode::Sensor, 1, maxLevel, maxLevel};
      return true;
    }
  }
  return false;
}

const char* settingWordDescription(DeviceModel model, Setting setting, std::uint16_t word) {
  SettingDescriptor descriptor;
  if (!settingDescriptor(model, setting, descriptor) || word < descriptor.minWord ||
      word > descriptor.maxWord) {
    return nullptr;
  }
  switch (setting) {
    case Setting::Address:
    case Setting::SwitchThreshold:
      return nullptr;
    case Setting::Baud:
      return baudToString(static_cast<Baud>(word));
    case Setting::SwitchPolarity:
      return word == 0 ? "low below threshold" : "high below threshold";
    case Setting::UartUnit:
      return word == 0 ? "mm" : "us";
    case Setting::Angle: {
      // Table 2, 0x0208. The SEN0592 CN edition identifies both dual axes at every level.
      static constexpr const char* SEN0591_ANGLES[] = {"about 30 deg", "about 40 deg",
                                                       "about 50 deg", "about 60 deg"};
      static constexpr const char* SEN0592_ANGLES[] = {
          "about 40 deg single, 45 deg dual horizontal, 60 deg dual vertical",
          "about 45 deg single, 50 deg dual horizontal, 65 deg dual vertical",
          "about 55 deg single, 55 deg dual horizontal, 70 deg dual vertical",
          "about 65 deg single, 65 deg dual horizontal, 80 deg dual vertical",
      };
      return model == DeviceModel::SEN0591 ? SEN0591_ANGLES[word - 1] : SEN0592_ANGLES[word - 1];
    }
    case Setting::Noise: {
      static constexpr const char* NOISE_SUPPLIES[] = {
          "battery supply",
          "high-frequency supply noise (USB)",
          "long USB power cable",
          "switching supply",
          "switching supply with environmental interference (routine use discouraged)",
      };
      return NOISE_SUPPLIES[word - 1];
    }
    case Setting::Range: {
      static constexpr const char* RANGES[] = {"about 50 cm", "about 150 cm", "about 250 cm",
                                               "about 350 cm", "about 500 cm"};
      return RANGES[word - 1];
    }
  }
  return nullptr;
}

}  // namespace SEN059x
