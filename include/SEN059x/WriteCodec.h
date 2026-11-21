/** @file WriteCodec.h
 *  @brief Addressed FC06 response validation for SEN0591 and SEN0592.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "Types.h"

namespace SEN059x {

/// Structured detail for FC06 response validation.
enum class WriteResponseError : std::uint8_t {
  NONE = 0,             ///< Response matches the expected shape.
  INVALID_ARGUMENT,     ///< Invalid response buffer or addressed FC06 context.
  FRAME_TOO_SHORT,      ///< Reply is shorter than required.
  CRC_MISMATCH,         ///< Received CRC differs from the calculated CRC.
  UNEXPECTED_ADDRESS,   ///< Reply address differs from the request destination.
  UNEXPECTED_FUNCTION,  ///< Reply function differs from FC06 and its exception form.
  EXCEPTION_RESPONSE,   ///< Validated FC06 exception. See Status::detail.
  LENGTH_MISMATCH,      ///< Reply has bytes beyond the exact expected length.
  REGISTER_MISMATCH,    ///< Echoed register differs from the requested register.
  VALUE_MISMATCH,       ///< Echoed value differs from the requested raw word.
};

/**
 * @brief Validate an addressed FC06 reply against the retained register and raw word.
 *
 * A normal reply is exactly eight bytes and matches the request byte for
 * byte. An adapter's transmit echo can pass the same check. The bus owner
 * qualifies the origin of the reply. A separate addressed readback can
 * establish the observed setting.
 *
 * @param rx Readable buffer containing the complete reply.
 * @param rxLength Actual frame length in bytes.
 * @param expected Retained FC06 context with the original address and its
 * policy, a documented writable register, and the raw requested word.
 * Vendor writes to 0xFF have no addressed context.
 * @param error Structured result assigned on every call, including NONE on
 * success. Keep its storage disjoint from rx and expected.
 * @return OK for a matching eight-byte reply, INVALID_CONFIG for invalid
 * arguments, CRC_ERROR for a bad CRC, EXCEPTION for a matching five-byte
 * 0x86 reply with the raw exception byte in Status::detail, or FRAME_ERROR
 * for other frame rejections.
 */
Status parseWriteSingleRegister(const std::uint8_t* rx, std::size_t rxLength,
                                const RequestContext& expected, WriteResponseError& error);

}  // namespace SEN059x
