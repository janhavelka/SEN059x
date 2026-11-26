#include "HumanConsole.h"

#include <SEN059x/Version.h>

namespace SEN059x::example {
namespace {
enum class Action { Help, Version, Color, Verbose };

struct Command {
  Action action;
  std::string_view group;
  std::string_view name;
  std::string_view arguments;  // Includes the leading separator for nonempty arguments.
  std::string_view alias;
  std::string_view description;
  std::string_view detail;
  std::string_view effects;
  std::size_t maxArguments;  // All commands in this group also accept zero arguments.
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
};

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
    constexpr std::string_view PADDING = "                    ";
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
  }
}

}  // namespace SEN059x::example
