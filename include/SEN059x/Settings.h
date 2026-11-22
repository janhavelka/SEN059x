/** @file Settings.h
 *  @brief Configuration register access for SEN0591 and SEN0592 (DFRobot table 2).
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Types.h"

namespace SEN059x {

/// Meaning of the 16-bit argument to a table 2 write.
enum class SettingUnit : std::uint8_t {
  Address,
  BaudCode,
  PolarityCode,
  Millimetres,
  AngleLevel,
  UartUnitCode,
  NoiseLevel,
  RangeLevel
};

/// Where the setting has its documented effect. All eight registers support FC03 and FC06.
enum class SettingMode : std::uint8_t { Modbus, SwitchOutput, AutomaticOrControlledUart, Sensor };

/// Published bounds and factory information for one model. Defaults are independent of live state.
struct SettingDescriptor {
  SettingUnit unit{};                        ///< Unit or code required by the write builder.
  SettingMode mode{};                        ///< Scope of the documented effect.
  std::uint16_t minWord = 0;                 ///< Inclusive device minimum in unit.
  std::uint16_t maxWord = 0;                 ///< Inclusive device maximum in unit.
  std::optional<std::uint16_t> factoryWord;  ///< Factory word if specified in table 2.
};

/**
 * @brief Look up a configuration register on the explicitly selected model.
 * @param model SEN0591 or SEN0592.
 * @param setting One of the eight configuration selectors.
 * @param out Descriptor assigned only on success. Address bounds describe the device.
 * @return True for a documented model and setting.
 */
bool settingDescriptor(DeviceModel model, Setting setting, SettingDescriptor& out);

/// Register 0x0201 codes from the SEN0591 and SEN0592 Chinese manuals, table 2.
enum class Baud : std::uint16_t {
  Bps4800 = 2,
  Bps9600,
  Bps14400,
  Bps19200,
  Bps38400,
  Bps57600,
  Bps76800,
  Bps115200
};

/// Convert code to bits per second. On an unknown code, return false and preserve bps.
bool baudToValue(Baud code, std::uint32_t& bps);
/// Convert bits per second to the exact register code. On failure return false and preserve code.
bool valueToBaud(std::uint32_t bps, Baud& code);
/// Return the statically stored decimal rate in bits per second, or nullptr for an unknown code.
const char* baudToString(Baud code);

/**
 * @brief Describe a published code or level for display.
 * @param model SEN0591 or SEN0592.
 * @param setting Configuration selector. Polarity applies strictly below the switch threshold.
 * Equality and physical mode activation on the four-wire models are unspecified.
 * UART units apply to automatic and controlled output. Modbus measurement units are fixed.
 * @param word Raw register code or level.
 * @return Static label, or nullptr for an unknown model, selector, word, or a numeric
 * address or threshold. Numeric thresholds use millimetres.
 */
const char* settingWordDescription(DeviceModel model, Setting setting, std::uint16_t word);

/// Identity of one typed FC03 setting request. A default instance has no active request.
class SettingRequest {
 public:
  /// Borrow the addressed wire identity for the lifetime of this request.
  const RequestContext& context() const { return context_; }

 private:
  RequestContext context_{};
  friend std::size_t buildSettingRead(DeviceModel, std::uint8_t, Setting, std::uint8_t*,
                                      std::size_t, SettingRequest&, AddressPolicy);
};

/// One attributable FC03 observation. Interpret rawWord using this model and selector.
struct SettingReadback {
  DeviceModel model{};        ///< Explicit model retained from the request.
  std::uint8_t address = 0;   ///< Address of the responding endpoint.
  Setting setting{};          ///< Register selected by the owner.
  std::uint16_t rawWord = 0;  ///< Exact word including values outside the published bounds.
  bool known = false;         ///< True when table 2 documents this word for the model.
};

/**
 * @brief Build a one-word addressed FC03 request.
 * @param model Explicit sensor model.
 * @param address Current device address admitted by addressPolicy.
 * @param setting Configuration register to read.
 * @param tx Writable storage disjoint from out.
 * @param txCapacity Capacity of tx in bytes, at least eight.
 * @param out Caller-owned identity to retain until the owner handles the reply.
 * @param addressPolicy Use DFRobotIsolated only on an isolated link.
 * @return Eight on success, or zero on rejection with tx and out preserved.
 */
std::size_t buildSettingRead(DeviceModel model, std::uint8_t address, Setting setting,
                             std::uint8_t* tx, std::size_t txCapacity, SettingRequest& out,
                             AddressPolicy addressPolicy = AddressPolicy::StandardUnicast);

/**
 * @brief Validate a complete FC03 reply and publish the observed setting word.
 * @param rx Buffer readable for rxLength bytes, disjoint from outputs.
 * @param rxLength Number of received bytes.
 * @param expected Identity from buildSettingRead for the outstanding exchange.
 * @param out Receives the observation by value on success. Rejections preserve it.
 * @param error FC03 diagnostic assigned on every call.
 * @return OK for an attributable word (including unknown words). Invalid arguments, frame
 * errors and addressed exceptions propagate the FC03 validator's status and detail.
 */
Status decodeSettingResponse(const std::uint8_t* rx, std::size_t rxLength,
                             const SettingRequest& expected, SettingReadback& out,
                             ReadResponseError& error);

/// Requested table 2 word tagged with its unit. Convert baud/s with valueToBaud first.
struct SettingValue {
  SettingUnit unit;    ///< Must match the selector's descriptor.
  std::uint16_t word;  ///< Register word in the specified unit.
};

/**
 * @brief Encode an addressed FC06 after checking the setting and value.
 * @param model Explicit sensor model used for threshold and level bounds.
 * @param address Current device address.
 * @param setting Writable configuration selector.
 * @param value Register word and unit for setting.
 * @param tx Writable storage disjoint from out.
 * @param txCapacity Capacity of tx in bytes, at least eight.
 * @param out Caller-owned identity retaining the original address, register and word.
 * @param addressPolicy Applies to both current and assigned addresses. Select
 * DFRobotIsolated explicitly on an isolated link to permit 248 through 254.
 * @return Eight on success, or zero on rejection with tx and out preserved.
 */
std::size_t buildWriteSingleRegister(DeviceModel model, std::uint8_t address, Setting setting,
                                     SettingValue value, std::uint8_t* tx, std::size_t txCapacity,
                                     RequestContext& out,
                                     AddressPolicy addressPolicy = AddressPolicy::StandardUnicast);

/**
 * @brief Encode the vendor FC06 to the fixed destination 0xFF on an isolated link.
 * @param model Explicit sensor model used for value bounds.
 * @param setting Writable configuration selector.
 * @param value Register word and unit for setting.
 * @param tx Writable request storage.
 * @param txCapacity Capacity of tx in bytes, at least eight.
 * @param addressPolicy Must explicitly be DFRobotIsolated.
 * @return Eight encoded bytes on success, or zero with tx preserved. This send-only
 * frame has no addressed reply identity. Establish later state by an addressed read.
 */
std::size_t buildVendorBroadcastWrite(DeviceModel model, Setting setting, SettingValue value,
                                      std::uint8_t* tx, std::size_t txCapacity,
                                      AddressPolicy addressPolicy);

}  // namespace SEN059x
