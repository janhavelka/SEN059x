#include "SEN059x/Configuration.h"

#include <algorithm>
#include <limits>

namespace SEN059x {
namespace {

constexpr std::array SETTINGS = {Setting::Address,         Setting::Baud,  Setting::SwitchPolarity,
                                 Setting::SwitchThreshold, Setting::Angle, Setting::UartUnit,
                                 Setting::Noise,           Setting::Range};

std::size_t settingIndex(Setting setting) {
  return static_cast<std::size_t>(std::find(SETTINGS.begin(), SETTINGS.end(), setting) -
                                  SETTINGS.begin());
}

bool sameEndpoint(Endpoint a, Endpoint b) {
  return a.address == b.address && a.bps == b.bps;
}

bool endpointSetting(Setting setting) {
  return setting == Setting::Address || setting == Setting::Baud;
}

constexpr Status INVALID_EVENT{Err::INVALID_CONFIG, 0, "Unexpected commissioning event"};

}  // namespace

Status Configuration::bind(DeviceModel model, Endpoint endpoint, AddressPolicy policy) {
  Baud baud;
  if ((model != DeviceModel::SEN0591 && model != DeviceModel::SEN0592) ||
      !isValidAddress(endpoint.address, policy) || !valueToBaud(endpoint.bps, baud)) {
    return {Err::INVALID_CONFIG, 0, "Invalid configuration binding"};
  }
  model_ = model;
  endpoint_ = endpoint;
  possibleEndpoint_.reset();
  policy_ = policy;
  endpointTrusted_ = false;
  settings_ = {};
  operation_ = {};
  writeId_ = 0;
  readId_ = 0;
  verification_ = false;
  return Ok();
}

ConfigurationSnapshot Configuration::snapshot(Setting setting) const {
  const auto index = settingIndex(setting);
  return index < settings_.size() ? settings_[index] : ConfigurationSnapshot{};
}

bool Configuration::current(const ConfigurationToken& token, std::uint64_t id) const {
  return id != 0 && token.owner_ == this && token.sequence_ == id;
}

std::uint64_t Configuration::issue(ConfigurationToken& token) {
  token.owner_ = this;
  token.sequence_ = ++sequence_;
  return token.sequence_;
}

std::size_t Configuration::buildWrite(Setting setting, SettingValue value, WriteScope scope,
                                      std::uint8_t* tx, std::size_t capacity,
                                      ConfigurationToken& out) {
  if (sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    return 0;
  }
  std::size_t bytes = 0;
  switch (scope) {
    case WriteScope::Addressed:
      bytes = buildWriteSingleRegister(model_, endpoint_.address, setting, value, tx, capacity,
                                       writeContext_, policy_);
      break;
    case WriteScope::VendorSendOnly:
      bytes = buildVendorBroadcastWrite(model_, setting, value, tx, capacity, policy_);
      break;
  }
  if (!bytes) {
    return 0;
  }
  operation_ = {};
  operation_.setting = setting;
  operation_.desired = value;
  operation_.scope = scope;
  operation_.original = operation_.intended = endpoint_;
  operation_.before = snapshot(setting);
  if (setting == Setting::Address) {
    operation_.intended.address = static_cast<std::uint8_t>(value.word);
  } else if (setting == Setting::Baud) {
    // The checked write builder accepts exactly the documented baud codes.
    baudToValue(static_cast<Baud>(value.word), operation_.intended.bps);
  }
  writeId_ = issue(out);
  readId_ = 0;
  txSettled_ = false;
  verification_ = false;
  return bytes;
}

Status Configuration::recordTx(const ConfigurationToken& write, TxProgress progress,
                               Status result) {
  const bool provenUnsent = operation_.tx == TxProgress::Possible && progress == TxProgress::Zero;
  if (!current(write, writeId_) || txSettled_ || operation_.primary ||
      progress > TxProgress::Complete || (progress < operation_.tx && !provenUnsent)) {
    return INVALID_EVENT;
  }
  auto& setting = settings_[settingIndex(operation_.setting)];
  if (operation_.tx == TxProgress::Zero && progress == TxProgress::Possible) {
    settingTrustedBeforePossible_ = setting.trusted;
    endpointTrustedBeforePossible_ = endpointTrusted_;
    candidateBeforePossible_ = possibleEndpoint_;
  }
  if (provenUnsent) {
    setting.trusted = setting.trusted || settingTrustedBeforePossible_;
    if (endpointSetting(operation_.setting) && !endpointTrusted_) {
      endpointTrusted_ = endpointTrustedBeforePossible_;
      possibleEndpoint_ = candidateBeforePossible_;
      if (!isReadEndpoint(readEndpoint_)) {
        readId_ = 0;
      }
    }
  }
  operation_.tx = progress;
  operation_.txStatus = result;
  txSettled_ = progress == TxProgress::Zero || progress == TxProgress::Complete || !result;
  if (progress != TxProgress::Zero) {
    setting.trusted = false;
    if (endpointSetting(operation_.setting)) {
      endpointTrusted_ = false;
      possibleEndpoint_ = operation_.intended;
    }
    readId_ = 0;  // A read prepared before possible TX cannot establish its effect.
  }
  if (progress == TxProgress::Zero ||
      (operation_.scope == WriteScope::VendorSendOnly && txSettled_)) {
    operation_.primary = result;
  }
  return result;
}

bool Configuration::awaitingAck(const ConfigurationToken& write) const {
  return current(write, writeId_) && operation_.scope == WriteScope::Addressed &&
         operation_.tx != TxProgress::Zero && !operation_.ackStatus && !operation_.primary &&
         !verification_;
}

Status Configuration::recordAck(const ConfigurationToken& write, const std::uint8_t* rx,
                                std::size_t length, AckOrigin origin) {
  if (!awaitingAck(write) || origin > AckOrigin::Device) {
    return INVALID_EVENT;
  }
  txSettled_ = true;
  operation_.ackOrigin = origin;
  operation_.ackStatus = parseWriteSingleRegister(rx, length, writeContext_, operation_.ackError);
  return *operation_.ackStatus;
}

Status Configuration::missAck(const ConfigurationToken& write, Status failure) {
  if (!awaitingAck(write) || failure.ok()) {
    return INVALID_EVENT;
  }
  txSettled_ = true;
  operation_.ackStatus = failure;
  return failure;
}

Status Configuration::finishWrite(const ConfigurationToken& write, Status failure) {
  if (!current(write, writeId_) || operation_.primary || failure.ok()) {
    return INVALID_EVENT;
  }
  if (verification_) {
    readId_ = 0;
    verification_ = false;
  }
  operation_.primary = failure;
  return failure;
}

bool Configuration::isReadEndpoint(Endpoint endpoint) const {
  return sameEndpoint(endpoint, endpoint_) ||
         (possibleEndpoint_ && sameEndpoint(endpoint, *possibleEndpoint_));
}

std::size_t Configuration::buildRead(Endpoint endpoint, Setting setting, std::uint8_t* tx,
                                     std::size_t capacity, ConfigurationToken& out) {
  if (!isReadEndpoint(endpoint) || verification_) {
    return 0;
  }
  return prepareRead(endpoint, setting, tx, capacity, out);
}

std::size_t Configuration::prepareRead(Endpoint endpoint, Setting setting, std::uint8_t* tx,
                                       std::size_t capacity, ConfigurationToken& out) {
  if (sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    return 0;
  }
  const auto bytes =
      buildSettingRead(model_, endpoint.address, setting, tx, capacity, readRequest_, policy_);
  if (bytes) {
    readEndpoint_ = endpoint;
    readId_ = issue(out);
    verification_ = false;
  }
  return bytes;
}

std::size_t Configuration::buildVerification(const ConfigurationToken& write, std::uint8_t* tx,
                                             std::size_t capacity, ConfigurationToken& out) {
  if (!current(write, writeId_) || operation_.scope != WriteScope::Addressed ||
      operation_.tx == TxProgress::Zero || operation_.primary || verification_) {
    return 0;
  }
  const auto bytes = prepareRead(operation_.intended, operation_.setting, tx, capacity, out);
  if (bytes) {
    verification_ = true;
    txSettled_ = true;
  }
  return bytes;
}

Status Configuration::finishRead(Status status) {
  readId_ = 0;
  if (verification_) {
    verification_ = false;
    operation_.readStatus = status;
    if (status.ok()) {
      operation_.verified = operation_.readback->value.rawWord == operation_.desired.word;
      if (!operation_.verified) {
        status = {Err::CONFIG_MISMATCH, operation_.readback->value.rawWord,
                  "Readback differs from requested word"};
      }
    } else if (endpointSetting(operation_.setting) && status.code != Err::EXCEPTION) {
      status = {Err::LINK_LOST, status.detail, "Post-write endpoint unconfirmed"};
    }
    operation_.primary = status;
  }
  return status;
}

Status Configuration::completeRead(const ConfigurationToken& read, Endpoint actualEndpoint,
                                   const std::uint8_t* rx, std::size_t length,
                                   std::uint32_t completedMs) {
  if (!current(read, readId_) || !sameEndpoint(actualEndpoint, readEndpoint_)) {
    return INVALID_EVENT;
  }
  SettingReadback value;
  ReadResponseError error;
  const auto status = decodeSettingResponse(rx, length, readRequest_, value, error);
  if (verification_) {
    operation_.readError = error;
  }
  if (status || error == ReadResponseError::EXCEPTION_RESPONSE) {
    if (writeId_ && !operation_.primary && operation_.tx == TxProgress::Zero &&
        !sameEndpoint(actualEndpoint, operation_.original)) {
      operation_.primary = {Err::ABORTED, 0, "Endpoint changed before write transmission"};
    }
    endpoint_ = actualEndpoint;
    possibleEndpoint_.reset();
    endpointTrusted_ = true;
  }
  if (status) {
    const ConfigurationObservation observed{value, actualEndpoint, completedMs};
    settings_[settingIndex(value.setting)] = {observed, true};
    if (verification_) {
      operation_.readback = observed;
    }
  }
  return finishRead(status);
}

Status Configuration::failRead(const ConfigurationToken& read, Status failure) {
  if (!current(read, readId_) || failure.ok()) {
    return INVALID_EVENT;
  }
  return finishRead(failure);
}

Status Configuration::recordCleanup(const ConfigurationToken& write, Status result) {
  if (!current(write, writeId_) || operation_.cleanup) {
    return INVALID_EVENT;
  }
  operation_.cleanup = result;
  return result;
}

void Configuration::interrupt() {
  for (auto& setting : settings_) {
    setting.trusted = false;
  }
  endpointTrusted_ = false;
  const Status aborted{Err::ABORTED, 0, "Power interruption"};
  if (writeId_ != 0 && !operation_.primary) {
    operation_.interrupted = true;
  }
  if (verification_ && readId_ != 0) {
    finishRead(aborted);
  } else if (writeId_ != 0 && !operation_.primary) {
    operation_.primary = aborted;
    if (operation_.scope == WriteScope::Addressed && operation_.tx != TxProgress::Zero &&
        endpointSetting(operation_.setting)) {
      operation_.primary = {Err::LINK_LOST, 0, "Power interruption after possible endpoint change"};
    }
  }
  readId_ = 0;
}

}  // namespace SEN059x
