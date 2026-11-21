/** @file Types.h
 *  @brief Sensor selectors and caller-owned Modbus request identity.
 */
#pragma once

#include <cstdint>

#include "Status.h"

namespace SEN059x {

/// Model selected from the part marking or installation record.
enum class DeviceModel : std::uint8_t {
  SEN0591 = 1,  ///< 3 m model.
  SEN0592,      ///< 5 m model.
};

/// Measurement selectors. The enum values are the published register addresses.
enum class MeasurementKind : std::uint16_t {
  ProcessedDistance = 0x0100,  ///< Processed distance in millimetres.
  RealTimeDistance = 0x0101,   ///< Real-time distance in millimetres.
  Temperature = 0x0102,        ///< Temperature word in tenths of a degree Celsius.
  EchoTime = 0x010A,           ///< Echo time in microseconds.
};

/// Setting selectors. The enum values are the published register addresses.
enum class Setting : std::uint16_t {
  Address = 0x0200,          ///< Modbus destination address.
  Baud = 0x0201,             ///< Baud rate code.
  SwitchPolarity = 0x0205,   ///< Switch output polarity code.
  SwitchThreshold = 0x0206,  ///< Switch distance threshold in millimetres.
  Angle = 0x0208,            ///< Detection angle level.
  UartUnit = 0x0209,         ///< UART distance output unit code.
  Noise = 0x021A,            ///< Power noise reduction level.
  Range = 0x021F,            ///< Maximum range level.
};

/// Destination policy for an addressed exchange.
enum class AddressPolicy : std::uint8_t {
  StandardUnicast,  ///< Standard Modbus unicast addresses 1–247.
  DFRobotIsolated,  ///< DFRobot addresses 1–254 on an isolated link.
};

/// Function for an addressed request. Values are the Modbus function bytes.
enum class FunctionCode : std::uint8_t {
  ReadHoldingRegisters = 0x03,  ///< FC03, one or more holding registers.
  WriteSingleRegister = 0x06,   ///< FC06, one holding register.
};

/**
 * @brief Identity retained by the caller until the addressed exchange is handled.
 *
 * Holds values by copy and borrows no buffer. A value-initialized context
 * has no selected model and cannot describe an active exchange.
 */
struct RequestContext {
  DeviceModel model{};                                           ///< Explicit sensor model.
  std::uint8_t address = 0;                                      ///< Destination address byte.
  FunctionCode function{};                                       ///< Request function.
  std::uint16_t startRegister = 0;                               ///< First register address.
  std::uint16_t countOrWriteWord = 0;                            ///< FC03 word count or FC06 word.
  AddressPolicy addressPolicy = AddressPolicy::StandardUnicast;  ///< Address rules for the reply.
};

/**
 * @brief Check an addressed destination under the given policy.
 * @param address Destination byte.
 * @param addressPolicy Address policy. Use DFRobotIsolated only on an isolated link.
 * @return True for addresses admitted by the policy. Zero and 255 are reserved.
 */
bool isValidAddress(std::uint8_t address,
                    AddressPolicy addressPolicy = AddressPolicy::StandardUnicast);

/**
 * @brief Create the context for an addressed FC03 or FC06 exchange.
 * @param model Explicitly selected model.
 * @param address Destination byte under addressPolicy.
 * @param function FC03 or FC06.
 * @param startRegister First 16-bit register address.
 * @param countOrWriteWord FC03 count in 16-bit registers (1–125), or raw FC06 write word.
 * @param out Caller storage, assigned only on success. No reference to it is retained.
 * @param addressPolicy Address policy. Use DFRobotIsolated only on an isolated link.
 * @return OK for valid request identity, or INVALID_CONFIG for an invalid model,
 * destination, policy, function, or FC03 count or span beyond the address space.
 * Setting access, value bounds and documented register coverage belong to the request builder.
 */
Status makeRequestContext(DeviceModel model, std::uint8_t address, FunctionCode function,
                          std::uint16_t startRegister, std::uint16_t countOrWriteWord,
                          RequestContext& out,
                          AddressPolicy addressPolicy = AddressPolicy::StandardUnicast);

}  // namespace SEN059x
