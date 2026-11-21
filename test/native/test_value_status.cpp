#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <type_traits>

#include <SEN059x/SEN059x.h>
#include <SEN059x/Version.h>

namespace {

using namespace SEN059x;

static_assert(std::is_same_v<std::underlying_type_t<Err>, std::uint16_t>);
static_assert(std::is_same_v<std::underlying_type_t<ReadResponseError>, std::uint8_t>);
static_assert(std::is_same_v<decltype(Status::detail), std::int32_t>);
static_assert(std::is_same_v<decltype(Status::msg), const char*>);
static_assert(!std::is_convertible_v<Status, bool>);
static_assert(std::is_constructible_v<bool, Status>);

// Keep checks active in Release, and report the exact failing expression.
#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

void checkStatus() {
  for (const auto status : {Status{}, Ok()}) {
    CHECK(status.code == Err::OK);
    CHECK(status.detail == 0);
    CHECK(std::strcmp(status.msg, "") == 0);
    CHECK(status.ok());
    CHECK(status.isOk());
    CHECK(static_cast<bool>(status));
  }

  constexpr Status io{Err::IO, -7, "Platform I/O"};
  CHECK(io.code == Err::IO);
  CHECK(io.detail == -7);
  CHECK(std::strcmp(io.msg, "Platform I/O") == 0);
  CHECK(!io.ok());
  CHECK(!io.isOk());
  CHECK(!static_cast<bool>(io));

  const Status exception{Err::EXCEPTION, 0xE1, modbusExceptionToString(0xE1)};
  CHECK(exception.code == Err::EXCEPTION);
  CHECK(exception.detail == 0xE1);
  CHECK(std::strcmp(exception.msg, "Unknown Exception") == 0);
}

void checkDiagnosticText() {
  constexpr struct {
    Err code;
    const char* text;
  } results[] = {{Err::OK, "OK"},
                 {Err::INVALID_CONFIG, "INVALID_CONFIG"},
                 {Err::ILLEGAL_VALUE, "ILLEGAL_VALUE"},
                 {Err::UNSUPPORTED, "UNSUPPORTED"},
                 {Err::CRC_ERROR, "CRC_ERROR"},
                 {Err::FRAME_ERROR, "FRAME_ERROR"},
                 {Err::EXCEPTION, "EXCEPTION"},
                 {Err::NOT_INITIALIZED, "NOT_INITIALIZED"},
                 {Err::BUS_BUSY, "BUS_BUSY"},
                 {Err::TIMEOUT, "TIMEOUT"},
                 {Err::IO, "IO"},
                 {Err::NO_DATA, "NO_DATA"},
                 {Err::DISALLOWED, "DISALLOWED"},
                 {Err::ABORTED, "ABORTED"},
                 {Err::CONFIG_MISMATCH, "CONFIG_MISMATCH"},
                 {Err::LINK_LOST, "LINK_LOST"},
                 {static_cast<Err>(0xFFFF), "UNKNOWN"}};
  for (const auto& result : results) {
    CHECK(std::strcmp(errToString(result.code), result.text) == 0);
  }

  constexpr struct {
    std::uint8_t code;
    const char* text;
  } exceptions[] = {{0x01, "Illegal Function"},     {0x02, "Illegal Data Address"},
                    {0x03, "Illegal Data Value"},   {0x04, "Slave Device Failure"},
                    {0x05, "Acknowledge"},          {0x06, "Slave Device Busy"},
                    {0x07, "Negative Acknowledge"}, {0x08, "Memory Parity Error"},
                    {0x00, "Unknown Exception"},    {0xE1, "Unknown Exception"}};
  for (const auto& exception : exceptions) {
    CHECK(std::strcmp(modbusExceptionToString(exception.code), exception.text) == 0);
  }
}

constexpr auto READ = FunctionCode::ReadHoldingRegisters;
constexpr auto WRITE = FunctionCode::WriteSingleRegister;
constexpr auto STANDARD = AddressPolicy::StandardUnicast;
constexpr auto ISOLATED = AddressPolicy::DFRobotIsolated;

// A prior successful context whose fields must survive every rejection.
constexpr RequestContext previous{DeviceModel::SEN0592, 250, WRITE, 0x0206, 750, ISOLATED};

bool sameContext(const RequestContext& actual, const RequestContext& expected) {
  return actual.model == expected.model && actual.address == expected.address &&
         actual.function == expected.function && actual.startRegister == expected.startRegister &&
         actual.countOrWriteWord == expected.countOrWriteWord &&
         actual.addressPolicy == expected.addressPolicy;
}

void checkAddressBoundaries() {
  constexpr struct {
    std::uint8_t address;
    bool standard;
    bool isolated;
  } cases[] = {{0, false, false},  {1, true, true},    {247, true, true},
               {248, false, true}, {254, false, true}, {255, false, false}};

  for (const auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    for (const auto& test : cases) {
      CHECK(isValidAddress(test.address) == test.standard);
      CHECK(isValidAddress(test.address, STANDARD) == test.standard);
      CHECK(isValidAddress(test.address, ISOLATED) == test.isolated);

      auto out = previous;
      const auto standard = makeRequestContext(model, test.address, READ, 0x0100, 3, out);
      CHECK(standard.code == (test.standard ? Err::OK : Err::INVALID_CONFIG));
      const RequestContext expectedRead{model, test.address, READ, 0x0100, 3, STANDARD};
      CHECK(sameContext(out, test.standard ? expectedRead : previous));

      out = previous;
      const auto isolated =
          makeRequestContext(model, test.address, WRITE, 0x0205, 0, out, ISOLATED);
      CHECK(isolated.code == (test.isolated ? Err::OK : Err::INVALID_CONFIG));
      const RequestContext expectedWrite{model, test.address, WRITE, 0x0205, 0, ISOLATED};
      CHECK(sameContext(out, test.isolated ? expectedWrite : previous));
    }
  }
}

void checkContextArguments() {
  constexpr auto model = DeviceModel::SEN0591;
  constexpr auto invalidPolicy = static_cast<AddressPolicy>(0xFF);
  CHECK(!isValidAddress(1, invalidPolicy));

  constexpr struct {
    DeviceModel model;
    FunctionCode function;
    std::uint16_t startRegister;
    std::uint16_t word;
    AddressPolicy policy;
  } rejected[] = {{RequestContext{}.model, READ, 0x0100, 1, STANDARD},
                  {static_cast<DeviceModel>(0xFF), READ, 0x0100, 1, STANDARD},
                  {model, READ, 0x0100, 1, invalidPolicy},
                  {model, static_cast<FunctionCode>(0x04), 0x0100, 1, STANDARD},
                  {model, READ, 0x0100, 0, STANDARD},
                  {model, READ, 0x0100, 126, STANDARD},
                  {model, READ, 0xFFFF, 2, STANDARD}};
  auto out = previous;
  for (const auto& test : rejected) {
    CHECK(makeRequestContext(test.model, 1, test.function, test.startRegister, test.word, out,
                             test.policy)
              .code == Err::INVALID_CONFIG);
    CHECK(sameContext(out, previous));
  }

  CHECK(makeRequestContext(model, 1, READ, 0xFF83, 125, out).ok());
  CHECK(makeRequestContext(model, 1, READ, 0xFFFF, 1, out).ok());
  CHECK(makeRequestContext(model, 1, WRITE, 0x0206, 0xFFFF, out).ok());
  CHECK(out.countOrWriteWord == 0xFFFF);
}

}  // namespace

int main() {
  CHECK(SEN059x::VERSION[0] != '\0');
  checkStatus();
  checkDiagnosticText();
  checkAddressBoundaries();
  checkContextArguments();
}
