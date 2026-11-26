#include <charconv>

#include "HumanConsole.h"

#include <SEN059x/Version.h>

namespace SEN059x::example {
namespace {
enum class Action { Help, Version, Color, Verbose, Model, Profile, UseAddress, Host };

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
constexpr Command COMMANDS[] = {
    {Action::Help, "Console", "help", " [command]", "?", "List commands or explain one command.",
     "Give a command name or its alias for detailed usage.", LOCAL_ONLY, 1},
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
};

const char* profileName(AddressPolicy policy) {
  return policy == AddressPolicy::DFRobotIsolated ? "dfrobot-isolated" : "standard";
}

bool parseDecimal(std::string_view token, std::uint32_t& out) {
  const char* end = token.data() + token.size();
  const auto result = std::from_chars(token.data(), end, out, 10);
  return result.ec == std::errc{} && result.ptr == end;
}

const Command* findCommand(std::string_view name) {
  for (const auto& command : COMMANDS) {
    if (command.name == name || (!command.alias.empty() && command.alias == name)) {
      return &command;
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

void HumanConsole::number(std::int64_t value) {
  char bytes[21];  // Includes space for the sign of INT64_MIN.
  const auto result = std::to_chars(bytes, bytes + sizeof(bytes), value);
  field({bytes, static_cast<std::size_t>(result.ptr - bytes)});
}

void HumanConsole::statusError(Status status) {
  field("[E]");
  append(" ");
  field(errToString(status.code));
  append(" detail ");
  number(status.detail);
  if (status.msg != nullptr && status.msg[0] != '\0') {
    append(" ");
    // Status text may be supplied by the adapter. Bound each service turn's output.
    std::size_t length = 0;
    while (length < 128 && status.msg[length] != '\0') {
      ++length;
    }
    append({status.msg, length});
  }
  if (status.code == Err::EXCEPTION) {
    append(" ");
    append(modbusExceptionToString(static_cast<std::uint8_t>(status.detail)));
  }
  append("\n");
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
    constexpr std::string_view PADDING = "                                     ";
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
  field(profileName(currentTarget().policy));
  append("\n");
}

void HumanConsole::dispatch(const char* const* words, std::size_t count) {
  const Command* command = findCommand(words[0]);
  if (command == nullptr) {
    error("Unknown command. Use help to list commands.");
    return;
  }
  if (count - 1 > command->maxArguments) {
    error("Too many arguments.");
    return;
  }
  auto next = currentTarget();
  switch (command->action) {
    case Action::Help: {
      if (count == 1) {
        help();
        return;
      }
      const Command* topic = findCommand(words[1]);
      if (topic == nullptr) {
        error("Unknown help topic.");
        return;
      }
      append(topic->name);
      append(topic->arguments);
      append("    ");
      append(topic->description);
      append("\n");
      info(topic->detail);
      info(topic->effects);
      if (topic->action == Action::Profile) {
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
    if (const Status status = applyTarget(next, command->action == Action::Host); !status) {
      statusError(status);
      return;
    }
  }
  append("[I] Model ");
  if (next.model) {
    field(*next.model == DeviceModel::SEN0591 ? "SEN0591" : "SEN0592");
  } else {
    field("unselected");
  }
  append(", profile ");
  field(profileName(next.policy));
  append(", address ");
  number(next.address);
  append(", ");
  number(next.baud);
  append(" baud 8N1\n");
  if (command->action == Action::Host) {
    append("[I] Supported baud rates (bits/s)");
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
