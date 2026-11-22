#include "SEN059x/Settings.h"

#include "../internal/WriteCodecInternal.h"
#include "SEN059x/ReadCodec.h"

namespace SEN059x {
namespace {

bool validWriteValue(DeviceModel model, Setting setting, SettingValue value, AddressPolicy policy) {
  SettingDescriptor descriptor;
  if (!settingDescriptor(model, setting, descriptor) || value.unit != descriptor.unit ||
      value.word < descriptor.minWord || value.word > descriptor.maxWord) {
    return false;
  }
  // The descriptor already bounded the full 16-bit address word to 1..254.
  return setting != Setting::Address ||
         isValidAddress(static_cast<std::uint8_t>(value.word), policy);
}

}  // namespace

std::size_t buildSettingRead(DeviceModel model, std::uint8_t address, Setting setting,
                             std::uint8_t* tx, std::size_t txCapacity, SettingRequest& out,
                             AddressPolicy addressPolicy) {
  SettingDescriptor descriptor;
  if (!settingDescriptor(model, setting, descriptor)) {
    return 0;
  }
  return buildReadRegisters(model, address, static_cast<std::uint16_t>(setting), 1, tx, txCapacity,
                            out.context_, addressPolicy);
}

Status decodeSettingResponse(const std::uint8_t* rx, std::size_t rxLength,
                             const SettingRequest& expected, SettingReadback& out,
                             ReadResponseError& error) {
  const auto& request = expected.context();
  std::uint16_t word = 0;
  const auto status = validateReadResponseExpected(rx, rxLength, request, &word, 1, error);
  if (!status) {
    return status;
  }
  const auto setting = static_cast<Setting>(request.startRegister);
  SettingDescriptor descriptor;
  const bool known = settingDescriptor(request.model, setting, descriptor) &&
                     word >= descriptor.minWord && word <= descriptor.maxWord;
  out = {request.model, request.address, setting, word, known};
  return status;
}

std::size_t buildWriteSingleRegister(DeviceModel model, std::uint8_t address, Setting setting,
                                     SettingValue value, std::uint8_t* tx, std::size_t txCapacity,
                                     RequestContext& out, AddressPolicy addressPolicy) {
  if (!validWriteValue(model, setting, value, addressPolicy)) {
    return 0;
  }
  return detail::encodeAddressedWriteSingleRegister(model, address,
                                                    static_cast<std::uint16_t>(setting), value.word,
                                                    tx, txCapacity, out, addressPolicy);
}

std::size_t buildVendorBroadcastWrite(DeviceModel model, Setting setting, SettingValue value,
                                      std::uint8_t* tx, std::size_t txCapacity,
                                      AddressPolicy addressPolicy) {
  if (addressPolicy != AddressPolicy::DFRobotIsolated ||
      !validWriteValue(model, setting, value, addressPolicy)) {
    return 0;
  }
  return detail::encodeIsolatedWriteSingleRegister(static_cast<std::uint16_t>(setting), value.word,
                                                   tx, txCapacity);
}

}  // namespace SEN059x
