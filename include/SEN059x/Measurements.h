/** @file Measurements.h
 *  @brief Single-register measurement requests, observations and response-time estimates.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Types.h"

namespace SEN059x {

/// Identity of one typed FC03 request. A default instance has no active request.
class MeasurementRequest {
 public:
  /// Borrow the retained wire identity for the lifetime of this request.
  const RequestContext& context() const { return context_; }

 private:
  RequestContext context_{};
  friend std::size_t buildMeasurementRead(DeviceModel, std::uint8_t, MeasurementKind, std::uint8_t*,
                                          std::size_t, MeasurementRequest&, AddressPolicy);
};

/// Unit of the interpreted value for the selected FC03 register.
enum class MeasurementUnit : std::uint8_t { Millimetres, DegreesCelsius, Microseconds };

/// Interpretation of a validated register word. Missing-echo codes are unknown for both models.
enum class MeasurementQuality : std::uint8_t {
  Unobserved,              ///< Initial state before a successful decode.
  Usable,                  ///< An engineering value is available.
  OutOfProfile,            ///< Distance word outside the selected model's inclusive rating.
  UnexpectedQuantization,  ///< Temperature word off the documented half-degree step.
  AmbiguousTemperature,    ///< High raw temperature word with unverified interpretation.
};

/// Resolution of register 0x0102 in degrees Celsius, per DFRobot table 1 for both models.
constexpr double TEMPERATURE_RESOLUTION_C = 0.5;

/// One caller-owned observation from the identified register, copied independently of RX.
struct Measurement {
  DeviceModel model{};            ///< Model selected by the caller.
  std::uint8_t address = 0;       ///< Addressed destination.
  MeasurementKind kind{};         ///< Selector whose underlying value is the register address.
  std::uint16_t rawWord = 0;      ///< Exact FC03 word in mm, tenths of a degree C, or microseconds.
  MeasurementUnit unit{};         ///< Engineering unit for this kind, independent of UART settings.
  MeasurementQuality quality{};   ///< Data quality, separate from frame validation status.
  std::optional<double> value;    ///< Engineering value in unit, present only for usable words.
  std::uint32_t completedMs = 0;  ///< Owner's monotonic completion time in ms if timeValid.
  bool timeValid = false;         ///< A known completion time may be zero.
};

/**
 * @brief Build a single-word FC03 request and retain its measurement identity.
 *
 * Each transmitted distance or echo request starts its own ranging cycle.
 * The temperature sampling instant is unspecified.
 * @param model Explicitly selected SEN0591 or SEN0592.
 * @param address Destination under addressPolicy.
 * @param kind One of the four published measurement registers.
 * @param tx Writable buffer disjoint from out, with room for eight bytes.
 * @param txCapacity Capacity of tx in bytes.
 * @param out Caller-owned identity to retain for the outstanding exchange.
 * @param addressPolicy Use DFRobotIsolated only on an isolated link.
 * @return Eight on success, or zero on rejection with tx and out preserved.
 */
std::size_t buildMeasurementRead(DeviceModel model, std::uint8_t address, MeasurementKind kind,
                                 std::uint8_t* tx, std::size_t txCapacity, MeasurementRequest& out,
                                 AddressPolicy addressPolicy = AddressPolicy::StandardUnicast);

/**
 * @brief Validate and interpret the complete reply for one outstanding typed request.
 *
 * The owner associates the frame with this exchange. FC03 omits the model and register.
 * Distances use the SKU rating. For temperature, the interpretation policy accepts
 * whole half-degree steps from zero through the common +65 C operating rating.
 * Higher words remain raw pending evidence for their encoding. Echo time retains its
 * microsecond unit. Its approximate distance conversion is available separately.
 * @param rx Buffer readable for rxLength bytes, disjoint from the outputs.
 * @param rxLength Actual received frame length in bytes.
 * @param expected Copy of the identity retained by buildMeasurementRead.
 * @param completedMs Owner-supplied monotonic completion time in ms.
 * @param timeValid Whether completedMs is known, including at zero.
 * @param out Observation assigned only after successful frame validation.
 * @param error FC03 diagnostic assigned on every call, including NONE on success.
 * @return OK for an attributable raw word, including one with unavailable interpretation.
 * Invalid input, frame errors and addressed exceptions propagate the FC03 validator's
 * status and detail. A validated exception retains its full code.
 */
Status decodeMeasurementResponse(const std::uint8_t* rx, std::size_t rxLength,
                                 const MeasurementRequest& expected, std::uint32_t completedMs,
                                 bool timeValid, Measurement& out, ReadResponseError& error);

/**
 * @brief Apply DFRobot's approximate distance conversion to an echo time.
 * @param echoTimeUs Register 0x010A word in microseconds.
 * @return Approximate distance in millimetres (microseconds divided by 5.75).
 */
double approximateEchoDistanceMm(std::uint16_t echoTimeUs);

/// Approximate response intervals from DFRobot's English and Chinese editions.
/// The bus owner budgets frame transmission, turnaround and deadline margin separately.
struct MeasurementTiming {
  std::uint16_t englishMinMs = 0;  ///< Lower estimate in the English edition, ms.
  std::uint16_t chineseMinMs = 0;  ///< Lower estimate in the Chinese edition, ms.
  std::uint16_t maxMs = 0;         ///< Upper estimate shared by both editions, ms.
};

/**
 * @brief Look up the general response interval in measurement table 1.
 * @param model Explicitly selected SKU.
 * @param kind Measurement register selector.
 * @param out Receives approximate times in ms on success.
 * @return True on success, or false for an unknown model or kind with out preserved.
 */
bool measurementTiming(DeviceModel model, MeasurementKind kind, MeasurementTiming& out);

/**
 * @brief Look up the additional SEN0591 distance estimate from table 2 (0x021F).
 * @param model Explicitly selected SKU. Only SEN0591 publishes per-level intervals.
 * @param kind Processed or real-time distance selector.
 * @param rangeLevel SEN0591 range level from 1 through 4.
 * @param out Receives the approximate interval in ms on success.
 * @return True for a published interval, or false with out preserved.
 */
bool rangeLevelTiming(DeviceModel model, MeasurementKind kind, std::uint16_t rangeLevel,
                      MeasurementTiming& out);

}  // namespace SEN059x
