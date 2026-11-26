#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "HumanConsole.h"
#include "SchedulerFixture.h"

#include <SEN059x/Version.h>

namespace {
using namespace SEN059x::example;

struct Sink : ConsoleSink {
  std::size_t limit = std::string::npos;
  unsigned calls = 0;
  std::string output;

  std::size_t write(const char* bytes, std::size_t length) override {
    ++calls;
    const auto count = std::min(length, limit);
    output.append(bytes, count);
    return count;
  }
};

struct Terminal {
  scheduler_test::NoTxPort port;  // nowUs() stays at zero throughout the replay.
  Scheduler owner{port};
  Sink sink;
  HumanConsole console{
      sink, {"abc123", "clean", "native-fw", "1.2", "host-cpu", "test-board"}, owner};

  void turn() {
    const auto before = sink.calls;
    console.service();
    CHECK(sink.calls - before <= 1);
  }

  void flush() {
    for (unsigned turns = 0; console.outputPending(); ++turns) {
      CHECK(turns < 8192);
      turn();
    }
  }

  std::string send(std::string_view bytes) {
    sink.output.clear();
    for (const unsigned char byte : bytes) {
      for (unsigned turns = 0; !console.feed(byte); ++turns) {
        CHECK(turns < 8192);
        turn();
      }
    }
    flush();
    return sink.output;
  }
};

bool contains(const std::string& output, std::string_view field) {
  return output.find(field) != std::string::npos;
}

bool listsAlias(const std::string& help, std::string_view name, std::string_view alias) {
  const auto row = help.find(name);
  if (row == std::string::npos) {
    return false;
  }
  const auto afterName = row + name.size();
  return contains(help.substr(afterName, help.find('\n', afterName) - afterName), alias);
}

void registryAndIdentity() {
  Terminal t;
  t.flush();
  const auto startup = t.sink.output;
  CHECK(startup.find("=== SEN059x CLI Help ===") == 0);
  CHECK(startup.size() >= 2 && startup.substr(startup.size() - 2) == "> ");
  CHECK(!contains(startup, "\x1b"));
  CHECK(contains(startup, "Console") && listsAlias(startup, "help [command]", "?") &&
        listsAlias(startup, "version", "ver"));
  for (const char* usage : {"help [command]", "version", "color [on|off]", "verbose [on|off]"}) {
    CHECK(contains(startup, usage));
  }
  CHECK(t.send("help\r") == startup);
  CHECK(t.send("\n").empty());  // CRLF split by an output flush still closes a single line.
  CHECK(t.send("?\n") == startup);
  for (const char* name : {"help", "version", "color", "verbose"}) {
    const auto detail = t.send(std::string("help ") + name + "\n");
    CHECK(contains(detail, name) && !contains(detail, "[E]") && detail != startup);
  }
  const auto versionHelp = t.send("help version\n");
  CHECK(t.send("? ver\n") == versionHelp);
  const auto version = t.send("version\n");
  CHECK(t.send("ver\n") == version);
  for (const char* field :
       {"[I]", SEN059x::VERSION, "abc123", "clean", "native-fw", "1.2", "host-cpu", "test-board"}) {
    CHECK(contains(version, field));
  }
  CHECK(contains(version, "Schema 1"));
  for (const char* invalid : {"HELP\n", "version 1\n", "help missing\n", "help color extra\n"}) {
    CHECK(contains(t.send(invalid), "[E]"));
  }
  for (const char* toggle : {"color", "verbose"}) {
    const std::string name(toggle);
    const auto off = t.send(name + "\n");
    CHECK(contains(off, "off"));
    CHECK(contains(t.send(name + " on\n"), "[I]"));
    const auto on = t.send(name + "\n");
    CHECK(contains(on, "on") && on != off);
    if (name == "color") {
      CHECK(contains(t.send("version\n"), "\x1b[0m"));
    }
    CHECK(contains(t.send(name + " OFF\n"), "[E]"));
    CHECK(contains(t.send(name + " off extra\n"), "[E]"));
    CHECK(t.send(name + "\n") == on);
    CHECK(contains(t.send(name + " off\n"), "[I]"));
    CHECK(t.send(name + "\n") == off);
  }
  for (unsigned i = 0; i < 10; ++i) {
    t.turn();
  }
  CHECK(t.owner.physicalAttempts() == 0);
}

void lineLimitsAndRecovery() {
  Terminal t;
  t.flush();
  const auto version = t.send("version\n");
  CHECK(t.send("\t  ver\t\r\n") == version);
  CHECK(t.send("vex\br\n") == version);
  CHECK(t.send("versiox\x7fn\n") == version);
  CHECK(t.send(std::string(189, ' ') + "ver\n") == version);

  // Each poisoned line contains a valid command prefix or suffix. Only the following clean
  // line may dispatch, even if the user attempts to erase the offending byte first.
  for (const auto& line :
       {std::string(190, ' ') + "ver", std::string(193, ' ') + "ver", std::string("ver\0", 4),
        std::string("ver\x01\b"), std::string(1, static_cast<char>(0x80)) + "ver"}) {
    const auto rejected = t.send(line + "\r\n");
    CHECK(contains(rejected, "[E]") && !contains(rejected, "abc123"));
    CHECK(t.send("ver\n") == version);
  }

  // At the token limits the command is syntactically valid input to the dispatcher but the
  // topic/arity is unsupported. One more byte/token is rejected by the editor before dispatch.
  const auto unknownTopic = t.send("help " + std::string(64, 'x') + "\n");
  const auto longToken = t.send("help " + std::string(65, 'x') + "\n");
  CHECK(contains(unknownTopic, "[E]") && contains(longToken, "[E]") && unknownTopic != longToken);
  const auto wrongArity = t.send("color on a b c d e f g h\n");
  const auto extraToken = t.send("color on a b c d e f g h i\n");
  CHECK(contains(wrongArity, "[E]") && contains(extraToken, "[E]") && wrongArity != extraToken);
  CHECK(contains(t.send("color\n"), "off"));
  CHECK(t.send("ver\n") == version);
}

void stalledSink() {
  Terminal baseline;
  baseline.flush();
  const auto startup = baseline.sink.output;
  const auto replies = baseline.send("help\n? ver\nversion\n");

  Terminal slow;
  slow.sink.limit = 0;
  for (unsigned i = 0; i < 10; ++i) {
    slow.turn();
    CHECK(slow.console.outputPending() && slow.sink.output.empty());
  }
  slow.sink.limit = 3;
  slow.turn();
  CHECK(!slow.sink.output.empty() && slow.console.outputPending());
  const auto prefix = slow.sink.output;
  slow.sink.limit = 0;
  slow.turn();
  CHECK(slow.sink.output == prefix && slow.console.outputPending());
  slow.sink.limit = 1;
  slow.flush();
  CHECK(slow.sink.output == startup);
  CHECK(slow.send("help\n? ver\nversion\n") == replies);
  CHECK(slow.owner.physicalAttempts() == 0);
}
}  // namespace

int main() {
  registryAndIdentity();
  lineLimitsAndRecovery();
  stalledSink();
}
