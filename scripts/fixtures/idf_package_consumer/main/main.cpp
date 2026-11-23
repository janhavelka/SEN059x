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

  constexpr auto model = SEN059x::DeviceModel::SEN0592;
  constexpr auto kind = SEN059x::MeasurementKind::RealTimeDistance;
  SEN059x::MeasurementRequest measurementRequest;
  SEN059x::Measurement reading;
  SEN059x::MeasurementTiming timing;
  if (SEN059x::buildMeasurementRead(model, 1, kind, tx, sizeof(tx), measurementRequest) !=
          sizeof(tx) ||
      !SEN059x::decodeMeasurementResponse(rx, sizeof(rx), measurementRequest, 0, true, reading,
                                          error) ||
      reading.value != 751.0 || !SEN059x::measurementTiming(model, kind, timing) ||
      timing.maxMs != 140) {
    std::abort();
  }

  SEN059x::MeasurementCache cache;
  SEN059x::SampleRequest sampleRequest;
  if (!cache.bind(model, 1) || cache.buildRead(kind, tx, sizeof(tx), sampleRequest) != sizeof(tx) ||
      !cache.complete(sampleRequest, rx, sizeof(rx), 0, true, error)) {
    std::abort();
  }
  const auto latest = cache.snapshot(kind, 10, 10);
  if (latest.usable.measurement.value != 751.0 || latest.usable.ageMs != 10 ||
      latest.usable.freshness != SEN059x::Freshness::Fresh) {
    std::abort();
  }

  SEN059x::SettingRequest settingRequest;
  SEN059x::SettingReadback setting;
  constexpr std::uint8_t baudReply[] = {0x01, 0x03, 0x02, 0x00, 0x03, 0xF8, 0x45};
  std::uint32_t bps = 0;
  if (SEN059x::buildSettingRead(model, 1, SEN059x::Setting::Baud, tx, sizeof(tx), settingRequest) !=
          sizeof(tx) ||
      !SEN059x::decodeSettingResponse(baudReply, sizeof(baudReply), settingRequest, setting,
                                      error) ||
      !setting.known || !SEN059x::baudToValue(static_cast<SEN059x::Baud>(setting.rawWord), bps) ||
      bps != 9600) {
    std::abort();
  }

  SEN059x::RequestContext writeRequest;
  constexpr SEN059x::SettingValue newAddress{SEN059x::SettingUnit::Address, 5};
  constexpr std::uint8_t writeReply[] = {0x01, 0x06, 0x02, 0x00, 0x00, 0x05, 0x48, 0x71};
  auto writeError = SEN059x::WriteResponseError::INVALID_ARGUMENT;
  if (SEN059x::buildWriteSingleRegister(model, 1, SEN059x::Setting::Address, newAddress, tx,
                                        sizeof(tx), writeRequest) != sizeof(tx) ||
      std::memcmp(tx, writeReply, sizeof(tx)) != 0 ||
      !SEN059x::parseWriteSingleRegister(writeReply, sizeof(writeReply), writeRequest,
                                         writeError) ||
      writeError != SEN059x::WriteResponseError::NONE) {
    std::abort();
  }
  if (SEN059x::buildVendorBroadcastWrite(model, SEN059x::Setting::Address, newAddress, tx,
                                         sizeof(tx),
                                         SEN059x::AddressPolicy::DFRobotIsolated) != sizeof(tx) ||
      tx[0] != 0xFF) {
    std::abort();
  }

  SEN059x::Configuration configuration;
  SEN059x::ConfigurationToken writeToken, readToken;
  constexpr SEN059x::Endpoint original{1, 115200};
  constexpr SEN059x::Endpoint intended{5, 115200};
  constexpr std::uint8_t addressReadback[] = {0x05, 0x03, 0x02, 0x00, 0x05, 0x89, 0x87};
  if (!configuration.bind(model, original) ||
      configuration.buildWrite(SEN059x::Setting::Address, newAddress,
                               SEN059x::WriteScope::Addressed, tx, sizeof(tx),
                               writeToken) != sizeof(tx) ||
      !configuration.recordTx(writeToken, SEN059x::TxProgress::Complete, SEN059x::Ok()) ||
      !configuration.recordAck(writeToken, writeReply, sizeof(writeReply),
                               SEN059x::AckOrigin::Device) ||
      configuration.operation().verified ||
      configuration.buildVerification(writeToken, tx, sizeof(tx), readToken) != sizeof(tx) ||
      !configuration.completeRead(readToken, intended, addressReadback, sizeof(addressReadback),
                                  42) ||
      !configuration.operation().verified ||
      !configuration.snapshot(SEN059x::Setting::Address).trusted) {
    std::abort();
  }
}
