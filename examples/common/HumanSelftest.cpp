#include <array>
#include <iterator>

#include "HumanConsole.h"

#include <SEN059x/ReadCodec.h>
#include <SEN059x/Settings.h>
#include <SEN059x/WriteCodec.h>

namespace SEN059x::example {
namespace {
using RequestFrame = std::array<std::uint8_t, 8>;

struct ReadVector {
  const char* name;
  std::uint16_t reg;
  std::uint16_t word;
  RequestFrame request;
  std::array<std::uint8_t, 7> reply;
};

struct WriteVector {
  const char* name;
  Setting setting;
  SettingValue value;
  RequestFrame frame;  // The published FC06 reply echoes the request exactly.
};

// DFRobot SEN0591/SEN0592 examples 1-6, transcribed in docs/reference/protocol/modbus_vectors.csv.
// Example 4 uses the complete eight-byte frame in both Chinese manuals and echoes address 1.
constexpr ReadVector READS[] = {
    {"read_processed", 0x0100, 754, {1, 3, 1, 0, 0, 1, 0x85, 0xF6}, {1, 3, 2, 2, 0xF2, 0x38, 0xA1}},
    {"read_realtime", 0x0101, 751, {1, 3, 1, 1, 0, 1, 0xD4, 0x36}, {1, 3, 2, 2, 0xEF, 0xF8, 0xA8}},
    {"read_temperature", 0x0102, 300, {1, 3, 1, 2, 0, 1, 0x24, 0x36}, {1, 3, 2, 1, 0x2C, 0xB8, 9}},
    {"read_baud", 0x0201, 3, {1, 3, 2, 1, 0, 1, 0xD4, 0x72}, {1, 3, 2, 0, 3, 0xF8, 0x45}},
};
constexpr WriteVector WRITES[] = {
    {"set_address_5", Setting::Address, {SettingUnit::Address, 5}, {1, 6, 2, 0, 0, 5, 0x48, 0x71}},
    {"set_baud_9600", Setting::Baud, {SettingUnit::BaudCode, 3}, {1, 6, 2, 1, 0, 3, 0x99, 0xB3}},
};

bool replay(DeviceModel model, const ReadVector& vector) {
  RequestFrame tx{};
  RequestContext request;
  ReadResponseError error;
  std::uint16_t word = 0;
  return buildReadRegisters(model, 1, vector.reg, 1, tx.data(), tx.size(), request) == tx.size() &&
         tx == vector.request &&
         validateReadResponseExpected(vector.reply.data(), vector.reply.size(), request, &word, 1,
                                      error) &&
         word == vector.word;
}

bool replay(DeviceModel model, const WriteVector& vector) {
  RequestFrame tx{};
  RequestContext request;
  WriteResponseError error;
  return buildWriteSingleRegister(model, 1, vector.setting, vector.value, tx.data(), tx.size(),
                                  request) == tx.size() &&
         tx == vector.frame &&
         parseWriteSingleRegister(vector.frame.data(), vector.frame.size(), request, error);
}
}  // namespace

void HumanConsole::selftest() {
  info("Offline frame vectors (SEN0591 and SEN0592)");
  unsigned passed = 0;
  const auto report = [this, &passed](const auto& vector) {
    const bool shortRange = replay(DeviceModel::SEN0591, vector);
    const bool longRange = replay(DeviceModel::SEN0592, vector);
    if (shortRange && longRange) {
      ++passed;
      info(vector.name);
    } else {
      if (!machine_)
        error(vector.name);
    }
  };
  for (const auto& vector : READS) {
    report(vector);
  }
  for (const auto& vector : WRITES) {
    report(vector);
  }
  if (machine_) {
    json_.integer("passed", passed);
    json_.integer("total", std::size(READS) + std::size(WRITES));
    if (passed != std::size(READS) + std::size(WRITES))
      statusError({Err::FRAME_ERROR, 0, "Offline frame vector failed"});
    return;
  }
  append(passed == std::size(READS) + std::size(WRITES) ? "[I] Passed " : "[E] Passed ");
  number(passed);
  append("/");
  number(std::size(READS) + std::size(WRITES));
  append(" offline frame vectors\n");
}

}  // namespace SEN059x::example
