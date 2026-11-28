#include "HumanConsole.h"

namespace SEN059x::example {
namespace {

void modelField(JsonWriter& out, const TargetSelection& target) {
  out.key("model");
  if (target.model) {
    out.str(*target.model == DeviceModel::SEN0591 ? "SEN0591" : "SEN0592");
  } else {
    out.null();
  }
}

void endpointField(JsonWriter& out, std::string_view name, std::optional<Endpoint> at) {
  out.key(name);
  if (!at) {
    out.null();
    return;
  }
  out.object();
  out.integer("address", at->address);
  out.integer("baud", at->bps);
  out.endObject();
}

template <typename T>
void nullableInteger(JsonWriter& out, std::string_view name, bool known, T value) {
  out.key(name);
  if (known) {
    out.num(value);
  } else {
    out.null();
  }
}

std::string_view kindName(MeasurementKind kind) {
  for (const auto& entry : SAMPLE_KINDS) {
    if (entry.kind == kind) {
      return entry.name;
    }
  }
  return "unknown";
}

const char* qualityName(MeasurementQuality quality) {
  switch (quality) {
    case MeasurementQuality::Unobserved:
      return "Unobserved";
    case MeasurementQuality::Usable:
      return "Usable";
    case MeasurementQuality::OutOfProfile:
      return "OutOfProfile";
    case MeasurementQuality::UnexpectedQuantization:
      return "UnexpectedQuantization";
    case MeasurementQuality::AmbiguousTemperature:
      return "AmbiguousTemperature";
  }
  return "unknown";
}

void observation(JsonWriter& out, MeasurementKind kind, const SampleHistory& history,
                 std::uint64_t generation, const TargetSelection& target) {
  if (!history.sampleGeneration) {
    out.null();
    return;
  }
  const auto& reading = history.measurement;
  out.object();
  modelField(out, target);
  out.integer("address", reading.address);
  out.integer("bindingGeneration", generation);
  out.value("kind", kindName(kind));
  out.integer("register", static_cast<std::uint16_t>(kind));
  out.integer("raw", reading.rawWord);
  out.key("value");
  if (!reading.value) {
    out.null();
  } else if (reading.unit == MeasurementUnit::DegreesCelsius) {
    out.tenths(reading.rawWord);
  } else {
    out.num(static_cast<unsigned>(*reading.value));
  }
  const char* unit = "mm";
  if (reading.unit == MeasurementUnit::DegreesCelsius) {
    unit = "degC";
  } else if (reading.unit == MeasurementUnit::Microseconds) {
    unit = "us";
  }
  out.value("unit", unit);
  out.value("quality", qualityName(reading.quality));
  out.integer("sampleGeneration", history.sampleGeneration);
  nullableInteger(out, "observationMs", reading.timeValid, reading.completedMs);
  nullableInteger(out, "ageMs", history.ageMs.has_value(), history.ageMs.value_or(0));
  out.flag("valid", reading.value.has_value());
  out.flag("fresh", history.freshness == Freshness::Fresh);
  out.endObject();
}

const char* disposition(Status status, bool usable) {
  switch (status.code) {
    case Err::OK:
      return usable ? "Ok" : "Failed";
    case Err::TIMEOUT:
      return "Timeout";
    case Err::ABORTED:
      return "Cancelled";
    default:
      return "Failed";
  }
}

// Stage details travel inside the original terminal, which can itself be nested in result.
// The envelope already carries the terminal message. Keep a bounded distinct stage message.
void stageStatus(JsonWriter& out, std::string_view key, Status stage, Status terminal) {
  out.key(key);
  out.object();
  out.value("code", errToString(stage.code));
  out.integer("detail", stage.detail);
  if (stage.msg && stage.msg[0] && stage.msg != terminal.msg) {
    std::size_t length = 0;
    while (length < 32 && stage.msg[length]) {
      ++length;
    }
    out.value("message", {stage.msg, length});
  }
  out.endObject();
}

std::string_view settingKey(Setting setting) {
  for (const auto& entry : SETTING_KEYS) {
    if (entry.setting == setting) {
      return entry.key;
    }
  }
  return "unknown";
}

void settingIdentity(JsonWriter& out, Setting setting) {
  out.value("key", settingKey(setting));
  out.integer("register", static_cast<std::uint16_t>(setting));
  if (setting == Setting::SwitchPolarity || setting == Setting::SwitchThreshold) {
    out.value("applicability", "switch-mode");
  } else if (setting == Setting::UartUnit) {
    out.value("applicability", "uart-output");
  } else {
    out.value("applicability", "rs485");
  }
}

void settingEvidence(JsonWriter& out, const ConfigurationSnapshot& snapshot) {
  const auto& observed = snapshot.observed;
  nullableInteger(out, "raw", observed.has_value(), observed ? observed->value.rawWord : 0);
  out.key("value");
  if (!observed || !observed->value.known) {
    out.null();
  } else {
    const auto& v = observed->value;
    if (v.setting == Setting::Baud) {
      std::uint32_t baud = 0;
      baudToValue(static_cast<Baud>(v.rawWord), baud);
      out.num(baud);
    } else if (const auto* meaning = settingWordDescription(v.model, v.setting, v.rawWord)) {
      out.str(meaning);
    } else {
      out.num(v.rawWord);
    }
  }
  out.flag("valid", observed.has_value());
  out.flag("trusted", snapshot.trusted);
  endpointField(out, "endpoint", observed ? std::optional{observed->endpoint} : std::nullopt);
  nullableInteger(out, "observationMs", observed.has_value(), observed ? observed->completedMs : 0);
}

void writePlan(JsonWriter& out, const ConfigurationOperation& write) {
  const bool addressed = write.scope == WriteScope::Addressed;
  settingIdentity(out, write.setting);
  out.integer("requested", write.desired.word);
  out.integer("destination", addressed ? write.original.address : 255);
  out.value("scope", addressed ? "addressed" : "send-only");
  endpointField(out, "oldEndpoint", write.original);
  endpointField(out, "newEndpoint", write.intended);
}

const char* originName(AckOrigin origin) {
  switch (origin) {
    case AckOrigin::LocalEcho:
      return "local-echo";
    case AckOrigin::Device:
      return "device";
    default:
      return "unknown";
  }
}

const char* ownerPhase(OwnerPhase phase) {
  switch (phase) {
    case OwnerPhase::Idle:
      return "idle";
    case OwnerPhase::Queued:
      return "queued";
    case OwnerPhase::Active:
      return "active";
    case OwnerPhase::Recovery:
      return "recovery";
  }
  return "unknown";
}
}  // namespace

void HumanConsole::sampleData(MeasurementKind kind, const SampleSnapshot& history,
                              const TargetSelection& target) {
  observation(json_, kind, history.raw, history.bindingGeneration, target);
}

void HumanConsole::previewData(const ConfigurationOperation& plan) {
  const auto target = owner_.localTarget(0);
  modelField(json_, target);
  json_.value("addressPolicy",
              target.policy == AddressPolicy::StandardUnicast ? "standard" : "dfrobot-isolated");
  json_.flag("preview", true);
  writePlan(json_, plan);
  json_.value("verification", plan.scope == WriteScope::Addressed ? "addressed-readback"
                                                                  : "separate-addressed-read");
  json_.integer("transfers", 0);
}

void HumanConsole::jobData(const Job& job, bool terminal) {
  modelField(json_, job.target);
  json_.integer("address", job.target.address);
  json_.integer("baud", job.target.baud);
  json_.integer("bindingGeneration", job.generation);
  json_.integer("timeoutMs", (job.budgetUs + 999) / 1000);
  // Writes and settings include the register with their named setting below.
  if (job.mode == JobMode::Raw) {
    json_.integer("startRegister", job.reg);
    json_.integer("count", job.words);
  } else if (job.mode == JobMode::Measurement || job.mode == JobMode::Watch) {
    json_.integer("register", job.reg);
    json_.value("kind", kindName(static_cast<MeasurementKind>(job.reg)));
  } else if (job.mode == JobMode::Write) {
    writePlan(json_, *job.last.write);
  } else {
    settingIdentity(json_, static_cast<Setting>(job.reg));
  }
  if (job.mode == JobMode::Watch) {
    json_.integer("requested", job.requested);
    json_.integer("intervalMs", job.intervalUs / 1000);
  }
  if (!terminal) {
    return;
  }
  const auto& result = job.last;
  json_.value("disposition", disposition(job.outcome, job.unusable == 0 && result.cleanup.ok()));
  json_.integer("elapsedMs", (job.endedUs - job.startedUs) / 1000);
  json_.integer("transfers", job.transfers);
  json_.integer("attempts", job.transfers);
  json_.flag("settlementRequired", job.settlementRequired);
  json_.value("cleanupCode", errToString(result.cleanup.code));
  json_.integer("cleanupDetail", result.cleanup.detail);
  const auto& wire = result.exchange;
  json_.integer("txAccepted", wire.txAccepted);
  json_.integer("rxTotal", wire.rxTotal);
  switch (job.mode) {
    case JobMode::Write: {
      const auto& write = *result.write;
      nullableInteger(json_, "observed", write.readback.has_value(),
                      write.readback ? write.readback->value.rawWord : 0);
      json_.flag("acknowledged",
                 write.ackStatus && write.ackStatus->ok() && write.ackOrigin == AckOrigin::Device);
      json_.value("ackOrigin", originName(write.ackOrigin));
      json_.flag("verified", write.verified);
      json_.flag("effectUncertain", write.tx != TxProgress::Zero && !write.readback);
      json_.flag("endpointTrusted", result.writeEndpointTrusted);
      const char* progress = "zero";
      switch (write.tx) {
        case TxProgress::Complete:
          progress = "complete";
          break;
        case TxProgress::Partial:
          progress = "partial";
          break;
        case TxProgress::Possible:
          progress = "possible";
          break;
        case TxProgress::Zero:
          break;
      }
      json_.value("writeTx", progress);
      if (write.primary) {
        stageStatus(json_, "primary", *write.primary, job.outcome);
      }
      if (write.readStatus) {
        stageStatus(json_, "readback", *write.readStatus, job.outcome);
      }
      if (write.ackStatus) {
        json_.value("ackCode", errToString(write.ackStatus->code));
        json_.integer("ackDetail", write.ackStatus->detail);
      }
      if (write.txStatus) {
        json_.value("txCode", errToString(write.txStatus->code));
        json_.integer("txDetail", write.txStatus->detail);
      }
      if (write.scope == WriteScope::VendorSendOnly) {
        json_.value("rxOrigin", originName(wire.origin));
        json_.key("rxBytes");
        json_.array();
        for (std::size_t i = 0; i < wire.rxAccessible; ++i) {
          json_.num(wire.vendorRx[i]);
        }
        json_.endArray();
      }
      break;
    }
    case JobMode::Raw:
      json_.key("words");
      if (!job.outcome) {
        json_.null();
      } else {
        json_.array();
        for (std::size_t i = 0; i < wire.wordCount; ++i) {
          json_.num(wire.words[i]);
        }
        json_.endArray();
      }
      break;
    case JobMode::Diagnose:
    case JobMode::Setting: {
      const auto& history = result.settingHistory;
      settingEvidence(json_,
                      {result.settingObservation, result.settingObservation && history.trusted});
      if (!result.settingObservation && history.observed) {
        json_.key("previousSetting");
        json_.object();
        settingEvidence(json_, history);
        json_.endObject();
      }
      if (job.mode == JobMode::Diagnose) {
        json_.integer("expected", job.target.address);
        nullableInteger(json_, "observed", result.settingObservation.has_value(),
                        result.settingObservation ? result.settingObservation->value.rawWord : 0);
      }
      break;
    }
    case JobMode::Watch:
      json_.integer("completed", job.completed);
      json_.integer("failed", job.failed);
      json_.integer("unusable", job.unusable);
      json_.integer("skippedIntervals", job.skipped);
      if (result.ticket) {
        stageStatus(json_, "lastResult", result.status, job.outcome);
      } else {
        json_.key("lastResult");
        json_.null();
      }
      [[fallthrough]];
    case JobMode::Measurement: {
      const auto kind = static_cast<MeasurementKind>(job.reg);
      const auto& history = result.sampleHistory;
      const bool hasReading = result.measurement.quality != MeasurementQuality::Unobserved;
      json_.key("sample");
      if (hasReading) {
        sampleData(kind, history, job.target);
      } else {
        json_.null();
      }
      if (!hasReading || (history.usable.sampleGeneration &&
                          history.raw.sampleGeneration != history.usable.sampleGeneration)) {
        json_.key("previousSample");
        const auto& previous = history.usable.sampleGeneration ? history.usable : history.raw;
        observation(json_, kind, previous, history.bindingGeneration, job.target);
      }
      break;
    }
  }
}

void HumanConsole::cacheData(std::string_view cmd, std::string_view arg) {
  const auto target = owner_.localTarget(0);
  if (cmd == "latest") {
    const auto now = owner_.nowMs();
    if (arg.empty()) {
      json_.key("samples");
      json_.object();
    }
    for (const auto& entry : SAMPLE_KINDS) {
      if (!arg.empty() && arg != entry.name) {
        continue;
      }
      const auto history = owner_.snapshot(0, entry.kind, now, EXAMPLE_SAMPLE_MAX_AGE_MS);
      json_.key(arg.empty() ? entry.name : "sample");
      sampleData(entry.kind, history, target);
      if (!arg.empty() && history.usable.sampleGeneration &&
          history.raw.sampleGeneration != history.usable.sampleGeneration) {
        json_.key("previousSample");
        observation(json_, entry.kind, history.usable, history.bindingGeneration, target);
      }
    }
    if (arg.empty()) {
      json_.endObject();
    }
  } else if (cmd == "settings") {
    json_.key("settings");
    json_.array();
    for (const auto& entry : SETTING_KEYS) {
      json_.object();
      settingIdentity(json_, entry.setting);
      settingEvidence(json_, owner_.settingSnapshot(0, entry.setting));
      const auto desired = owner_.desiredSetting(0, entry.setting);
      nullableInteger(json_, "desired", desired.has_value(), desired ? desired->word : 0);
      json_.endObject();
    }
    json_.endArray();
  } else if (cmd == "health") {
    const auto h = owner_.health(0);
    json_.value("state", driverStateName(h.state));
    json_.flag("initialized", h.initialized);
    json_.integer("consecutiveFailures", h.consecutiveFailures);
    json_.integer("totalFailures", h.totalFailures);
    json_.integer("totalSuccess", h.totalSuccess);
    nullableInteger(json_, "lastOkMs", h.lastOkTimeValid, h.lastOkMs);
    nullableInteger(json_, "lastErrorMs", h.lastErrorTimeValid, h.lastErrorMs);
    json_.flag("lastOkTimeValid", h.lastOkTimeValid);
    json_.flag("lastErrorTimeValid", h.lastErrorTimeValid);
    json_.key("lastError");
    jsonStatus(h.lastError);
  } else if (cmd == "stats") {
    const auto& s = owner_.stats();
    json_.integer("successCount", s.successCount);
    json_.integer("failCount", s.failCount);
    json_.integer("physicalAttempts", owner_.physicalAttempts());
    json_.integer("retryCount", s.retryCount);
    json_.integer("crcErrors", s.crcErrors);
    json_.integer("timeouts", s.timeouts);
    json_.integer("exceptions", s.exceptions);
    json_.integer("frameErrors", s.frameErrors);
    json_.value("lastError", errToString(s.lastError));
    json_.integer("lastErrorDetail", s.lastErrorDetail);
    std::size_t length = 0;
    while (s.lastErrorMsg && length < 128 && s.lastErrorMsg[length]) {
      ++length;
    }
    json_.value("lastErrorMsg", {s.lastErrorMsg ? s.lastErrorMsg : "", length});
    nullableInteger(json_, "lastErrorTimestampMs", s.failCount != 0, s.lastErrorTimestampMs);
    nullableInteger(json_, "lastSuccessTimestampMs", s.successCount != 0, s.lastSuccessTimestampMs);
  } else if (cmd == "status") {
    modelField(json_, target);
    json_.integer("address", target.address);
    json_.integer("baud", target.baud);
    json_.value("addressPolicy",
                target.policy == AddressPolicy::StandardUnicast ? "standard" : "dfrobot-isolated");
    json_.integer("bindingGeneration",
                  owner_.snapshot(0, MeasurementKind::ProcessedDistance, 0, 0).bindingGeneration);
    json_.flag("endpointTrusted", owner_.endpointTrusted(0));
    endpointField(json_, "lastKnownEndpoint", owner_.endpoint(0));
    endpointField(json_, "intendedEndpoint", owner_.possibleEndpoint(0));
    const auto bus = owner_.activity();
    json_.value("ownerPhase", ownerPhase(bus.phase));
    nullableInteger(json_, "activeOp", active_.has_value(), active_ ? active_->id : 0);
    nullableInteger(json_, "busOperation", bus.request.has_value(),
                    bus.request ? bus.request->operationId : 0);
    nullableInteger(json_, "deadlineUs", bus.request.has_value(),
                    bus.request ? bus.request->deadlineUs : 0);
  } else if (cmd == "drv") {
    const auto diag = owner_.diagnostics();
    json_.key("request");
    if (!diag) {
      json_.null();
      return;
    }
    json_.object();
    const auto& wire = diag->exchange;
    const auto& request = wire.request;
    json_.integer("operation", request.operationId);
    json_.integer("address", request.context.address);
    json_.integer("function", static_cast<std::uint8_t>(request.context.function));
    json_.integer("register", request.context.startRegister);
    json_.integer("countOrWriteWord", request.context.countOrWriteWord);
    json_.integer("startedUs", wire.startedUs);
    json_.integer("deadlineUs", request.deadlineUs);
    json_.integer("elapsedUs", diag->elapsedUs);
    nullableInteger(json_, "completedUs", diag->primary.has_value(), wire.completedUs);
    json_.integer("txAccepted", wire.txAccepted);
    json_.flag("txDrained", wire.txDrained);
    json_.integer("rxTotal", wire.rxTotal);
    json_.key("primary");
    if (diag->primary) {
      jsonStatus(*diag->primary);
    } else {
      json_.null();
    }
    json_.key("cleanup");
    if (diag->cleanup) {
      jsonStatus(*diag->cleanup);
    } else {
      json_.null();
    }
    json_.endObject();
  }
}

}  // namespace SEN059x::example
