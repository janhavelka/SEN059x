// clang-format off: Include the public header first to check self-containment.
#include <SEN059x/MeasurementCache.h>
// clang-format on

#include <array>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include <SEN059x/ReadCodec.h>

namespace {

using namespace SEN059x;
using Kind = MeasurementKind;
using Reply = std::array<std::uint8_t, 7>;
constexpr std::array<Kind, 4> KINDS{Kind::ProcessedDistance, Kind::RealTimeDistance,
                                    Kind::Temperature, Kind::EchoTime};
using Snapshots = std::array<SampleSnapshot, KINDS.size()>;

// Without a cache-issued identity, typed and raw FC03 requests cannot publish.
static_assert(!std::is_aggregate_v<SampleRequest>);
static_assert(!std::is_constructible_v<SampleRequest, RequestContext>);
static_assert(!std::is_constructible_v<SampleRequest, MeasurementRequest>);
static_assert(!std::is_invocable_v<decltype(&MeasurementCache::complete), MeasurementCache&,
                                   const RequestContext&, const std::uint8_t*, std::size_t,
                                   std::uint32_t, bool, ReadResponseError&>);

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      std::exit(EXIT_FAILURE);                                             \
    }                                                                      \
  } while (false)

// DFRobot examples 1 and 3. The same single-word response can serve distinct requests.
constexpr Reply DISTANCE{1, 3, 2, 2, 0xF2, 0x38, 0xA1};
constexpr Reply WORD_300{1, 3, 2, 1, 0x2C, 0xB8, 0x09};

SampleRequest build(MeasurementCache& cache, Kind kind) {
  SampleRequest request;
  std::array<std::uint8_t, 8> tx{};
  CHECK(cache.buildRead(kind, tx.data(), tx.size(), request) == tx.size());
  return request;
}

void publish(MeasurementCache& cache, const SampleRequest& request, const Reply& rx,
             std::uint32_t ms, bool known = true) {
  auto error = ReadResponseError::INVALID_ARGUMENT;
  CHECK(cache.complete(request, rx.data(), rx.size(), ms, known, error));
  CHECK(error == ReadResponseError::NONE);
}

void reject(MeasurementCache& cache, const SampleRequest& stale) {
  auto error = ReadResponseError::NONE;
  const auto status = cache.complete(stale, DISTANCE.data(), DISTANCE.size(), 99, true, error);
  CHECK(status.code == Err::INVALID_CONFIG && error == ReadResponseError::INVALID_ARGUMENT);
  CHECK(!cache.discard(stale));
}

Snapshots snapshotAll(const MeasurementCache& cache) {
  Snapshots result;
  for (std::size_t i = 0; i < KINDS.size(); ++i) {
    result[i] = cache.snapshot(KINDS[i], 100, 50);
  }
  return result;
}

void checkHistory(const SampleHistory& current, const SampleHistory& prior) {
  CHECK(current.sampleGeneration == prior.sampleGeneration);
  CHECK(current.ageMs == prior.ageMs && current.freshness == prior.freshness);
  const auto& a = current.measurement;
  const auto& b = prior.measurement;
  CHECK(a.model == b.model && a.address == b.address && a.kind == b.kind);
  CHECK(a.rawWord == b.rawWord && a.quality == b.quality && a.unit == b.unit && a.value == b.value);
  CHECK(a.completedMs == b.completedMs && a.timeValid == b.timeValid);
}

void checkUnchanged(const MeasurementCache& cache, const Snapshots& before) {
  const auto after = snapshotAll(cache);
  for (std::size_t i = 0; i < KINDS.size(); ++i) {
    CHECK(after[i].bindingGeneration == before[i].bindingGeneration);
    checkHistory(after[i].raw, before[i].raw);
    checkHistory(after[i].usable, before[i].usable);
  }
}

void seed(MeasurementCache& cache, std::uint32_t startMs) {
  for (const auto kind : KINDS) {
    publish(cache, build(cache, kind), WORD_300, startMs++, kind != Kind::EchoTime);
  }
  // 754 is above the interpreted temperature range. Keep the earlier usable 30.0 C.
  publish(cache, build(cache, Kind::Temperature), DISTANCE, startMs);
  const auto temperature = cache.snapshot(Kind::Temperature, startMs, 50);
  CHECK(temperature.raw.measurement.rawWord == 754 && temperature.usable.measurement.value == 30.0);
  CHECK(temperature.raw.sampleGeneration != temperature.usable.sampleGeneration);
}

void checkCleared(const MeasurementCache& cache, std::uint64_t priorGeneration) {
  for (const auto& sample : snapshotAll(cache)) {
    CHECK(sample.bindingGeneration != 0 && sample.bindingGeneration != priorGeneration);
    for (const auto* history : {&sample.raw, &sample.usable}) {
      CHECK(history->sampleGeneration == 0 && history->freshness == Freshness::NeverSampled);
      CHECK(!history->ageMs && !history->measurement.timeValid && !history->measurement.value);
    }
  }
}

