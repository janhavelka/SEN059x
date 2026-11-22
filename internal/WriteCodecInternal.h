#pragma once

// Keep this header outside the consumer include paths for the package.
#include <cstddef>
#include <cstdint>

#include "SEN059x/Types.h"

/// @cond INTERNAL
namespace SEN059x::detail {

// For checked setting builders. Validate model-specific values before encoding.
// tx and out must be disjoint. Returns eight bytes or zero, with no outputs changed on failure.
std::size_t encodeAddressedWriteSingleRegister(
    DeviceModel model, std::uint8_t address, std::uint16_t reg, std::uint16_t rawWord,
    std::uint8_t* tx, std::size_t txCapacity, RequestContext& out,
    AddressPolicy addressPolicy = AddressPolicy::StandardUnicast);

// For an explicitly isolated link. Emits destination 0xFF without an addressed context.
// Returns eight bytes or zero, with tx unchanged on failure.
std::size_t encodeIsolatedWriteSingleRegister(std::uint16_t reg, std::uint16_t rawWord,
                                              std::uint8_t* tx, std::size_t txCapacity);

}  // namespace SEN059x::detail
/// @endcond
