#include "SEN059x/ReadCodec.h"

namespace SEN059x {
namespace {

constexpr std::size_t READ_REQUEST_BYTES = 8;
constexpr std::size_t MIN_READ_RESPONSE_BYTES = 5;
constexpr std::size_t MAX_READ_RESPONSE_BYTES = 255;
constexpr auto READ_FUNCTION = static_cast<std::uint8_t>(FunctionCode::ReadHoldingRegisters);
constexpr std::uint8_t READ_EXCEPTION_FUNCTION = READ_FUNCTION | 0x80;
constexpr std::uint16_t CRC_SEED = 0xFFFF;
constexpr std::uint16_t CRC_POLYNOMIAL = 0xA001;

struct RegisterRange {
  std::uint16_t first;
  std::uint16_t last;
};

// Both models publish the same readable addresses in DFRobot register tables 1 and 2.
constexpr RegisterRange READABLE_RANGES[] = {{0x0100, 0x0102}, {0x010A, 0x010A}, {0x0200, 0x0201},
                                             {0x0205, 0x0206}, {0x0208, 0x0209}, {0x021A, 0x021A},
                                             {0x021F, 0x021F}};

// The caller has already validated the wire count and address space.
bool isDocumentedReadSpan(std::uint16_t startRegister, std::uint16_t count) {
  const std::uint32_t lastRegister = std::uint32_t{startRegister} + count - 1;
  for (const auto& range : READABLE_RANGES) {
    if (startRegister >= range.first && lastRegister <= range.last) {
      return true;
    }
  }
  return false;
}

Status rejectFrame(ReadResponseError& error, ReadResponseError reason, const char* message) {
  error = reason;
  return {Err::FRAME_ERROR, 0, message};
}

}  // namespace

std::uint16_t calcCrc16(const std::uint8_t* data, std::size_t length) {
  std::uint16_t crc = CRC_SEED;
  if (data == nullptr) {
    return crc;
  }
  for (std::size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const bool lowBit = (crc & 1) != 0;
      crc >>= 1;
      if (lowBit) {
        crc ^= CRC_POLYNOMIAL;
      }
    }
  }
  return crc;
}

std::size_t buildReadRegisters(DeviceModel model, std::uint8_t address, std::uint16_t startRegister,
                               std::uint16_t count, std::uint8_t* tx, std::size_t txCapacity,
                               RequestContext& out, AddressPolicy addressPolicy) {
  RequestContext request;
  if (tx == nullptr || txCapacity < READ_REQUEST_BYTES ||
      !makeRequestContext(model, address, FunctionCode::ReadHoldingRegisters, startRegister, count,
                          request, addressPolicy) ||
      !isDocumentedReadSpan(startRegister, count)) {
    return 0;
  }

  tx[0] = address;
  tx[1] = READ_FUNCTION;
  tx[2] = static_cast<std::uint8_t>(startRegister >> 8);
  tx[3] = static_cast<std::uint8_t>(startRegister);
  tx[4] = static_cast<std::uint8_t>(count >> 8);
  tx[5] = static_cast<std::uint8_t>(count);
  const auto crc = calcCrc16(tx, READ_REQUEST_BYTES - 2);
  tx[6] = static_cast<std::uint8_t>(crc);
  tx[7] = static_cast<std::uint8_t>(crc >> 8);
  out = request;
  return READ_REQUEST_BYTES;
}

Status validateReadResponseExpected(const std::uint8_t* rx, std::size_t rxLength,
                                    const RequestContext& expected, std::uint16_t* words,
                                    std::size_t wordCapacity, ReadResponseError& error) {
  error = ReadResponseError::NONE;
  const auto count = expected.countOrWriteWord;
  RequestContext checked;
  if (rx == nullptr || words == nullptr || wordCapacity < count ||
      expected.function != FunctionCode::ReadHoldingRegisters ||
      !makeRequestContext(expected.model, expected.address, expected.function,
                          expected.startRegister, count, checked, expected.addressPolicy) ||
      !isDocumentedReadSpan(expected.startRegister, count)) {
    error = ReadResponseError::INVALID_ARGUMENT;
    return {Err::INVALID_CONFIG, 0, "Invalid FC03 context or buffer"};
  }

  if (rxLength < MIN_READ_RESPONSE_BYTES) {
    return rejectFrame(error, ReadResponseError::FRAME_TOO_SHORT, "Reply shorter than five bytes");
  }
  if (rxLength > MAX_READ_RESPONSE_BYTES) {
    return rejectFrame(error, ReadResponseError::LENGTH_MISMATCH, "Reply exceeds FC03 wire limit");
  }
  const auto crc = calcCrc16(rx, rxLength - 2);
  if (rx[rxLength - 2] != static_cast<std::uint8_t>(crc) || rx[rxLength - 1] != crc >> 8) {
    error = ReadResponseError::CRC_MISMATCH;
    return {Err::CRC_ERROR, 0, "Reply CRC mismatch"};
  }
  if (rx[0] != expected.address) {
    return rejectFrame(error, ReadResponseError::UNEXPECTED_ADDRESS, "Unexpected reply address");
  }
  if (rx[1] == READ_EXCEPTION_FUNCTION) {
    if (rxLength != MIN_READ_RESPONSE_BYTES) {
      return rejectFrame(error, ReadResponseError::LENGTH_MISMATCH,
                         "Invalid FC03 exception length");
    }
    error = ReadResponseError::EXCEPTION_RESPONSE;
    return {Err::EXCEPTION, rx[2], modbusExceptionToString(rx[2])};
  }
  if (rx[1] != READ_FUNCTION) {
    return rejectFrame(error, ReadResponseError::UNEXPECTED_FUNCTION, "Unexpected reply function");
  }

  const std::size_t byteCount = 2u * count;
  if (rx[2] != byteCount) {
    return rejectFrame(error, ReadResponseError::BYTE_COUNT_MISMATCH, "Unexpected FC03 byte count");
  }
  const std::size_t expectedLength = MIN_READ_RESPONSE_BYTES + byteCount;
  if (rxLength < expectedLength) {
    return rejectFrame(error, ReadResponseError::FRAME_TOO_SHORT, "Incomplete FC03 payload");
  }
  if (rxLength > expectedLength) {
    return rejectFrame(error, ReadResponseError::LENGTH_MISMATCH, "Extra bytes in FC03 reply");
  }

  for (std::size_t i = 0; i < count; ++i) {
    const auto offset = 3 + 2 * i;
    words[i] = static_cast<std::uint16_t>((std::uint16_t{rx[offset]} << 8) | rx[offset + 1]);
  }
  return Ok();
}

}  // namespace SEN059x
