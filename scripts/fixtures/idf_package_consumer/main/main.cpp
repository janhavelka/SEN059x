#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <SEN059x/SEN059x.h>
#include <SEN059x/Version.h>

static_assert(SEN059x::VERSION[0] != '\0', "Missing SEN059x package version");

extern "C" void app_main() {
  SEN059x::RequestContext request;
  std::uint8_t tx[8]{};
  constexpr std::uint8_t expectedTx[] = {0x01, 0x03, 0x01, 0x01, 0x00, 0x01, 0xD4, 0x36};
  if (SEN059x::buildReadRegisters(SEN059x::DeviceModel::SEN0592, 1, 0x0101, 1, tx, sizeof(tx),
                                  request) != sizeof(tx) ||
      std::memcmp(tx, expectedTx, sizeof(tx)) != 0) {
    std::abort();
  }

  constexpr std::uint8_t rx[] = {0x01, 0x03, 0x02, 0x02, 0xEF, 0xF8, 0xA8};
  std::uint16_t word = 0;
  SEN059x::ReadResponseError error = SEN059x::ReadResponseError::INVALID_ARGUMENT;
  const auto status =
      SEN059x::validateReadResponseExpected(rx, sizeof(rx), request, &word, 1, error);
  if (!status || error != SEN059x::ReadResponseError::NONE || word != 0x02EF ||
      std::strcmp(SEN059x::errToString(status.code), "OK") != 0) {
    std::abort();
  }

  SEN059x::RequestContext writeRequest;
  if (!SEN059x::makeRequestContext(SEN059x::DeviceModel::SEN0592, 1,
                                   SEN059x::FunctionCode::WriteSingleRegister, 0x0200, 5,
                                   writeRequest)) {
    std::abort();
  }
  constexpr std::uint8_t writeReply[] = {0x01, 0x06, 0x02, 0x00, 0x00, 0x05, 0x48, 0x71};
  auto writeError = SEN059x::WriteResponseError::INVALID_ARGUMENT;
  if (!SEN059x::parseWriteSingleRegister(writeReply, sizeof(writeReply), writeRequest,
                                         writeError) ||
      writeError != SEN059x::WriteResponseError::NONE) {
    std::abort();
  }
}
