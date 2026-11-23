/** @file ExchangeTiming.h
 *  @brief Application-side deadline and RTU silence intervals for one addressed exchange.
 */
#pragma once

#include <cstdint>
#include <optional>

#include "SEN059x/Measurements.h"
#include "SEN059x/Settings.h"

namespace SEN059x::example {

/// Durations in microseconds for the selected 8N1 endpoint.
struct ExchangeTiming {
  std::uint32_t txUs = 0;              ///< Eight-byte request serialization, rounded up.
  std::uint32_t rxUs = 0;              ///< Full normal reply serialization, rounded up.
  std::uint32_t interCharacterUs = 0;  ///< Longest whole-us silence allowed within a frame.
  std::uint32_t closureUs = 0;         ///< Minimum whole-us silence sufficient to close a frame.
  std::uint64_t budgetUs = 0;          ///< From start of TX through reception and closure.
};

/// Calculate whole-microsecond silence limits per Modbus Serial Line v1.02 section 2.5.1.1.
/// At a supported sensor baud, set the maximum in-frame gap and the minimum inter-frame gap.
/// An unsupported baud returns false and preserves both outputs.
inline bool rtuTiming(std::uint32_t baud, std::uint32_t& interCharacterUs,
                      std::uint32_t& closureUs) {
  Baud baudCode{};
  if (!valueToBaud(baud, baudCode)) {
    return false;
  }
  // At or below 19200 bps use 1.5 and 3.5 character times. Each 8N1 character is ten bits.
  interCharacterUs = baud <= 19200 ? 15000000u / baud : 750u;
  closureUs = baud <= 19200 ? (35000000u + baud - 1) / baud : 1750u;
  return true;
}

/**
 * @brief Budget one exchange on an idle bus.
 * @param request Retained addressed FC03/FC06 context from the corresponding request builder.
 * @param baud Documented sensor rate in bits per second, using 8N1 (ten bits per byte).
 * @param turnaroundUs Owner's allowance for direction turnaround in microseconds.
 * @param marginUs Owner's additional scheduling and device margin in microseconds.
 * @param deviceBudgetUs Owner estimate for device processing alone in microseconds. Required
 * for settings and block reads. A one-word measurement always reserves at least its SKU's
 * general table 1 maximum, even if the caller supplies a smaller estimate.
 * @param out Receives durations on success. A five-byte exception fits the normal reply budget.
 * @return False for invalid wire identity, unsupported baud or missing device estimate.
 * The caller supplies the monotonic start time and checks addition when forming a deadline.
 */
inline bool exchangeTiming(const RequestContext& request, std::uint32_t baud,
                           std::uint32_t turnaroundUs, std::uint32_t marginUs,
                           std::optional<std::uint32_t> deviceBudgetUs, ExchangeTiming& out) {
  ExchangeTiming timing{};
  RequestContext validated{};
  if (!rtuTiming(baud, timing.interCharacterUs, timing.closureUs) ||
      !makeRequestContext(request.model, request.address, request.function, request.startRegister,
                          request.countOrWriteWord, validated, request.addressPolicy)) {
    return false;
  }

  const bool read = request.function == FunctionCode::ReadHoldingRegisters;
  MeasurementTiming measurement{};
  if (read && request.countOrWriteWord == 1 &&
      measurementTiming(request.model, static_cast<MeasurementKind>(request.startRegister),
                        measurement)) {
    const std::uint32_t minimumUs = 1000u * measurement.maxMs;
    if (deviceBudgetUs.value_or(0) < minimumUs) {
      deviceBudgetUs = minimumUs;
    }
  }
  if (!deviceBudgetUs) {
    return false;
  }

  // The wire validator bounds an FC03 normal reply to 255 bytes.
  const auto wireUs = [baud](std::uint32_t bytes) { return (bytes * 10000000u + baud - 1) / baud; };
  timing.txUs = wireUs(8);
  timing.rxUs = wireUs(read ? 5u + 2u * request.countOrWriteWord : 8u);
  timing.budgetUs = std::uint64_t{timing.txUs} + turnaroundUs + *deviceBudgetUs + timing.rxUs +
                    timing.closureUs + marginUs;
  out = timing;
  return true;
}

}  // namespace SEN059x::example
