/** @file Status.h
 *  @brief Portable operation results and Modbus diagnostics.
 */
#pragma once

#include <cstdint>

namespace SEN059x {

/// Broad operation result. Only the numeric value of OK is part of the public contract.
enum class Err : std::uint16_t {
  OK = 0,           ///< The requested operation succeeded.
  INVALID_CONFIG,   ///< Invalid argument or binding.
  ILLEGAL_VALUE,    ///< Value outside a documented setting's limits.
  UNSUPPORTED,      ///< Register, access, or feature outside the profile.
  CRC_ERROR,        ///< Received frame failed the CRC check.
  FRAME_ERROR,      ///< Invalid or mismatched reply.
  EXCEPTION,        ///< Validated Modbus exception. See Status::detail.
  NOT_INITIALIZED,  ///< Owner lacks an active binding.
  BUS_BUSY,         ///< Owner cannot admit the exchange.
  TIMEOUT,          ///< Response deadline expired.
  IO,               ///< Platform I/O failure. See Status::detail.
  NO_DATA,          ///< Requested sample is unavailable.
  DISALLOWED,       ///< Local write policy rejected admission.
  ABORTED,          ///< An admitted operation was cancelled.
  CONFIG_MISMATCH,  ///< Addressed readback differs from the requested value.
  LINK_LOST,        ///< The post-change endpoint could not be established.
};

/// Structured detail for FC03 response validation.
enum class ReadResponseError : std::uint8_t {
  NONE = 0,             ///< Response matches the expected shape.
  INVALID_ARGUMENT,     ///< Invalid validation input.
  FRAME_TOO_SHORT,      ///< Reply is shorter than required.
  CRC_MISMATCH,         ///< Received CRC differs from the calculated CRC.
  UNEXPECTED_ADDRESS,   ///< Reply came from a different address.
  UNEXPECTED_FUNCTION,  ///< Reply function differs from FC03 and its exception form.
  EXCEPTION_RESPONSE,   ///< Validated FC03 exception. See Status::detail.
  BYTE_COUNT_MISMATCH,  ///< Byte count differs from twice the expected register count.
  LENGTH_MISMATCH,      ///< Reply has bytes beyond the exact expected length.
};

/// Operation result with a signed numeric detail and static message storage.
struct Status {
  Err code = Err::OK;       ///< Result category for programmatic handling.
  std::int32_t detail = 0;  ///< Signed source detail or raw exception byte in the range 0–255.
  const char* msg = "";     ///< Diagnostic string with static storage duration.

  /// Construct a success with zero detail and an empty message.
  constexpr Status() = default;

  /**
   * @brief Construct a result with its diagnostic fields.
   * @param code Result category.
   * @param detail Signed source detail or raw exception byte, as specified by code.
   * @param msg Null-terminated text with static storage duration. The result retains this pointer.
   */
  constexpr Status(Err code, std::int32_t detail, const char* msg)
      : code(code), detail(detail), msg(msg) {}

  /// @return True for Err::OK.
  constexpr bool ok() const { return code == Err::OK; }

  /// @return The same result as ok().
  constexpr bool isOk() const { return ok(); }

  /// @return The same result as ok(), for explicit or contextual conversion.
  constexpr explicit operator bool() const { return ok(); }
};

/// @return Success with zero detail and an empty message.
constexpr Status Ok() {
  return {};
}

/**
 * @brief Look up a broad result's diagnostic name.
 * @param err Result category.
 * @return Static, null-terminated uppercase enum spelling or "UNKNOWN".
 */
const char* errToString(Err err);

/**
 * @brief Look up human-readable Modbus exception text.
 * @param code Unmodified exception byte from a validated reply.
 * @return Static, null-terminated text or "Unknown Exception".
 */
const char* modbusExceptionToString(std::uint8_t code);

}  // namespace SEN059x