void checkReboundCompletion() {
  MeasurementCache target, peer;
  CHECK(target.bind(DeviceModel::SEN0591, 1) && peer.bind(DeviceModel::SEN0592, 1));
  seed(target, 10);
  seed(peer, 25);
  const auto peerState = snapshotAll(peer);
  const auto old = build(target, Kind::ProcessedDistance);
  const auto first = snapshotAll(target);
  CHECK(target.bind(DeviceModel::SEN0592, 2));
  checkCleared(target, first[0].bindingGeneration);
  const auto away = snapshotAll(target);
  CHECK(target.bind(DeviceModel::SEN0591, 1));
  checkCleared(target, away[0].bindingGeneration);
  seed(target, 60);
  const auto current = build(target, Kind::ProcessedDistance);
  const auto before = snapshotAll(target);
  CHECK(before[0].bindingGeneration != first[0].bindingGeneration);
  reject(target, old);
  checkUnchanged(target, before);
  checkUnchanged(peer, peerState);
  publish(target, current, DISTANCE, 98);
  CHECK(target.snapshot(Kind::ProcessedDistance, 100, 50).usable.measurement.value == 754);

  // The owner may change the bus port while keeping the same SKU, address and policy.
  const auto pending = build(target, Kind::EchoTime);
  CHECK(target.bind(DeviceModel::SEN0591, 1));
  checkCleared(target, before[0].bindingGeneration);
  reject(target, pending);
  checkUnchanged(peer, peerState);
}

void checkCurrentRequest() {
  MeasurementCache cache, other;
  CHECK(cache.bind(DeviceModel::SEN0591, 1) && other.bind(DeviceModel::SEN0591, 1));
  const auto a = build(cache, Kind::ProcessedDistance);
  const auto b = build(other, Kind::ProcessedDistance);
  reject(cache, b);  // Same model, address, kind and initial generations on another instance.
  publish(cache, a, WORD_300, 0);
  publish(other, b, DISTANCE, 1);
  const auto initial = snapshotAll(cache);
  reject(cache, a);  // Already consumed.

  const auto old = build(cache, Kind::Temperature);
  auto current = build(cache, Kind::Temperature);
  reject(cache, old);
  CHECK(cache.discard(current));
  reject(cache, current);
  current = build(cache, Kind::Temperature);
  auto error = ReadResponseError::NONE;
  const auto failed = cache.complete(current, WORD_300.data(), 4, 3, true, error);
  CHECK(failed.code == Err::FRAME_ERROR && error == ReadResponseError::FRAME_TOO_SHORT);
  reject(cache, current);
  checkUnchanged(cache, initial);

  current = build(cache, Kind::Temperature);
  CHECK(!cache.bind(DeviceModel::SEN0591, 248));
  std::array<std::uint8_t, 7> shortTx{};
  const auto saved = shortTx;
  CHECK(cache.buildRead(Kind::EchoTime, shortTx.data(), shortTx.size(), current) == 0);
  CHECK(shortTx == saved);
  checkUnchanged(cache, initial);
  publish(cache, current, WORD_300, 4);
  CHECK(cache.snapshot(Kind::Temperature, 4, 0).raw.measurement.value == 30.0);
}

void checkBindingPolicy() {
  MeasurementCache cache;
  SampleRequest request;
  std::array<std::uint8_t, 8> tx{};
  CHECK(cache.buildRead(Kind::Temperature, tx.data(), tx.size(), request) == 0);
  CHECK(cache.snapshot(Kind::Temperature, 0, 0).bindingGeneration == 0);
  reject(cache, request);
  CHECK(!cache.bind(DeviceModel::SEN0591, 248));
  CHECK(!cache.bind(DeviceModel{}, 1));
  CHECK(cache.bind(DeviceModel::SEN0591, 248, AddressPolicy::DFRobotIsolated));
  CHECK(cache.buildRead(Kind::Temperature, tx.data(), tx.size(), request) == tx.size());
  CHECK(tx[0] == 248 && request.context().addressPolicy == AddressPolicy::DFRobotIsolated);
  CHECK(cache.discard(request));
}

void checkRawReads() {
  MeasurementCache cache;
  CHECK(cache.bind(DeviceModel::SEN0591, 1));
  seed(cache, 30);
  const auto before = snapshotAll(cache);
  // Raw measurement diagnostic, synthetic contiguous diagnostic and manufacturer baud readback.
  auto read = [&](std::uint16_t reg, std::uint16_t count, const auto& rx, auto expected) {
    RequestContext request;
    std::array<std::uint8_t, 8> tx{};
    CHECK(buildReadRegisters(DeviceModel::SEN0591, 1, reg, count, tx.data(), tx.size(), request) ==
          tx.size());
    decltype(expected) words{};
    auto error = ReadResponseError::INVALID_ARGUMENT;
    CHECK(validateReadResponseExpected(rx.data(), rx.size(), request, words.data(), words.size(),
                                       error));
    CHECK(error == ReadResponseError::NONE && words == expected);
    checkUnchanged(cache, before);
  };
  read(0x0100, 1, DISTANCE, std::array<std::uint16_t, 1>{754});
  constexpr std::array<std::uint8_t, 11> block{1, 3, 6, 2, 0xF2, 2, 0xEF, 1, 0x2C, 0x29, 0x42};
  read(0x0100, 3, block, std::array<std::uint16_t, 3>{754, 751, 300});
  read(0x0201, 1, Reply{1, 3, 2, 0, 3, 0xF8, 0x45}, std::array<std::uint16_t, 1>{3});
}

}  // namespace

int main() {
  checkReboundCompletion();
  checkCurrentRequest();
  checkBindingPolicy();
  checkRawReads();
}
