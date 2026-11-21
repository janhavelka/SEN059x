#include "SEN059x/WriteCodec.h"

#include "SEN059x/ReadCodec.h"
#include "WriteCodecInternal.h"

namespace SEN059x {
namespace {

constexpr std::size_t WRITE_FRAME_BYTES = 8;
constexpr std::size_t EXCEPTION_FRAME_BYTES = 5;
constexpr auto WRITE_FUNCTION = static_cast<std::uint8_t>(FunctionCode::WriteSingleRegister);
constexpr std::uint8_t WRITE_EXCEPTION_FUNCTION = WRITE_FUNCTION | 0x80;
constexpr std::uint8_t VENDOR_DESTINATION = 0xFF;

// DFRobot register table 2 lists the same eight writable registers for both models.
bool isWritableRegister(std::uint16_t reg) {
  switch (static_cast<Setting>(reg)) {
    case Setting::Address:
    case Setting::Baud:
    case Setting::SwitchPolarity:
    case Setting::SwitchThreshold:
    case Setting::Angle:
    case Setting::UartUnit:
    case Setting::Noise:
    case Setting::Range:
      return true;
  }
  return false;
}

std::size_t encodeWriteFrame(std::uint8_t address, std::uint16_t reg, std::uint16_t rawWord,
                             std::uint8_t* tx, std::size_t txCapacity) {
  if (tx == nullptr || txCapacity < WRITE_FRAME_BYTES || !isWritableRegister(reg)) {
    return 0;
  }
  tx[0] = address;
  tx[1] = WRITE_FUNCTION;
  tx[2] = static_cast<std::uint8_t>(reg >> 8);
  tx[3] = static_cast<std::uint8_t>(reg);
  tx[4] = static_cast<std::uint8_t>(rawWord >> 8);
  tx[5] = static_cast<std::uint8_t>(rawWord);
  const auto crc = calcCrc16(tx, WRITE_FRAME_BYTES - 2);
  tx[6] = static_cast<std::uint8_t>(crc);
  tx[7] = static_cast<std::uint8_t>(crc >> 8);
  return WRITE_FRAME_BYTES;
}

std::uint16_t readWord(const std::uint8_t* bytes) {
  return static_cast<std::uint16_t>((std::uint16_t{bytes[0]} << 8) | bytes[1]);
}

Status rejectFrame(WriteResponseError& error, WriteResponseError reason, const char* message) {
  error = reason;
  return {Err::FRAME_ERROR, 0, message};
}

}  // namespace

/// @cond INTERNAL
namespace detail {

std::size_t encodeAddressedWriteSingleRegister(DeviceModel model, std::uint8_t address,
                                               std::uint16_t reg, std::uint16_t rawWord,
                                               std::uint8_t* tx, std::size_t txCapacity,
                                               RequestContext& out, AddressPolicy addressPolicy) {
  RequestContext request;
  if (!makeRequestContext(model, address, FunctionCode::WriteSingleRegister, reg, rawWord, request,
                          addressPolicy)) {
    return 0;
  }
  const auto length = encodeWriteFrame(address, reg, rawWord, tx, txCapacity);
  if (length != 0) {
    out = request;
  }
  return length;
}

std::size_t encodeIsolatedWriteSingleRegister(std::uint16_t reg, std::uint16_t rawWord,
                                              std::uint8_t* tx, std::size_t txCapacity) {
  return encodeWriteFrame(VENDOR_DESTINATION, reg, rawWord, tx, txCapacity);
}

}  // namespace detail
/// @endcond

Status parseWriteSingleRegister(const std::uint8_t* rx, std::size_t rxLength,
                                const RequestContext& expected, WriteResponseError& error) {
  error = WriteResponseError::NONE;
  RequestContext checked;
  if (rx == nullptr || expected.function != FunctionCode::WriteSingleRegister ||
      !makeRequestContext(expected.model, expected.address, expected.function,
                          expected.startRegister, expected.countOrWriteWord, checked,
                          expected.addressPolicy) ||
      !isWritableRegister(expected.startRegister)) {
    error = WriteResponseError::INVALID_ARGUMENT;
    return {Err::INVALID_CONFIG, 0, "Invalid FC06 context or buffer"};
  }

  if (rxLength < EXCEPTION_FRAME_BYTES) {
    return rejectFrame(error, WriteResponseError::FRAME_TOO_SHORT, "Reply shorter than five bytes");
  }
  if (rxLength > WRITE_FRAME_BYTES) {
    return rejectFrame(error, WriteResponseError::LENGTH_MISMATCH, "Reply exceeds eight bytes");
  }
  const auto crc = calcCrc16(rx, rxLength - 2);
  if (rx[rxLength - 2] != static_cast<std::uint8_t>(crc) || rx[rxLength - 1] != crc >> 8) {
    error = WriteResponseError::CRC_MISMATCH;
    return {Err::CRC_ERROR, 0, "Reply CRC mismatch"};
  }
  if (rx[0] != expected.address) {
    return rejectFrame(error, WriteResponseError::UNEXPECTED_ADDRESS, "Unexpected reply address");
  }
  if (rx[1] == WRITE_EXCEPTION_FUNCTION) {
    if (rxLength != EXCEPTION_FRAME_BYTES) {
      return rejectFrame(error, WriteResponseError::LENGTH_MISMATCH,
                         "Invalid FC06 exception length");
    }
    error = WriteResponseError::EXCEPTION_RESPONSE;
    return {Err::EXCEPTION, rx[2], modbusExceptionToString(rx[2])};
  }
  if (rx[1] != WRITE_FUNCTION) {
    return rejectFrame(error, WriteResponseError::UNEXPECTED_FUNCTION, "Unexpected reply function");
  }
  if (rxLength != WRITE_FRAME_BYTES) {
    return rejectFrame(error, WriteResponseError::FRAME_TOO_SHORT, "Incomplete FC06 echo");
  }
  if (readWord(rx + 2) != expected.startRegister) {
    return rejectFrame(error, WriteResponseError::REGISTER_MISMATCH, "Unexpected echoed register");
  }
  if (readWord(rx + 4) != expected.countOrWriteWord) {
    return rejectFrame(error, WriteResponseError::VALUE_MISMATCH, "Unexpected echoed value");
  }
  return Ok();
}

}  // namespace SEN059x
