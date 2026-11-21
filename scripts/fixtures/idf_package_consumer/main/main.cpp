#include <cstdlib>
#include <cstring>

#include <SEN059x/SEN059x.h>
#include <SEN059x/Version.h>

static_assert(SEN059x::VERSION[0] != '\0', "Missing SEN059x package version");

extern "C" void app_main() {
  SEN059x::RequestContext request;
  const auto status =
      SEN059x::makeRequestContext(SEN059x::DeviceModel::SEN0592, 1,
                                  SEN059x::FunctionCode::ReadHoldingRegisters, 0x0101, 1, request);
  if (!status || request.startRegister != 0x0101 ||
      std::strcmp(SEN059x::errToString(status.code), "OK") != 0) {
    std::abort();
  }
}
