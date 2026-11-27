#include <charconv>

#include "HumanConsole.h"

#include <SEN059x/Version.h>

namespace SEN059x::example {
namespace {
enum class Action {
  Help,
  Version,
  Color,
  Verbose,
  Model,
  Profile,
  UseAddress,
  Host,
  Status,
  Drv,
  Health,
  Stats,
  Settings,
  Latest,
  Selftest,
  Read,
  Config,
  Maintenance,
  ConfigSet,
  SetAddress,
  SetBaud,
  Broadcast,
  ReadReg,
  ReadBaud,
  Raw,
  Watch,
  JobStatus,
  Result,
  Cancel,
  Diagnose,
  Recover
};

struct SampleKind {
  std::string_view name;
  MeasurementKind kind;
};
constexpr SampleKind SAMPLE_KINDS[] = {
    {"processed", MeasurementKind::ProcessedDistance},
    {"realtime", MeasurementKind::RealTimeDistance},
    {"temperature", MeasurementKind::Temperature},
    {"echo", MeasurementKind::EchoTime},
};
constexpr std::string_view RANGE_EFFECT =
    "Processed distance, realtime distance and echo reads start ranging. Temperature reads do not.";

struct Command {
  Action action;
  std::string_view group;
  std::string_view name;
  std::string_view arguments;  // Includes the leading separator for nonempty arguments.
  std::string_view alias;
  std::string_view description;
  std::string_view detail;
  std::string_view effects;
  std::size_t maxArguments;
};

constexpr std::string_view LOCAL_ONLY =
    "Works with either model or no selection. No sensor transfer or ranging.";
constexpr std::string_view SET_KEYS =
    "Keys: address, baud, polarity, threshold, angle, uart-unit, noise, range. "
    "Use help config set <key> for units and model limits.";
constexpr std::string_view WRITE_EFFECT =
    "Preview uses zero transfers. With maintenance on isolated, append confirm for one addressed "
    "FC06 and an independent same-register FC03 at the intended endpoint. No ranging.";
constexpr std::string_view VENDOR_EFFECT =
    "Preview uses zero transfers. With maintenance on isolated, append confirm for one send-only "
    "FC06 to destination 255. Use a separate addressed config read to verify. No ranging.";
constexpr Command COMMANDS[] = {
    {Action::Help, "Console", "help", " [command]", "?", "List commands or explain one command.",
     "Give a command name, its alias, or a topic such as config set threshold.", LOCAL_ONLY, 3},
    {Action::Version, "Console", "version", "", "ver", "Show library and application identity.",
     "Takes no arguments. Missing build metadata is shown as unknown.", LOCAL_ONLY, 0},
    {Action::Color, "Console", "color", " [on|off]", "", "Show or change human output color.",
     "Use on for ANSI color or off for plain text. The default is off.", LOCAL_ONLY, 1},
    {Action::Verbose, "Console", "verbose", " [on|off]", "", "Show or change human diagnostics.",
     "Use on or off to select verbose human diagnostics. The default is off.", LOCAL_ONLY, 1},
    {Action::Model, "Target", "model", " [SEN0591|SEN0592]", "", "Show or select the sensor model.",
     "Select SEN0591 (3 m) or SEN0592 (5 m) from the part marking.", LOCAL_ONLY, 1},
    {Action::Profile, "Target", "profile", " [standard|dfrobot-isolated]", "",
     "Show or select the address policy.",
     "Use standard for addresses 1-247 or dfrobot-isolated for 1-254 on a dedicated bench.",
     LOCAL_ONLY, 1},
    {Action::UseAddress, "Target", "useaddr", " <address>", "", "Select the local slave address.",
     "Give a decimal address from 1-247 (standard) or 1-254 (dfrobot-isolated).", LOCAL_ONLY, 1},
    {Action::Host, "Target", "host", " [baud <rate>]", "", "Show or configure the host UART.",
     "Give an admitted decimal rate in bits per second. The serial format is fixed at 8N1.",
     LOCAL_ONLY, 2},
    {Action::Status, "Cached", "status", "", "", "Show binding, bus owner and current operation.",
     "Shows endpoint trust and the operation's absolute deadline in microseconds.", LOCAL_ONLY, 0},
    {Action::Drv, "Cached", "drv", "", "", "Show the latest bus request and its results.",
     "Times are in microseconds. Enable verbose for TX and RX byte counts.", LOCAL_ONLY, 0},
    {Action::Health, "Cached", "health", "", "", "Show communication health for the binding.",
     "Shows device reply and failure counts and monotonic timestamps in milliseconds.", LOCAL_ONLY,
     0},
    {Action::Stats, "Cached", "stats", "", "", "Show shared bus operation and transport counters.",
     "Includes the last failure detail and completion timestamps in milliseconds.", LOCAL_ONLY, 0},
    {Action::Settings, "Cached", "settings", "", "", "Show desired and observed configuration.",
     "Shows each register's raw word, observation time in milliseconds and readback trust.",
     LOCAL_ONLY, 0},
    {Action::Latest, "Cached", "latest", " [processed|realtime|temperature|echo]", "sample",
     "Show cached measurements.",
     "Omit the kind to show all four. Distances are in mm, temperature in degrees Celsius and "
     "echo time in microseconds. Usable samples up to 1000 ms old are fresh.",
     LOCAL_ONLY, 1},
    {Action::Read, "Measurements", "read", " <processed|realtime|temperature|echo>", "",
     "Read one measurement.", "Returns mm, degrees Celsius or echo time in microseconds.",
     RANGE_EFFECT, 1},
    {Action::Watch, "Measurements", "watch", " <kind> <count> <interval_ms>", "",
     "Schedule a finite series of reads.",
     "Use a read kind, 1-100 readings and 1-60000 ms between starts. Missed intervals are skipped.",
     RANGE_EFFECT, 3},
    {Action::Config, "Configuration", "config", " read <key|register>", "", "Read one setting.",
     "Keys: address, baud, polarity, threshold, angle, uart-unit, noise, range. Use decimal or 0x "
     "hexadecimal register addresses.",
     "One addressed FC03 updates this setting. No ranging.", 2},
    {Action::ReadBaud, "Configuration", "readbaud", "", "", "Read the baud code and rate.",
     "Equivalent to config read baud.", "One addressed FC03 at 0x0201. No ranging.", 0},
    {Action::ReadReg, "Configuration", "readreg", " <register>", "", "Read one typed register.",
     "Give one of the twelve documented registers in decimal or 0x hexadecimal.", RANGE_EFFECT, 1},
    {Action::Maintenance, "Commissioning", "maintenance", " [on isolated|off]", "",
     "Show, grant or release one write.",
     "Select a model and declare that one device is physically isolated. The grant covers one "
     "write for this binding and session. It expires after 60 seconds of inactivity.",
     "No sensor transfer or ranging. Off releases the grant.", 2},
    {Action::ConfigSet, "Commissioning", "config set", " <key> <value> [confirm]", "",
     "Preview or verify an addressed write.", SET_KEYS, WRITE_EFFECT, 4},
    {Action::SetAddress, "Commissioning", "setaddr", " <address> [confirm]", "",
     "Alias for config set address.", "Give a decimal slave address.", WRITE_EFFECT, 2},
    {Action::SetBaud, "Commissioning", "setbaud", " <rate> [confirm]", "",
     "Alias for config set baud.", "Give a rate in bits per second.", WRITE_EFFECT, 2},
    {Action::Broadcast, "Commissioning", "broadcast", " set <key> <value> [confirm]", "",
     "Preview or send an isolated vendor write.",
     "Requires profile dfrobot-isolated on a dedicated single-device bench. Use help broadcast "
     "set <key> for the value and units.",
     VENDOR_EFFECT, 4},
    {Action::Raw, "Block diagnostic", "readregs", " <start> <count>", "",
     "Read a contiguous raw block.",
     "Give a decimal or 0x hexadecimal start and a decimal count 1-125. Every register must be "
     "documented; the longest span is three words. Block support and sampling coherence are "
     "unverified.",
     "One addressed FC03. Range-containing spans may start ranging. No retry or typed cache "
     "update. The owner allows 50 ms per configuration word because its device latency is "
     "unspecified.",
     2},
    {Action::JobStatus, "Jobs", "job", " status", "", "Show the active or most recent operation.",
     "Shows the generation, phase and deadline on the owner clock.", LOCAL_ONLY, 1},
    {Action::Result, "Jobs", "result", " [op]", "", "Show the retained terminal result.",
     "Omit the ID for the last terminal. A decimal ID selects it or the last completed write.",
     LOCAL_ONLY, 1},
    {Action::Cancel, "Jobs", "cancel", " [op]", "stop", "Stop the active operation.",
     "An optional decimal operation ID must match the active job.",
     "Stops pending work. A transmitted request needs settlement and a write may have taken "
     "effect.",
     1},
    {Action::Diagnose, "Diagnostics", "diagnose", "", "diag", "Check the addressed device.",
     "Reads 0x0200 and compares the observed address with the selected slave.",
     "One addressed FC03. No ranging.", 0},
    {Action::Recover, "Diagnostics", "recover", "", "", "Service host-side line recovery.",
     "Polls for physical drain or a qualified retired response, or checks adapter-confirmed "
     "isolation.",
     "No new sensor request or ranging.", 0},
    {Action::Selftest, "Diagnostics", "selftest", "", "",
     "Check six published Modbus vectors offline.",
     "Builds requests and checks published replies for both models.", LOCAL_ONLY, 0},
};

const char* profileName(AddressPolicy policy) {
  return policy == AddressPolicy::DFRobotIsolated ? "dfrobot-isolated" : "standard";
}

template <class Integer>
bool parseDecimal(std::string_view token, Integer& out) {
  const char* end = token.data() + token.size();
  const auto result = std::from_chars(token.data(), end, out, 10);
  return result.ec == std::errc{} && result.ptr == end;
}

bool parseRegister(std::string_view token, std::uint16_t& reg) {
  if (token.substr(0, 2) != "0x") {
    return parseDecimal(token, reg);
  }
  token.remove_prefix(2);
  const auto end = token.data() + token.size();
  const auto result = std::from_chars(token.data(), end, reg, 16);
  return result.ec == std::errc{} && result.ptr == end;
}

const Command* findCommand(std::string_view name, std::string_view subcommand) {
  if (name == "config" && subcommand == "set") {
    name = "config set";
  }
  for (const auto& command : COMMANDS) {
    if (command.name == name || (!command.alias.empty() && command.alias == name)) {
      return &command;
    }
  }
  return nullptr;
}

const SettingKey* findKey(std::string_view name) {
  for (const auto& key : SETTING_KEYS) {
    if (name == key.key) {
      return &key;
    }
  }
  return nullptr;
}

// Application identity fields are optional NUL-terminated printable ASCII strings.
std::string_view identityField(const char* value) {
  if (value != nullptr) {
    for (std::size_t i = 0; i <= 64; ++i) {
      if (value[i] == '\0' && i != 0) {
        return {value, i};
      }
      if (value[i] < ' ' || value[i] > '~') {
        break;
      }
    }
  }
  return "unknown";
}
}  // namespace

void HumanConsole::hexByte(std::uint8_t byte) {
  constexpr std::string_view digits = "0123456789ABCDEF";
  const char bytes[] = {'0', 'x', digits[byte >> 4], digits[byte & 15]};
  field({bytes, sizeof(bytes)});
}

void HumanConsole::hexWord(std::uint16_t word) {
  constexpr char DIGITS[] = "0123456789ABCDEF";
  char bytes[] = "0x0000";
  for (int i = 5; i >= 2; --i) {
    bytes[i] = DIGITS[word & 0xF];
    word >>= 4;
  }
  field(bytes);
}

void HumanConsole::sampleHistory(const SampleHistory& history) {
  if (history.freshness == Freshness::NeverSampled) {
    append("never sampled\n");
    return;
  }
  const auto& measurement = history.measurement;
  if (measurement.value) {
    if (measurement.unit == MeasurementUnit::DegreesCelsius) {
      // Usable temperature observations are nonnegative whole or half degrees.
      const auto tenths = static_cast<unsigned>(*measurement.value * 10);
      number(tenths / 10);
      append(".");
      number(tenths % 10);
      append(
          " \xC2\xB0"
          "C");  // UTF-8 degree sign.
    } else {
      number(static_cast<unsigned>(*measurement.value));
      append(measurement.unit == MeasurementUnit::Millimetres ? " mm" : " \xC2\xB5s");
    }
    append(", ");
  }
  append("raw ");
  hexWord(measurement.rawWord);
  append(", sample generation ");
  number(history.sampleGeneration);
  append(", observed ");
  if (measurement.timeValid) {
    number(measurement.completedMs);
    append(" ms");
  } else {
    append("unknown");
  }
  append(", age ");
  if (history.ageMs) {
    number(*history.ageMs);
    append(" ms");
  } else {
    append("unknown");
  }
  switch (history.freshness) {
    case Freshness::Fresh:
      append(", valid, fresh");
      break;
    case Freshness::Stale:
      append(", valid, stale");
      break;
    case Freshness::UnknownTime:
      append(", valid, freshness unknown");
      break;
    case Freshness::Unusable:
      if (measurement.quality == MeasurementQuality::AmbiguousTemperature) {
        append(", invalid, ambiguous temperature");
      } else if (measurement.quality == MeasurementQuality::UnexpectedQuantization) {
        append(", invalid, unexpected quantization");
      } else {
        append(", invalid, out of profile");
      }
      break;
    case Freshness::NeverSampled:
      break;
  }
  append("\n");
}

void HumanConsole::sampleLine(MeasurementKind kind, const SampleSnapshot& snapshot, bool previous) {
  for (const auto& entry : SAMPLE_KINDS) {
    if (entry.kind != kind) {
      continue;
    }
    append(previous ? "[I] Previous sample, " : "[I] ");
    field(entry.name);
    append(" ");
    hexWord(static_cast<std::uint16_t>(entry.kind));
    append(", ");
    sampleHistory(snapshot.raw);
    if (snapshot.usable.sampleGeneration != 0 &&
        snapshot.usable.sampleGeneration != snapshot.raw.sampleGeneration) {
      append("    previous usable, ");
      sampleHistory(snapshot.usable);
    }
    return;
  }
}

void HumanConsole::latest(std::string_view kind) {
  const auto target = owner_.localTarget(0);
  const auto now = owner_.nowMs();
  bool shown = false;
  for (const auto& entry : SAMPLE_KINDS) {
    if (!kind.empty() && kind != entry.name) {
      continue;
    }
    const auto snapshot = owner_.snapshot(0, entry.kind, now, EXAMPLE_SAMPLE_MAX_AGE_MS);
    if (!shown) {
      append("[I] ");
      if (target.model) {
        field(*target.model == DeviceModel::SEN0591 ? "SEN0591" : "SEN0592");
      } else {
        field("Model unselected");
      }
      append(" address ");
      number(target.address);
      append(", binding generation ");
      number(snapshot.bindingGeneration);
      append("\n");
      shown = true;
    }
    sampleLine(entry.kind, snapshot, false);
  }
  if (!shown) {
    error("Expected processed, realtime, temperature or echo.");
  }
}

void HumanConsole::showTarget(const TargetSelection& target) {
  append("[I] Model ");
  if (target.model) {
    field(*target.model == DeviceModel::SEN0591 ? "SEN0591" : "SEN0592");
  } else {
    field("unselected");
  }
  append(", profile ");
  field(profileName(target.policy));
  append(", address ");
  number(target.address);
  append(", ");
  number(target.baud);
  append(" baud 8N1\n");
}

void HumanConsole::status(const TargetSelection& target) {
  showTarget(target);
  const bool trusted = owner_.endpointTrusted(0);
  append("[I] Binding generation ");
  number(owner_.snapshot(0, MeasurementKind::ProcessedDistance, 0, 0).bindingGeneration);
  append(", endpoint ");
  field(trusted ? "trusted" : "unconfirmed");
  append("\n");
  const auto showEndpoint = [this](std::string_view label, Endpoint at) {
    append("[I] ");
    append(label);
    append(" endpoint address ");
    number(at.address);
    append(", ");
    number(at.bps);
    append(" baud\n");
  };
  const auto intended = owner_.possibleEndpoint(0);
  if (const auto endpoint = owner_.endpoint(0)) {
    showEndpoint(trusted || intended ? "Last-known" : "Selected", *endpoint);
  }
  if (intended) {
    showEndpoint("Intended", *intended);
  }
  const auto activity = owner_.activity();
  append("[I] Owner ");
  switch (activity.phase) {
    case OwnerPhase::Idle:
      field("idle");
      break;
    case OwnerPhase::Queued:
      field("queued");
      break;
    case OwnerPhase::Active:
      field("active");
      break;
    case OwnerPhase::Recovery:
      field("recovery");
      break;
  }
  append(active_ ? ", bus operation " : ", current operation ");
  if (activity.phase == OwnerPhase::Recovery) {
    append("none, retired operation ");
  }
  if (activity.request) {
    const auto& request = *activity.request;
    number(request.operationId);
    append(", address ");
    number(request.context.address);
    append(", register ");
    hexWord(request.context.startRegister);
    append(", deadline ");
    number(request.deadlineUs);
    append(" us");
  } else {
    field("none");
  }
  append("\n");
  if (active_) {
    jobStatus();
  }
}

void HumanConsole::drv() {
  const auto history = owner_.diagnostics();
  if (!history) {
    info("No bus request history.");
    return;
  }
  const auto& wire = history->exchange;
  const auto& request = wire.request;
  append("[I] Bus request, operation ");
  number(request.operationId);
  append(", address ");
  number(request.context.address);
  append(", function ");
  hexByte(static_cast<std::uint8_t>(request.context.function));
  append(", register ");
  hexWord(request.context.startRegister);
  append(request.context.function == FunctionCode::WriteSingleRegister ? ", write word "
                                                                       : ", count ");
  number(request.context.countOrWriteWord);
  append("\n[I] Started ");
  number(wire.startedUs);
  append(" us, deadline ");
  number(request.deadlineUs);
  append(" us, elapsed ");
  number(history->elapsedUs);
  append(" us");
  if (history->primary) {
    append(", completed ");
    number(wire.completedUs);
    append(" us");
  }
  const auto show = [this](std::string_view label, const std::optional<Status>& result) {
    append("\n[I] ");
    append(label);
    append(" ");
    if (result) {
      statusFields(*result);
    } else {
      field("pending");
    }
  };
  show("Primary", history->primary);
  show("Cleanup", history->cleanup);
  if (verbose_) {
    append("\n[I] TX accepted ");
    number(wire.txAccepted);
    append(wire.txDrained ? " bytes, drained" : " bytes, drain unconfirmed");
    append(", RX observed ");
    number(wire.rxTotal);
    append(" bytes");
    if (wire.rxTotal) {
      append(", last delivery ");
      number(wire.lastDeliveryUs);
      append(" us");
    }
    append("\n[I] Exchange ");
    statusFields(wire.status);
  }
  append("\n");
}

void HumanConsole::statusError(Status status) {
  field("[E]");
  append(" ");
  statusFields(status);
  append("\n");
}

void HumanConsole::statusFields(Status status, bool statsLabels) {
  field(errToString(status.code));
  append(statsLabels ? " lastErrorDetail " : " detail ");
  number(status.detail);
  if (statsLabels) {
    append(" lastErrorMsg");
  }
  if (status.msg != nullptr && status.msg[0] != '\0') {
    append(" ");
    // Status text may be supplied by the adapter. Bound each service turn's output.
    std::size_t length = 0;
    while (length < 128 && status.msg[length] != '\0') {
      ++length;
    }
    append({status.msg, length});
  } else if (statsLabels) {
    append(" none");
  }
  if (status.code == Err::EXCEPTION) {
    append(" exception ");
    hexByte(static_cast<std::uint8_t>(status.detail));
    append(" ");
    append(modbusExceptionToString(static_cast<std::uint8_t>(status.detail)));
  }
}

void HumanConsole::help() {
  append("=== SEN059x CLI Help ===\n");
  std::string_view group;
  for (const auto& command : COMMANDS) {
    if (command.group != group) {
      group = command.group;
      field(group);
      append("\n");
    }
    append("  ");
    append(command.name);
    append(command.arguments);
    constexpr std::string_view PADDING = "                                              ";
    const auto width = command.name.size() + command.arguments.size();
    append(width < PADDING.size() ? PADDING.substr(width) : "  ");
    append(command.description);
    if (!command.alias.empty()) {
      append(" (alias ");
      append(command.alias);
      append(")");
    }
    append("\n");
  }
  append("Selected profile ");
  field(profileName(owner_.localTarget(0).policy));
  append("\n");
}

void HumanConsole::settingHelp(const SettingKey& key, std::string_view usage, bool write,
                               WriteScope scope) {
  append(usage);
  if (usage != "setaddr" && usage != "setbaud") {
    append(" ");
    append(key.key);
  }
  if (write) {
    append(" <value> [confirm]");
  }
  append(", register ");
  hexWord(static_cast<std::uint16_t>(key.setting));
  append("\n");
  switch (key.setting) {
    case Setting::Address:
      info("Decimal slave address. Standard admits 1..247, dfrobot-isolated admits 1..254.");
      break;
    case Setting::Baud:
      info("Enter bits per second. The register stores the corresponding baud code.");
      break;
    case Setting::SwitchPolarity:
      info("Switch-mode output strictly below threshold. Output at equality is unspecified.");
      break;
    case Setting::SwitchThreshold:
      info("Switch-mode threshold distance in mm.");
      break;
    case Setting::UartUnit:
      info(
          "Enter mm or us for automatic/controlled UART output. RS485 measurement units are "
          "fixed.");
      break;
    case Setting::Angle:
      info("Sensor beam angle level. Published angles are approximate.");
      break;
    case Setting::Noise:
      info("Sensor noise suppression level for the power supply and environment.");
      break;
    case Setting::Range:
      info("Sensor range level. SEN0591 is rated 3 m, SEN0592 is rated 5 m.");
      break;
  }
  for (const auto model : {DeviceModel::SEN0591, DeviceModel::SEN0592}) {
    SettingDescriptor bounds;
    settingDescriptor(model, key.setting, bounds);
    append(model == DeviceModel::SEN0591 ? "[I] SEN0591 " : "[I] SEN0592 ");
    append(key.setting == Setting::SwitchThreshold ? "switch threshold (mm) " : "raw word ");
    number(bounds.minWord);
    append("..");
    number(bounds.maxWord);
    append("\n");
    if (key.setting != Setting::Address && key.setting != Setting::SwitchThreshold) {
      for (auto word = bounds.minWord; word <= bounds.maxWord; ++word) {
        append("    ");
        number(word);
        append(" = ");
        field(settingWordDescription(model, key.setting, word));
        append(key.setting == Setting::Baud ? " bps\n" : "\n");
      }
    }
  }
  if (write) {
    if (scope == WriteScope::VendorSendOnly) {
      info("Requires profile dfrobot-isolated on a dedicated single-device bench.");
      info(VENDOR_EFFECT);
    } else {
      info(WRITE_EFFECT);
    }
    info(
        "Readback establishes the register word. Physical output and power-cycle persistence "
        "require bench evidence.");
  } else {
    info("One addressed FC03 updates this setting. No ranging.");
  }
}

void HumanConsole::writeCommand(std::string_view key, std::string_view input, bool confirm,
                                WriteScope scope) {
  const auto* entry = findKey(key);
  if (!entry) {
    error("Unknown configuration key.");
    return;
  }
  const auto target = owner_.localTarget(0);
  if (!target.model) {
    statusError({Err::NOT_INITIALIZED, 0, "Select SEN0591 or SEN0592 first"});
    return;
  }
  SettingDescriptor descriptor;
  settingDescriptor(*target.model, entry->setting, descriptor);
  SettingValue value{descriptor.unit, 0};
  if (entry->setting == Setting::Baud) {
    std::uint32_t rate = 0;
    Baud code{};
    if (!parseDecimal(input, rate) || !valueToBaud(rate, code)) {
      statusError({Err::ILLEGAL_VALUE, 0, "Expected a supported baud rate in bits per second"});
      return;
    }
    value.word = static_cast<std::uint16_t>(code);
  } else if (entry->setting == Setting::UartUnit) {
    if (input != "mm" && input != "us") {
      statusError(
          {Err::ILLEGAL_VALUE, 0, "Expected mm or us for automatic/controlled UART output"});
      return;
    }
    value.word = input == "us" ? 1 : 0;
  } else if (!parseDecimal(input, value.word)) {
    statusError({Err::ILLEGAL_VALUE, 0, "Expected an unsigned decimal setting value"});
    return;
  }

  ConfigurationOperation plan;
  if (const auto status = owner_.previewWrite(0, entry->setting, value, plan, scope); !status) {
    statusError(status);
    return;
  }
  if (confirm) {
    startWrite(plan);
    return;
  }
  const bool vendor = scope == WriteScope::VendorSendOnly;
  append("[I] Preview only, ");
  field(*target.model == DeviceModel::SEN0591 ? "SEN0591" : "SEN0592");
  append(", profile ");
  field(profileName(target.policy));
  append(vendor ? ", send-only FC06 destination " : ", addressed FC06 destination ");
  number(vendor ? 255 : plan.original.address);
  append(", ");
  append(entry->key);
  append(", register ");
  hexWord(static_cast<std::uint16_t>(entry->setting));
  append(", raw word ");
  hexWord(value.word);
  append(" (");
  if (const auto* label = settingWordDescription(*target.model, entry->setting, value.word)) {
    field(label);
    if (entry->setting == Setting::Baud) {
      append(" bps");
    } else if (entry->setting == Setting::SwitchPolarity) {
      append(", switch-mode");
    } else if (entry->setting == Setting::UartUnit) {
      append(", automatic/controlled UART output");
    } else if (entry->setting == Setting::Range) {
      append(*target.model == DeviceModel::SEN0591 ? ", rated 3 m" : ", rated 5 m");
    }
  } else {
    number(value.word);
    if (entry->setting == Setting::SwitchThreshold) {
      append(" mm, switch-mode");
    }
  }
  append(")\n[I] Original endpoint address ");
  number(plan.original.address);
  append(", ");
  number(plan.original.bps);
  append(" bps, intended endpoint address ");
  number(plan.intended.address);
  append(", ");
  number(plan.intended.bps);
  append(" bps\n");
  info(vendor ? VENDOR_EFFECT : WRITE_EFFECT);
  if (!owner_.endpointTrusted(0)) {
    info("Confirm the selected endpoint with an addressed read before writing.");
  }
}

void HumanConsole::dispatch(const char* const* words, std::size_t count) {
  owner_.sessionActivity(session_);
  const Command* command = findCommand(words[0], count >= 2 ? words[1] : "");
  if (command == nullptr) {
    error("Unknown command. Use help to list commands.");
    return;
  }
  if (count - 1 > command->maxArguments) {
    error("Too many arguments.");
    return;
  }
  auto next = owner_.localTarget(0);
  switch (command->action) {
    case Action::Help: {
      if (count == 1) {
        help();
        return;
      }
      const std::string_view verb = count >= 3 ? words[2] : "";
      const Command* topic = findCommand(words[1], verb);
      if (topic == nullptr) {
        error("Unknown help topic.");
        return;
      }
      const bool read = topic->action == Action::Config && verb == "read";
      const bool vendor = topic->action == Action::Broadcast && verb == "set";
      const bool settingTopic = read || vendor || topic->action == Action::ConfigSet;
      if (count == 4 && settingTopic) {
        if (const auto* key = findKey(words[3])) {
          settingHelp(*key, read ? "config read" : (vendor ? "broadcast set" : "config set"), !read,
                      vendor ? WriteScope::VendorSendOnly : WriteScope::Addressed);
        } else {
          error("Unknown configuration key.");
        }
        return;
      }
      if (count > 2 && !(count == 3 && (settingTopic || (topic->action == Action::JobStatus &&
                                                         verb == "status")))) {
        error("Unknown help topic.");
        return;
      }
      if (topic->action == Action::SetAddress || topic->action == Action::SetBaud) {
        settingHelp(*findKey(topic->action == Action::SetAddress ? "address" : "baud"), topic->name,
                    true, WriteScope::Addressed);
        info(topic->description);
        return;
      }
      append(topic->name);
      append(topic->arguments);
      append("    ");
      append(topic->description);
      append("\n");
      info(topic->detail);
      info(topic->effects);
      if (topic->action == Action::Profile || topic->action == Action::ConfigSet ||
          topic->action == Action::Broadcast) {
        append("Selected profile ");
        field(profileName(next.policy));
        append("\n");
      }
      return;
    }
    case Action::Version: {
      const auto show = [this](std::string_view label, const char* value) {
        append("[I] ");
        append(label);
        append(" ");
        field(identityField(value));
        append("\n");
      };
      show("Library version", VERSION);
      show("Library revision", GIT_COMMIT);
      show("Library dirty status", GIT_STATUS);
      show("Firmware revision", identity_.firmwareRevision);
      show("Firmware dirty status", identity_.dirty);
      show("Framework", identity_.framework);
      show("Framework version", identity_.frameworkVersion);
      show("Target", identity_.target);
      show("Board", identity_.board);
      show("Schema", "1");
      return;
    }
    case Action::Status:
      status(next);
      return;
    case Action::Drv:
      drv();
      return;
    case Action::Health:
      health();
      return;
    case Action::Stats:
      stats();
      return;
    case Action::Settings:
      settings();
      return;
    case Action::Selftest:
      selftest();
      return;
    case Action::Latest:
      latest(count == 2 ? words[1] : "");
      return;
    case Action::Read:
    case Action::Watch: {
      const bool watch = command->action == Action::Watch;
      unsigned readings = 1, interval = 0;
      if (count != (watch ? 4u : 2u) ||
          (watch && (!parseDecimal(words[2], readings) || readings < 1 || readings > 100 ||
                     !parseDecimal(words[3], interval) || interval < 1 || interval > 60000))) {
        error("Expected a read kind, and for watch a count 1-100 and interval 1-60000 ms.");
        return;
      }
      for (const auto& kind : SAMPLE_KINDS) {
        if (kind.name == words[1]) {
          startJob(watch ? JobMode::Watch : JobMode::Measurement,
                   static_cast<std::uint16_t>(kind.kind), 1, readings, interval);
          return;
        }
      }
      error("Expected processed, realtime, temperature or echo.");
      return;
    }
    case Action::ConfigSet:
    case Action::Broadcast:
    case Action::SetAddress:
    case Action::SetBaud: {
      const bool alias =
          command->action == Action::SetAddress || command->action == Action::SetBaud;
      const std::size_t valueIndex = alias ? 1 : 3;
      if (count <= valueIndex || (!alias && std::string_view(words[1]) != "set")) {
        error(
            "Expected config set <key> <value>, broadcast set <key> <value>, setaddr <address> or "
            "setbaud <bps>. Append confirm to write.");
        return;
      }
      const bool confirm = count == valueIndex + 2;
      if (confirm && std::string_view(words[valueIndex + 1]) != "confirm") {
        error("Expected literal confirm after the value.");
        return;
      }
      const std::string_view key =
          alias ? (command->action == Action::SetAddress ? "address" : "baud") : words[2];
      writeCommand(key, words[valueIndex], confirm,
                   command->action == Action::Broadcast ? WriteScope::VendorSendOnly
                                                        : WriteScope::Addressed);
      return;
    }
    case Action::Config:
    case Action::ReadReg: {
      const bool config = command->action == Action::Config;
      if ((config && (count != 3 || std::string_view(words[1]) != "read")) ||
          (!config && count != 2)) {
        error("Expected config read <key|register> or readreg <register>.");
        return;
      }
      const std::string_view selector = words[count - 1];
      std::uint16_t reg = 0;
      const bool numeric = parseRegister(selector, reg);
      for (const auto& key : SETTING_KEYS) {
        if ((config && key.key == selector) ||
            (numeric && static_cast<std::uint16_t>(key.setting) == reg)) {
          startJob(JobMode::Setting, static_cast<std::uint16_t>(key.setting));
          return;
        }
      }
      if (!config && numeric) {
        for (const auto& kind : SAMPLE_KINDS) {
          if (static_cast<std::uint16_t>(kind.kind) == reg) {
            startJob(JobMode::Measurement, reg);
            return;
          }
        }
      }
      error("Unknown register or setting key.");
      return;
    }
    case Action::Maintenance:
      if (count == 3 && std::string_view(words[1]) == "on" &&
          std::string_view(words[2]) == "isolated") {
        const auto result = owner_.grantIsolatedMaintenance(0, session_);
        if (!result) {
          statusError(result);
          return;
        }
      } else if (count == 2 && std::string_view(words[1]) == "off") {
        owner_.maintenanceOff();
      } else if (count != 1) {
        error("Expected maintenance, maintenance on isolated or maintenance off.");
        return;
      }
      if (owner_.maintenanceGranted(0, session_)) {
        info("Maintenance on isolated, one write available for this binding and session.");
        showTarget(next);
      } else {
        info("Maintenance off.");
      }
      return;
    case Action::ReadBaud:
      startJob(JobMode::Setting, static_cast<std::uint16_t>(Setting::Baud));
      return;
    case Action::Diagnose:
      startJob(JobMode::Diagnose, static_cast<std::uint16_t>(Setting::Address));
      return;
    case Action::Raw: {
      std::uint16_t reg = 0, length = 0;
      if (count != 3 || !parseRegister(words[1], reg) || !parseDecimal(words[2], length) ||
          length == 0 || length > 125) {
        error("Expected readregs <start> <decimal count 1-125>.");
        return;
      }
      startJob(JobMode::Raw, reg, length);
      return;
    }
    case Action::JobStatus:
      if (count == 2 && std::string_view(words[1]) == "status") {
        jobStatus();
      } else {
        error("Expected job status.");
      }
      return;
    case Action::Cancel:
    case Action::Result: {
      std::uint64_t id = 0;
      if (count == 2 && (!parseDecimal(words[1], id) || id == 0)) {
        error("Expected a positive decimal operation ID.");
        return;
      }
      const auto selected = count == 2 ? std::optional{id} : std::nullopt;
      if (command->action == Action::Cancel) {
        cancelJob(selected);
      } else {
        jobResult(selected);
      }
      return;
    }
    case Action::Recover: {
      const auto phase = owner_.activity().phase;
      if (phase == OwnerPhase::Active || phase == OwnerPhase::Queued) {
        statusError({Err::BUS_BUSY, 0, "Owner has pending work"});
      } else if (owner_.recover() || owner_.recoverIsolated()) {
        info("Host transport settled.");
      } else {
        statusError({Err::BUS_BUSY, 0, "Wire settlement pending"});
      }
      return;
    }
    case Action::Color:
    case Action::Verbose: {
      bool& enabled = command->action == Action::Color ? color_ : verbose_;
      if (count == 2) {
        const std::string_view value = words[1];
        if (value != "on" && value != "off") {
          error("Expected on or off.");
          return;
        }
        enabled = value == "on";
      }
      append("[I] ");
      append(command->name);
      append(" ");
      field(enabled ? "on" : "off");
      append("\n");
      return;
    }
    case Action::Model:
      if (count == 2) {
        const std::string_view value = words[1];
        if (value == "SEN0591") {
          next.model = DeviceModel::SEN0591;
        } else if (value == "SEN0592") {
          next.model = DeviceModel::SEN0592;
        } else {
          error("Expected SEN0591 or SEN0592.");
          return;
        }
      }
      break;
    case Action::Profile:
      if (count == 2) {
        const std::string_view value = words[1];
        if (value == "standard") {
          next.policy = AddressPolicy::StandardUnicast;
        } else if (value == "dfrobot-isolated") {
          next.policy = AddressPolicy::DFRobotIsolated;
        } else {
          error("Expected standard or dfrobot-isolated.");
          return;
        }
      }
      break;
    case Action::UseAddress: {
      std::uint32_t address = 0;
      if (count != 2 || !parseDecimal(words[1], address) || address > 255) {
        error("Expected a decimal slave address under the selected profile.");
        return;
      }
      next.address = static_cast<std::uint8_t>(address);
      break;
    }
    case Action::Host:
      if (count != 1 && (count != 3 || std::string_view(words[1]) != "baud" ||
                         !parseDecimal(words[2], next.baud))) {
        error("Expected host or host baud <rate> in bits per second.");
        return;
      }
      break;
  }
  if (count > 1) {
    if (active_) {
      statusError({Err::BUS_BUSY, 0, "Target change requires the active job to finish"});
      return;
    }
    if (const Status status = owner_.selectLocalTarget(0, next, command->action == Action::Host);
        !status) {
      statusError(status);
      return;
    }
  }
  showTarget(next);
  if (command->action == Action::Host) {
    append("[I] Host UART ");
    if (const auto applied = owner_.hostBaud()) {
      number(*applied);
      append(" baud 8N1");
    } else {
      field("unknown");
    }
    append("\n[I] Supported baud rates (bits/s)");
    for (auto code = static_cast<unsigned>(Baud::Bps4800);
         code <= static_cast<unsigned>(Baud::Bps115200); ++code) {
      std::uint32_t rate = 0;
      if (baudToValue(static_cast<Baud>(code), rate) && owner_.supportsBaud(rate)) {
        append(" ");
        number(rate);
      }
    }
    append("\n");
  }
}

}  // namespace SEN059x::example
