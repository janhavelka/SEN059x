/** @file ReadCodec.h
 *  @brief CRC and addressed FC03 frame codec for SEN0591 and SEN0592.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "Types.h"

namespace SEN059x {

/**
 * @brief Calculate CRC-16/MODBUS with seed 0xFFFF and reflected polynomial 0xA001.
 * @param data Input buffer readable for length bytes.
 * @param length Number of bytes to include.
 * @return The CRC as a word, or the seed for a null or empty input. Transmit the low byte first.
 */
std::uint16_t calcCrc16(const std::uint8_t* data, std::size_t length);

/**
 * @brief Build an addressed FC03 request for a contiguous span of documented registers.
 *
 * The manuals demonstrate single-word reads for both models. Device acceptance
 * and sampling coherence of multi-word reads remain unverified for each model.
 *
 * @param model Explicitly selected sensor model.
 * @param address Destination byte under addressPolicy.
 * @param startRegister First register address.
 * @param count Number of 16-bit registers, from 1 to 125 at the wire level.
 * The longest documented span is three registers.
 * @param tx Writable byte buffer with at least eight bytes, disjoint from out.
 * @param txCapacity Size of tx in bytes.
 * @param out Caller-owned request identity to retain until the exchange is handled.
 * @param addressPolicy Address policy. Use DFRobotIsolated only on an isolated link.
 * @return Eight on success, or zero for an invalid argument or undocumented span.
 * The request bytes and context are assigned only on success.
 */
std::size_t buildReadRegisters(DeviceModel model, std::uint8_t address, std::uint16_t startRegister,
                               std::uint16_t count, std::uint8_t* tx, std::size_t txCapacity,
                               RequestContext& out,
                               AddressPolicy addressPolicy = AddressPolicy::StandardUnicast);

/**
 * @brief Validate a complete addressed FC03 reply and extract ordered raw words.
 *
 * The bus owner associates the reply with the outstanding request. The wire
 * reply omits the starting register.
 *
 * Validation proceeds through arguments, wire length bounds (5 through 255 bytes),
 * CRC, address, and function. An addressed 0x83 must have exactly five bytes.
 * For 0x03, expected byte count precedes the exact length check. A short normal
 * reply with the expected byte count yields FRAME_TOO_SHORT. Oversize replies
 * and padded exceptions that reach the exact length check yield LENGTH_MISMATCH.
 *
 * @param rx Readable buffer with the complete reply bytes.
 * @param rxLength Actual received length in bytes.
 * @param expected Retained FC03 request context from buildReadRegisters.
 * @param words Writable output buffer, disjoint from rx, expected and error.
 * Exactly the requested words are assigned on success. They remain raw register values.
 * @param wordCapacity Capacity in 16-bit words, at least the requested count.
 * @param error Structured result assigned on every call, including NONE on success.
 * Keep its storage disjoint from rx and expected.
 * @return OK on success, INVALID_CONFIG for invalid arguments, CRC_ERROR for bad CRC,
 * EXCEPTION with the full code in Status::detail for a valid exception, or FRAME_ERROR
 * for other frame rejections. The word buffer is assigned only on success.
 */
Status validateReadResponseExpected(const std::uint8_t* rx, std::size_t rxLength,
                                    const RequestContext& expected, std::uint16_t* words,
                                    std::size_t wordCapacity, ReadResponseError& error);

}  // namespace SEN059x
