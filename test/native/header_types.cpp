#include <SEN059x/Types.h>

using SEN059x::AddressPolicy;
using SEN059x::FunctionCode;
using SEN059x::MeasurementKind;
using SEN059x::RequestContext;
using SEN059x::Setting;

// Register values from OPERATIONS.csv and the two SKU register tables.
static_assert(static_cast<std::uint16_t>(MeasurementKind::ProcessedDistance) == 0x0100);
static_assert(static_cast<std::uint16_t>(MeasurementKind::RealTimeDistance) == 0x0101);
static_assert(static_cast<std::uint16_t>(MeasurementKind::Temperature) == 0x0102);
static_assert(static_cast<std::uint16_t>(MeasurementKind::EchoTime) == 0x010A);
static_assert(static_cast<std::uint16_t>(Setting::Address) == 0x0200);
static_assert(static_cast<std::uint16_t>(Setting::Baud) == 0x0201);
static_assert(static_cast<std::uint16_t>(Setting::SwitchPolarity) == 0x0205);
static_assert(static_cast<std::uint16_t>(Setting::SwitchThreshold) == 0x0206);
static_assert(static_cast<std::uint16_t>(Setting::Angle) == 0x0208);
static_assert(static_cast<std::uint16_t>(Setting::UartUnit) == 0x0209);
static_assert(static_cast<std::uint16_t>(Setting::Noise) == 0x021A);
static_assert(static_cast<std::uint16_t>(Setting::Range) == 0x021F);

static_assert(static_cast<std::uint8_t>(FunctionCode::ReadHoldingRegisters) == 0x03);
static_assert(static_cast<std::uint8_t>(FunctionCode::WriteSingleRegister) == 0x06);
static_assert(RequestContext{}.addressPolicy == AddressPolicy::StandardUnicast);
