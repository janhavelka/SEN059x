#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <SEN059x/SEN059x.h>

namespace {
using namespace SEN059x;
using Frame = std::array<std::uint8_t, 8>;
using Reply = std::array<std::uint8_t, 7>;
constexpr auto STANDARD = AddressPolicy::StandardUnicast;
constexpr auto ISOLATED = AddressPolicy::DFRobotIsolated;
constexpr auto SHORT_MODEL = DeviceModel::SEN0591;
constexpr auto LONG_MODEL = DeviceModel::SEN0592;
constexpr Frame CANARY{11, 22, 33, 44, 55, 66, 77, 88};
constexpr RequestContext PRIOR{LONG_MODEL, 253, FunctionCode::ReadHoldingRegisters,
                               0x0100,     3,   ISOLATED};

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

struct SettingCase {
  Setting setting;
  SettingUnit unit;
  SettingMode mode;
  std::uint16_t reg, minWord, maxShort, maxLong;
  std::optional<std::uint16_t> defaultShort, defaultLong;
  std::uint16_t readWord, writeWord;
  std::uint16_t readCrc, replyCrc, writeCrc, vendorCrc;
};

// Table 2 from each SKU and the independent words in OPERATIONS.csv. CRCs were calculated
// offline for each request/reply; the address-5 and baud-9600 cases also appear in the manuals.
constexpr SettingCase CASES[] = {
    {Setting::Address, SettingUnit::Address, SettingMode::Modbus, 0x0200, 1, 254, 254, 1, 1, 1, 5,
     0xb285, 0x8479, 0x7148, 0xaf5d},
    {Setting::Baud, SettingUnit::BaudCode, SettingMode::Modbus, 0x0201, 2, 9, 9, 9, 9, 3, 3, 0x72d4,
     0x45f8, 0xb399, 0x6d8c},
    {Setting::SwitchPolarity, SettingUnit::PolarityCode, SettingMode::SwitchOutput, 0x0205, 0, 1, 1,
     1, 1, 1, 0, 0xb395, 0x8479, 0x7398, 0xad8d},
    {Setting::SwitchThreshold,
     SettingUnit::Millimetres,
     SettingMode::SwitchOutput,
     0x0206,
     30,
     3000,
     5000,
     {},
     {},
     100,
     750,
     0xb365,
     0xafb9,
     0x5fe9,
     0x81fc},
    {Setting::Angle, SettingUnit::AngleLevel, SettingMode::Sensor, 0x0208, 1, 4, 4, 4, 4, 4, 2,
     0x7004, 0x87b9, 0x7188, 0xaf9d},
    {Setting::UartUnit,
     SettingUnit::UartUnitCode,
     SettingMode::AutomaticOrControlledUart,
     0x0209,
     0,
     1,
     1,
     {},
     {},
     0,
     1,
     0xb055,
     0x44b8,
     0xb099,
     0x6e8c},
    {Setting::Noise, SettingUnit::NoiseLevel, SettingMode::Sensor, 0x021a, 1, 5, 5, 1, 1, 1, 4,
     0x75a4, 0x8479, 0x76a8, 0xa8bd},
    {Setting::Range, SettingUnit::RangeLevel, SettingMode::Sensor, 0x021f, 1, 4, 5, 4, 5, 1, 3,
     0x74b4, 0x8479, 0xb5f9, 0x6bec},
};

std::uint8_t lo(std::uint16_t word) {
  return static_cast<std::uint8_t>(word);
}
std::uint8_t hi(std::uint16_t word) {
  return static_cast<std::uint8_t>(word >> 8);
}

Frame frame(std::uint8_t address, std::uint8_t function, std::uint16_t reg, std::uint16_t word,
            std::uint16_t crc) {
  return {address, function, hi(reg), lo(reg), hi(word), lo(word), lo(crc), hi(crc)};
}

bool sameContext(const RequestContext& a, const RequestContext& b) {
  return a.model == b.model && a.address == b.address && a.function == b.function &&
         a.startRegister == b.startRegister && a.countOrWriteWord == b.countOrWriteWord &&
         a.addressPolicy == b.addressPolicy;
}

void checkWord(const SettingRequest& request, const Reply& reply, bool known,
               const SettingCase& test, DeviceModel model, std::uint16_t word) {
  SettingReadback out{};
  out.known = !known;
  ReadResponseError error = ReadResponseError::INVALID_ARGUMENT;
  CHECK(decodeSettingResponse(reply.data(), reply.size(), request, out, error));
  CHECK(error == ReadResponseError::NONE && out.known == known);
  CHECK(out.model == model && out.address == 1 && out.setting == test.setting &&
        out.rawWord == word);
}

void checkOperation(DeviceModel model, const SettingCase& test) {
  const bool shortModel = model == SHORT_MODEL;
  SettingDescriptor descriptor{};
  CHECK(settingDescriptor(model, test.setting, descriptor));
  CHECK(descriptor.unit == test.unit && descriptor.mode == test.mode);
  CHECK(descriptor.minWord == test.minWord);
  CHECK(descriptor.maxWord == (shortModel ? test.maxShort : test.maxLong));
  CHECK(descriptor.factoryWord == (shortModel ? test.defaultShort : test.defaultLong));

  Frame tx{};
  SettingRequest read;
  CHECK(buildSettingRead(model, 1, test.setting, tx.data(), tx.size(), read) == 8);
  CHECK(tx == frame(1, 3, test.reg, 1, test.readCrc));
  CHECK(sameContext(read.context(),
                    {model, 1, FunctionCode::ReadHoldingRegisters, test.reg, 1, STANDARD}));
  const Reply reply{
      1, 3, 2, hi(test.readWord), lo(test.readWord), lo(test.replyCrc), hi(test.replyCrc)};
  checkWord(read, reply, true, test, model, test.readWord);

  const SettingValue value{test.unit, test.writeWord};
  RequestContext write;
  CHECK(buildWriteSingleRegister(model, 1, test.setting, value, tx.data(), tx.size(), write) == 8);
  CHECK(tx == frame(1, 6, test.reg, test.writeWord, test.writeCrc));
  CHECK(sameContext(
      write, {model, 1, FunctionCode::WriteSingleRegister, test.reg, test.writeWord, STANDARD}));
  CHECK(buildVendorBroadcastWrite(model, test.setting, value, tx.data(), tx.size(), ISOLATED) == 8);
  CHECK(tx == frame(255, 6, test.reg, test.writeWord, test.vendorCrc));
}

void checkBuilders(DeviceModel model, Setting setting, SettingValue value, bool addressed,
                   bool vendor, std::uint8_t address = 1, AddressPolicy policy = ISOLATED,
                   std::size_t capacity = 8) {
  Frame tx = CANARY;
  auto retained = PRIOR;
  const auto reg = static_cast<std::uint16_t>(setting);
  auto checkWrite = [&](std::uint8_t destination) {
    auto expected = frame(destination, 6, reg, value.word, 0);
    const auto crc = calcCrc16(expected.data(), 6);
    expected[6] = lo(crc);
    expected[7] = hi(crc);
    CHECK(tx == expected);
  };
  CHECK(buildWriteSingleRegister(model, address, setting, value, tx.data(), capacity, retained,
                                 policy) == (addressed ? 8u : 0u));
  if (addressed) {
    checkWrite(address);
    CHECK(sameContext(
        retained, {model, address, FunctionCode::WriteSingleRegister, reg, value.word, policy}));
  } else {
    CHECK(tx == CANARY && sameContext(retained, PRIOR));
  }
  tx = CANARY;
  CHECK(buildVendorBroadcastWrite(model, setting, value, tx.data(), capacity, policy) ==
        (vendor ? 8u : 0u));
  if (vendor) {
    checkWrite(255);
  } else {
    CHECK(tx == CANARY);
  }
}

void checkBounds(DeviceModel model, const SettingCase& test) {
  const auto maxWord = model == SHORT_MODEL ? test.maxShort : test.maxLong;
  Frame tx{};
  SettingRequest read;
  CHECK(buildSettingRead(model, 1, test.setting, tx.data(), tx.size(), read) == 8);
  auto exercise = [&](std::uint16_t word) {
    const bool known = word >= test.minWord && word <= maxWord;
    checkBuilders(model, test.setting, {test.unit, word}, known, known);
    Reply reply{1, 3, 2, hi(word), lo(word), 0, 0};
    const auto crc = calcCrc16(reply.data(), 5);
    reply[5] = lo(crc);
    reply[6] = hi(crc);
    checkWord(read, reply, known, test, model, word);
    if (!known) {
      CHECK(settingWordDescription(model, test.setting, word) == nullptr);
    }
  };
  if (test.setting == Setting::SwitchThreshold) {
    for (auto word : {29, 30, 3000, 3001, 5000, 5001})
      exercise(word);
  } else if (test.setting == Setting::Address) {
    for (auto word : {0, 1, 247, 248, 254, 255, 256, 257, 65535})
      exercise(word);
  } else {
    for (std::uint16_t word = 0; word <= test.maxLong + 1; ++word)
      exercise(word);
  }
  const auto wrongUnit =
      test.unit == SettingUnit::Address ? SettingUnit::BaudCode : SettingUnit::Address;
  checkBuilders(model, test.setting, {wrongUnit, test.writeWord}, false, false);
}

void checkPolicies(DeviceModel model) {
  constexpr auto setting = Setting::Address;
  constexpr auto unit = SettingUnit::Address;
  for (auto address : {0, 255}) {
    checkBuilders(model, setting, {unit, 1}, false, false, address, STANDARD);
    checkBuilders(model, setting, {unit, 1}, false, true, address, ISOLATED);
  }
  for (auto word : {0, 248, 254, 255, 256, 257, 65535}) {
    checkBuilders(model, setting, {unit, static_cast<std::uint16_t>(word)}, false, false, 247,
                  STANDARD);
  }
  checkBuilders(model, setting, {unit, 247}, true, false, 247, STANDARD);
  checkBuilders(model, setting, {unit, 1}, false, false, 254, STANDARD);
  constexpr Frame changes[] = {{0xf7, 6, 2, 0, 0, 248, 0x9d, 0x66},
                               {0xfe, 6, 2, 0, 0, 1, 0x5d, 0xbd}};
  for (const auto& expected : changes) {
    Frame tx{};
    RequestContext retained;
    CHECK(buildWriteSingleRegister(model, expected[0], setting, {unit, expected[5]}, tx.data(),
                                   tx.size(), retained, ISOLATED) == 8);
    CHECK(tx == expected);
    CHECK(sameContext(retained, {model, expected[0], FunctionCode::WriteSingleRegister, 0x0200,
                                 expected[5], ISOLATED}));
  }
}

void checkBadArguments(DeviceModel model) {
  Frame tx{};
  SettingRequest read;
  CHECK(buildSettingRead(model, 248, Setting::Range, tx.data(), 8, read, ISOLATED) == 8);
  const auto prior = read.context();
  Reply extended{248, 3, 2, 0, 3, 0, 0};
  const auto crc = calcCrc16(extended.data(), 5);
  extended[5] = lo(crc);
  extended[6] = hi(crc);
  SettingReadback result{};
  ReadResponseError error{};
  CHECK(decodeSettingResponse(extended.data(), extended.size(), read, result, error));
  CHECK(result.model == model && result.address == 248 && result.setting == Setting::Range &&
        result.rawWord == 3 && result.known);
  auto rejectRead = [&](DeviceModel sku, std::uint8_t address, Setting setting,
                        std::uint8_t* buffer, std::size_t capacity, AddressPolicy policy) {
    tx = CANARY;
    CHECK(buildSettingRead(sku, address, setting, buffer, capacity, read, policy) == 0);
    CHECK(tx == CANARY && sameContext(read.context(), prior));
  };
  constexpr auto setting = Setting::Address;
  constexpr SettingValue value{SettingUnit::Address, 5};
  auto rejectDescriptor = [](DeviceModel sku, Setting selector) {
    SettingDescriptor out{SettingUnit::NoiseLevel, SettingMode::Sensor, 3, 11, 6};
    CHECK(!settingDescriptor(sku, selector, out));
    CHECK(out.unit == SettingUnit::NoiseLevel && out.mode == SettingMode::Sensor &&
          out.minWord == 3 && out.maxWord == 11 && out.factoryWord == 6);
    CHECK(settingWordDescription(sku, selector, 1) == nullptr);
  };
  for (auto reg : {0x0100, 0x0101, 0x0102, 0x010a, 0x0202, 0xffff}) {
    const auto invalid = static_cast<Setting>(reg);
    rejectDescriptor(model, invalid);
    checkBuilders(model, invalid, value, false, false);
    rejectRead(model, 1, invalid, tx.data(), 8, STANDARD);
  }
  constexpr DeviceModel badModel{};
  constexpr auto badPolicy = static_cast<AddressPolicy>(255);
  rejectDescriptor(badModel, Setting::Angle);
  checkBuilders(badModel, setting, value, false, false);
  checkBuilders(model, setting, value, false, false, 1, badPolicy);
  rejectRead(badModel, 1, setting, tx.data(), 8, ISOLATED);
  rejectRead(model, 1, setting, tx.data(), 8, badPolicy);
  rejectRead(model, 248, setting, tx.data(), 8, STANDARD);
  rejectRead(model, 255, setting, tx.data(), 8, ISOLATED);
  for (std::size_t capacity = 0; capacity < 8; ++capacity) {
    checkBuilders(model, setting, value, false, false, 1, ISOLATED, capacity);
    rejectRead(model, 1, setting, tx.data(), capacity, STANDARD);
  }
  rejectRead(model, 1, setting, nullptr, 8, STANDARD);
  auto write = PRIOR;
  CHECK(buildWriteSingleRegister(model, 1, setting, value, nullptr, 8, write) == 0);
  CHECK(sameContext(write, PRIOR));
  CHECK(buildVendorBroadcastWrite(model, setting, value, nullptr, 8, ISOLATED) == 0);
}

void checkRejectedReply(DeviceModel model) {
  SettingRequest read;
  Frame tx{};
  CHECK(buildSettingRead(model, 1, Setting::UartUnit, tx.data(), tx.size(), read) == 8);
  SettingReadback out{};
  ReadResponseError error{};
  constexpr Reply last{1, 3, 2, 0, 1, 0x79, 0x84};
  CHECK(decodeSettingResponse(last.data(), last.size(), read, out, error));
  auto reject = [&](const std::uint8_t* data, std::size_t size, const SettingRequest& request,
                    Err code, ReadResponseError reason, std::int32_t detail = 0) {
    const auto status = decodeSettingResponse(data, size, request, out, error);
    CHECK(status.code == code && error == reason && status.detail == detail);
    CHECK(out.model == model && out.address == 1 && out.setting == Setting::UartUnit &&
          out.rawWord == 1 && out.known);
  };
  auto broken = last;
  ++broken[6];
  reject(broken.data(), broken.size(), read, Err::CRC_ERROR, ReadResponseError::CRC_MISMATCH);
  constexpr std::uint8_t exception[] = {1, 0x83, 2, 0xc0, 0xf1};
  reject(exception, sizeof(exception), read, Err::EXCEPTION, ReadResponseError::EXCEPTION_RESPONSE,
         2);
  reject(nullptr, 0, read, Err::INVALID_CONFIG, ReadResponseError::INVALID_ARGUMENT);
  reject(last.data(), last.size(), {}, Err::INVALID_CONFIG, ReadResponseError::INVALID_ARGUMENT);
}

std::string_view labelText(const char* label) {
  return label == nullptr ? "" : label;
}

void checkLabels(DeviceModel model) {
  auto label = [&](Setting setting, std::uint16_t word) {
    return labelText(settingWordDescription(model, setting, word));
  };
  CHECK(label(Setting::SwitchPolarity, 0) == "low below threshold");
  CHECK(label(Setting::SwitchPolarity, 1) == "high below threshold");
  CHECK(label(Setting::UartUnit, 0) == "mm");
  CHECK(label(Setting::UartUnit, 1) == "us");
  CHECK(settingWordDescription(model, Setting::Address, 1) == nullptr);
  CHECK(settingWordDescription(model, Setting::SwitchThreshold, 30) == nullptr);
  constexpr const char* shortAngles[] = {"about 30 deg", "about 40 deg", "about 50 deg",
                                         "about 60 deg"};
  constexpr const char* longAngles[] = {
      "about 40 deg single, 45 deg dual horizontal, 60 deg dual vertical",
      "about 45 deg single, 50 deg dual horizontal, 65 deg dual vertical",
      "about 55 deg single, 55 deg dual horizontal, 70 deg dual vertical",
      "about 65 deg single, 65 deg dual horizontal, 80 deg dual vertical",
  };
  for (unsigned i = 0; i < 4; ++i) {
    CHECK(label(Setting::Angle, i + 1) == (model == SHORT_MODEL ? shortAngles[i] : longAngles[i]));
  }
  constexpr const char* noise[] = {
      "battery supply", "high-frequency supply noise (USB)", "long USB power cable",
      "switching supply",
      "switching supply with environmental interference (routine use discouraged)"};
  constexpr const char* ranges[] = {"about 50 cm", "about 150 cm", "about 250 cm", "about 350 cm",
                                    "about 500 cm"};
  for (unsigned i = 0; i < 5; ++i) {
    CHECK(label(Setting::Noise, i + 1) == noise[i]);
    if (model == LONG_MODEL || i < 4) {
      CHECK(label(Setting::Range, i + 1) == ranges[i]);
    }
  }
}

void checkBaud() {
  constexpr std::uint32_t rates[] = {4800, 9600, 14400, 19200, 38400, 57600, 76800, 115200};
  for (unsigned i = 0; i < 8; ++i) {
    auto code = static_cast<Baud>(0);
    std::uint32_t bps = 0;
    CHECK(valueToBaud(rates[i], code) && static_cast<unsigned>(code) == i + 2);
    CHECK(baudToValue(code, bps) && bps == rates[i]);
    CHECK(labelText(baudToString(code)) == std::to_string(rates[i]));
    for (auto model : {SHORT_MODEL, LONG_MODEL}) {
      CHECK(labelText(settingWordDescription(model, Setting::Baud, i + 2)) ==
            std::to_string(rates[i]));
    }
  }
  for (auto word : {0, 1, 10, 258, 65535}) {
    const auto code = static_cast<Baud>(word);
    std::uint32_t bps = 12345;
    CHECK(!baudToValue(code, bps) && bps == 12345 && baudToString(code) == nullptr);
  }
  auto prior = Baud::Bps76800;
  for (auto invalid : {0, 3, 9601, 115201}) {
    CHECK(!valueToBaud(invalid, prior) && prior == Baud::Bps76800);
  }
}
}  // namespace

int main() {
  for (auto model : {SHORT_MODEL, LONG_MODEL}) {
    for (const auto& test : CASES) {
      checkOperation(model, test);
      checkBounds(model, test);
    }
    checkPolicies(model);
    checkBadArguments(model);
    checkRejectedReply(model);
    checkLabels(model);
  }
  checkBaud();
}
