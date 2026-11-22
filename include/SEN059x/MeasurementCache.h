/** @file MeasurementCache.h
 *  @brief Per-binding publication and passive freshness of typed measurements.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Measurements.h"

namespace SEN059x {

class MeasurementCache;

/// Retain this identity by value from buildRead through completion or discard.
/// Copies identify the same request. Use them only within the originating cache's lifetime.
class SampleRequest {
 public:
  /// Borrow the typed request's wire identity for the lifetime of this object.
  const RequestContext& context() const { return request_.context(); }

 private:
  friend class MeasurementCache;
  MeasurementRequest request_;
  const MeasurementCache* cache_ = nullptr;
  std::uint64_t bindingGeneration_ = 0;
  std::uint64_t sampleGeneration_ = 0;
};

/// State of one retained observation at the owner's query time.
enum class Freshness : std::uint8_t {
  NeverSampled,  ///< This history is empty.
  UnknownTime,   ///< Usable observation with an unknown completion time.
  Fresh,         ///< Usable observation whose age in ms is within the inclusive limit.
  Stale,         ///< Usable observation older than the limit.
  Unusable,      ///< Attributable raw observation whose word has no usable interpretation.
};

/// One observation and its age at the supplied query time.
struct SampleHistory {
  Measurement measurement;  ///< Copied observation, meaningful when sampleGeneration > 0.
  std::uint64_t sampleGeneration = 0;  ///< Request sequence within this binding, or zero if empty.
  Freshness freshness = Freshness::NeverSampled;  ///< Quality and freshness at query time.
  std::optional<std::uint32_t> ageMs;  ///< Elapsed ms for any observation with a known timestamp.
};

/// Independent copies of both histories for one kind in one binding generation.
struct SampleSnapshot {
  std::uint64_t bindingGeneration = 0;  ///< Increments on every successful bind, zero if unbound.
  SampleHistory raw;                    ///< Latest attributable word, including unusable words.
  SampleHistory usable;                 ///< Latest observation with an engineering value.
};

/// Fixed storage for one explicit binding. The owner serializes access, supplies time,
/// associates RX with its one outstanding bus exchange and settles late wire traffic.
class MeasurementCache {
 public:
  /// Start unbound with empty histories. Tokens refer to this object's stable address.
  MeasurementCache() = default;
  MeasurementCache(const MeasurementCache&) = delete;
  MeasurementCache& operator=(const MeasurementCache&) = delete;

  /**
   * @brief Start a new binding generation and clear its request and all four histories.
   * Call also when external binding identity changes (e.g. a host port or baud change).
   * Binding is local and emits zero transfers, even when selecting another SKU.
   * @param model Explicit selection of SEN0591 or SEN0592.
   * @param address Destination byte under addressPolicy.
   * @param addressPolicy Use DFRobotIsolated only on an isolated link.
   * @return OK even when rebinding to the same endpoint, or INVALID_CONFIG for an invalid
   * binding or exhausted generation counter. Rejection preserves the current state.
   */
  Status bind(DeviceModel model, std::uint8_t address,
              AddressPolicy addressPolicy = AddressPolicy::StandardUnicast);

  /**
   * @brief Prepare an owner-admitted typed read, superseding the previous request on success.
   * @param kind Published measurement register.
   * @param tx Writable buffer disjoint from out, with capacity for eight bytes.
   * @param txCapacity Size of tx in bytes.
   * @param out Token to retain with the exchange. A copy may be used to complete it once.
   * @return Eight on success. Zero for unbound state, exhausted request sequence, invalid
   * kind or insufficient storage, preserving tx, out and the prior request.
   */
  std::size_t buildRead(MeasurementKind kind, std::uint8_t* tx, std::size_t txCapacity,
                        SampleRequest& out);

  /**
   * @brief Retire the current exchange and decode its complete reply into the selected history.
   * @param request Identity retained for this completion.
   * @param rx Buffer readable for rxLength bytes, disjoint from request and error.
   * @param rxLength Actual frame size in bytes.
   * @param completedMs Completion time in ms on the owner's monotonic clock.
   * @param timeValid Whether completedMs is known, including at zero.
   * @param error Structured FC03 result, or INVALID_ARGUMENT for a noncurrent request.
   * @return INVALID_CONFIG for a noncurrent identity, preserving the active request. For a
   * current request, propagate the decoder status including exception detail. Only OK
   * updates raw history, and only usable quality updates usable history.
   */
  Status complete(const SampleRequest& request, const std::uint8_t* rx, std::size_t rxLength,
                  std::uint32_t completedMs, bool timeValid, ReadResponseError& error);

  /**
   * @brief Retire a request after an owner timeout, cancellation or transport failure.
   * @param request Retained identity. Both observation histories are preserved.
   * @return True if this request was current and has been retired.
   */
  bool discard(const SampleRequest& request);

  /**
   * @brief Copy raw and usable histories without initiating any transfer or changing state.
   * @param kind Published register selector. An unknown selector yields empty histories.
   * @param nowMs Query time on the same monotonic millisecond clock as completedMs.
   * Actual elapsed time must be less than one full 32-bit cycle (about 49.7 days).
   * @param maxAgeMs Inclusive maximum age in ms. At zero only same-tick observations are fresh.
   * @return Both histories with independently calculated ages, empty before acquisition.
   */
  SampleSnapshot snapshot(MeasurementKind kind, std::uint32_t nowMs, std::uint32_t maxAgeMs) const;

 private:
  struct Sample {
    Measurement measurement;
    std::uint64_t generation = 0;
  };
  struct History {
    Sample raw;
    Sample usable;
  };
  static SampleHistory describe(const Sample& sample, std::uint32_t nowMs, std::uint32_t maxAgeMs);

  std::array<History, 4> histories_{};
  DeviceModel model_{};
  std::uint8_t address_ = 0;
  AddressPolicy policy_ = AddressPolicy::StandardUnicast;
  std::uint64_t bindingGeneration_ = 0;
  std::uint64_t sampleGeneration_ = 0;
  bool active_ = false;
};

}  // namespace SEN059x
