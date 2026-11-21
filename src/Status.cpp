#include "SEN059x/Status.h"

namespace SEN059x {

const char* errToString(Err err) {
  switch (err) {
    case Err::OK:
      return "OK";
    case Err::INVALID_CONFIG:
      return "INVALID_CONFIG";
    case Err::ILLEGAL_VALUE:
      return "ILLEGAL_VALUE";
    case Err::UNSUPPORTED:
      return "UNSUPPORTED";
    case Err::CRC_ERROR:
      return "CRC_ERROR";
    case Err::FRAME_ERROR:
      return "FRAME_ERROR";
    case Err::EXCEPTION:
      return "EXCEPTION";
    case Err::NOT_INITIALIZED:
      return "NOT_INITIALIZED";
    case Err::BUS_BUSY:
      return "BUS_BUSY";
    case Err::TIMEOUT:
      return "TIMEOUT";
    case Err::IO:
      return "IO";
    case Err::NO_DATA:
      return "NO_DATA";
    case Err::DISALLOWED:
      return "DISALLOWED";
    case Err::ABORTED:
      return "ABORTED";
    case Err::CONFIG_MISMATCH:
      return "CONFIG_MISMATCH";
    case Err::LINK_LOST:
      return "LINK_LOST";
  }
  return "UNKNOWN";
}

const char* modbusExceptionToString(std::uint8_t code) {
  switch (code) {
    case 0x01:
      return "Illegal Function";
    case 0x02:
      return "Illegal Data Address";
    case 0x03:
      return "Illegal Data Value";
    case 0x04:
      return "Slave Device Failure";
    case 0x05:
      return "Acknowledge";
    case 0x06:
      return "Slave Device Busy";
    case 0x07:
      return "Negative Acknowledge";
    case 0x08:
      return "Memory Parity Error";
    default:
      return "Unknown Exception";
  }
}

}  // namespace SEN059x
