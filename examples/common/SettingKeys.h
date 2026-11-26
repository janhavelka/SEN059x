#pragma once

#include <array>
#include <string_view>

#include <SEN059x/Settings.h>

namespace SEN059x::example {

struct SettingKey {
  Setting setting;
  std::string_view key;
};

inline constexpr std::array<SettingKey, 8> SETTING_KEYS{{
    {Setting::Address, "address"},
    {Setting::Baud, "baud"},
    {Setting::SwitchPolarity, "polarity"},
    {Setting::SwitchThreshold, "threshold"},
    {Setting::Angle, "angle"},
    {Setting::UartUnit, "uart-unit"},
    {Setting::Noise, "noise"},
    {Setting::Range, "range"},
}};

}  // namespace SEN059x::example
