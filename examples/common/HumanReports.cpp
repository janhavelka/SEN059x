#include "HumanConsole.h"
#include "SettingKeys.h"

namespace SEN059x::example {

void HumanConsole::counterField(std::string_view name, std::optional<std::uint64_t> value) {
  append("[I] ");
  append(name);
  append(" ");
  if (value) {
    number(*value);
  } else {
    field("unknown");
  }
  append("\n");
}

void HumanConsole::health() {
  showTarget(currentTarget());
  const auto h = owner_.health(0);
  append("[I] state ");
  field(driverStateName(h.state));
  append("\n[I] initialized ");
  field(h.initialized ? "true" : "false");
  append("\n");
  counterField("consecutiveFailures", h.consecutiveFailures);
  counterField("totalFailures", h.totalFailures);
  counterField("totalSuccess", h.totalSuccess);
  counterField("lastOkMs", h.lastOkTimeValid ? std::optional{h.lastOkMs} : std::nullopt);
  counterField("lastErrorMs", h.lastErrorTimeValid ? std::optional{h.lastErrorMs} : std::nullopt);
  append("[I] lastOkTimeValid ");
  field(h.lastOkTimeValid ? "true" : "false");
  append("\n[I] lastErrorTimeValid ");
  field(h.lastErrorTimeValid ? "true" : "false");
  append("\n[I] lastError ");
  statusFields(h.lastError);
  append("\n");
}

void HumanConsole::stats() {
  info("Shared bus counters since scheduler construction");
  const auto& s = owner_.stats();
  counterField("successCount", s.successCount);
  counterField("failCount", s.failCount);
  counterField("physicalAttempts", owner_.physicalAttempts());
  counterField("retryCount", s.retryCount);
  counterField("crcErrors", s.crcErrors);
  counterField("timeouts", s.timeouts);
  counterField("exceptions", s.exceptions);
  counterField("frameErrors", s.frameErrors);
  append("[I] lastError ");
  statusFields({s.lastError, s.lastErrorDetail, s.lastErrorMsg}, true);
  append("\n");
  counterField("lastErrorTimestampMs",
               s.failCount ? std::optional{s.lastErrorTimestampMs} : std::nullopt);
  counterField("lastSuccessTimestampMs",
               s.successCount ? std::optional{s.lastSuccessTimestampMs} : std::nullopt);
}

void HumanConsole::settings() {
  showTarget(currentTarget());
  for (const auto& entry : SETTING_KEYS) {
    const auto setting = entry.setting;
    append("[I] ");
    append(entry.key);
    append(" ");
    hexWord(static_cast<std::uint16_t>(setting));
    if (setting == Setting::SwitchThreshold || setting == Setting::SwitchPolarity) {
      append(" (switch-mode)");
    } else if (setting == Setting::UartUnit) {
      append(" (automatic/controlled UART output)");
    } else {
      append(" (RS485)");
    }
    append(" desired ");
    if (const auto desired = owner_.desiredSetting(0, setting)) {
      hexWord(desired->word);
    } else {
      field("none");
    }
    const auto snapshot = owner_.settingSnapshot(0, setting);
    if (!snapshot.observed) {
      append(", observed none, unconfirmed\n");
      continue;
    }
    const auto& observed = *snapshot.observed;
    const auto& value = observed.value;
    append(", observed ");
    hexWord(value.rawWord);
    append(" (");
    if (!value.known) {
      field("outside published range");
    } else if (const char* label = settingWordDescription(value.model, setting, value.rawWord)) {
      field(label);
      if (setting == Setting::Baud) {
        append(" baud");
      } else if (setting == Setting::Range) {
        append(value.model == DeviceModel::SEN0591 ? ", rated 3 m" : ", rated 5 m");
      }
    } else {
      number(value.rawWord);
      if (setting == Setting::SwitchThreshold) {
        append(" mm");
      }
    }
    append(") at ");
    number(observed.completedMs);
    append(" ms, address ");
    number(observed.endpoint.address);
    append(" ");
    number(observed.endpoint.bps);
    append(" baud, ");
    field(snapshot.trusted ? "trusted" : "historical, unconfirmed");
    append("\n");
  }
}

}  // namespace SEN059x::example
