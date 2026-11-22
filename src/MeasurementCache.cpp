#include "SEN059x/MeasurementCache.h"

#include <limits>

namespace SEN059x {
namespace {

std::size_t historyIndex(MeasurementKind kind) {
  switch (kind) {
    case MeasurementKind::ProcessedDistance:
      return 0;
    case MeasurementKind::RealTimeDistance:
      return 1;
    case MeasurementKind::Temperature:
      return 2;
    case MeasurementKind::EchoTime:
      return 3;
  }
  return 4;
}

}  // namespace

Status MeasurementCache::bind(DeviceModel model, std::uint8_t address,
                              AddressPolicy addressPolicy) {
  if ((model != DeviceModel::SEN0591 && model != DeviceModel::SEN0592) ||
      !isValidAddress(address, addressPolicy)) {
    return {Err::INVALID_CONFIG, 0, "Invalid measurement binding"};
  }
  if (bindingGeneration_ == std::numeric_limits<std::uint64_t>::max()) {
    return {Err::INVALID_CONFIG, 0, "Binding generation exhausted"};
  }
  model_ = model;
  address_ = address;
  policy_ = addressPolicy;
  ++bindingGeneration_;
  sampleGeneration_ = 0;
  active_ = false;
  histories_ = {};
  return Ok();
}

std::size_t MeasurementCache::buildRead(MeasurementKind kind, std::uint8_t* tx,
                                        std::size_t txCapacity, SampleRequest& out) {
  if (sampleGeneration_ == std::numeric_limits<std::uint64_t>::max()) {
    return 0;
  }
  const auto bytes =
      buildMeasurementRead(model_, address_, kind, tx, txCapacity, out.request_, policy_);
  if (bytes != 0) {
    out.cache_ = this;
    out.bindingGeneration_ = bindingGeneration_;
    out.sampleGeneration_ = ++sampleGeneration_;
    active_ = true;
  }
  return bytes;
}

bool MeasurementCache::discard(const SampleRequest& request) {
  if (!active_ || request.cache_ != this || request.bindingGeneration_ != bindingGeneration_ ||
      request.sampleGeneration_ != sampleGeneration_) {
    return false;
  }
  active_ = false;
  return true;
}

Status MeasurementCache::complete(const SampleRequest& request, const std::uint8_t* rx,
                                  std::size_t rxLength, std::uint32_t completedMs, bool timeValid,
                                  ReadResponseError& error) {
  if (!discard(request)) {
    error = ReadResponseError::INVALID_ARGUMENT;
    return {Err::INVALID_CONFIG, 0, "Measurement request is no longer current for this binding"};
  }
  Measurement observation;
  const auto status = decodeMeasurementResponse(rx, rxLength, request.request_, completedMs,
                                                timeValid, observation, error);
  if (status) {
    // The typed builder admits only the four published measurement kinds.
    auto& history = histories_[historyIndex(observation.kind)];
    history.raw = {observation, request.sampleGeneration_};
    if (observation.quality == MeasurementQuality::Usable) {
      history.usable = history.raw;
    }
  }
  return status;
}

SampleHistory MeasurementCache::describe(const Sample& sample, std::uint32_t nowMs,
                                         std::uint32_t maxAgeMs) {
  SampleHistory result;
  if (sample.generation == 0) {
    return result;
  }
  result.measurement = sample.measurement;
  result.sampleGeneration = sample.generation;
  if (sample.measurement.timeValid) {
    result.ageMs = nowMs - sample.measurement.completedMs;
  }
  if (sample.measurement.quality != MeasurementQuality::Usable) {
    result.freshness = Freshness::Unusable;
  } else if (!result.ageMs) {
    result.freshness = Freshness::UnknownTime;
  } else {
    result.freshness = *result.ageMs <= maxAgeMs ? Freshness::Fresh : Freshness::Stale;
  }
  return result;
}

SampleSnapshot MeasurementCache::snapshot(MeasurementKind kind, std::uint32_t nowMs,
                                          std::uint32_t maxAgeMs) const {
  SampleSnapshot result;
  result.bindingGeneration = bindingGeneration_;
  const auto index = historyIndex(kind);
  if (index < histories_.size()) {
    result.raw = describe(histories_[index].raw, nowMs, maxAgeMs);
    result.usable = describe(histories_[index].usable, nowMs, maxAgeMs);
  }
  return result;
}

}  // namespace SEN059x
