#include "SEN059x/Types.h"

namespace SEN059x {

bool isValidAddress(std::uint8_t address, AddressPolicy addressPolicy) {
  switch (addressPolicy) {
    case AddressPolicy::StandardUnicast:
      return address >= 1 && address <= 247;
    case AddressPolicy::DFRobotIsolated:
      return address >= 1 && address <= 254;
  }
  return false;
}

Status makeRequestContext(DeviceModel model, std::uint8_t address, FunctionCode function,
                          std::uint16_t startRegister, std::uint16_t countOrWriteWord,
                          RequestContext& out, AddressPolicy addressPolicy) {
  if (model != DeviceModel::SEN0591 && model != DeviceModel::SEN0592) {
    return {Err::INVALID_CONFIG, 0, "Select SEN0591 or SEN0592"};
  }
  if (!isValidAddress(address, addressPolicy)) {
    return {Err::INVALID_CONFIG, 0, "Invalid destination or address policy"};
  }
  switch (function) {
    case FunctionCode::ReadHoldingRegisters:
      if (countOrWriteWord == 0 || countOrWriteWord > 125 ||
          std::uint32_t{startRegister} + countOrWriteWord > 0x10000u) {
        return {Err::INVALID_CONFIG, 0, "Invalid read register span"};
      }
      break;
    case FunctionCode::WriteSingleRegister:
      break;
    default:
      return {Err::INVALID_CONFIG, 0, "Invalid request function"};
  }

  out = {model, address, function, startRegister, countOrWriteWord, addressPolicy};
  return Ok();
}

}  // namespace SEN059x
