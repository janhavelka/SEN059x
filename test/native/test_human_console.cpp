#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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

template <class Port>
struct TestTerminal {
  Port port;
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
using Terminal = TestTerminal<scheduler_test::NoTxPort>;

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

void localTargetCommands() {
  using SEN059x::MeasurementKind;
  Terminal t;
  t.port.unsupportedBaud = 14400;
  t.flush();
  const auto help = t.sink.output;
  CHECK(contains(help, "Target"));
  for (const char* name : {"model", "profile", "useaddr", "host"}) {
    CHECK(contains(help, name));
    const auto topic = t.send(std::string("help ") + name + "\n");
    CHECK(contains(topic, name) && !contains(topic, "[E]"));
  }
  CHECK(contains(t.send("model\n"), "unselected"));
  CHECK(contains(t.send("profile\n"), "standard"));
  const auto host = t.send("host\n");
  for (const char* field : {"8N1", "4800", "9600", "19200", "38400", "57600", "76800", "115200"}) {
    CHECK(contains(host, field));
  }
  CHECK(!contains(host, "14400") && !t.owner.endpoint(0));

  CHECK(contains(t.send("model SEN0591\n"), "SEN0591"));
  CHECK(t.owner.endpoint(0) && t.owner.endpoint(0)->address == 1 &&
        t.owner.endpoint(0)->bps == 115200);
  const auto generation = [&] {
    return t.owner.snapshot(0, MeasurementKind::Temperature, 0, 0).bindingGeneration;
  };
  auto previous = generation();
  CHECK(previous != 0);
  for (const char* invalid :
       {"model sen0591", "model 1", "model SEN0592 extra", "profile DFRobot-Isolated", "useaddr",
        "useaddr 0", "useaddr 248", "useaddr 257", "useaddr -1", "useaddr 2x", "useaddr 0x02",
        "useaddr 2 3", "host baud", "host 9600", "host baud -1", "host baud 14400",
        "host baud 4294976896", "host baud 9600x", "host baud 9600 1"}) {
    CHECK(contains(t.send(std::string(invalid) + "\n"), "[E]"));
    CHECK(generation() == previous);
  }
  for (const char* change : {"useaddr 247", "profile dfrobot-isolated", "useaddr 254",
                             "host baud 9600", "model SEN0592"}) {
    CHECK(!contains(t.send(std::string(change) + "\n"), "[E]"));
    CHECK(generation() > previous);
    previous = generation();
  }
  CHECK(contains(t.send("model\n"), "SEN0592"));
  CHECK(t.owner.endpoint(0)->address == 254 && t.owner.endpoint(0)->bps == 9600 &&
        t.port.baud == 9600);
  CHECK(contains(t.send("useaddr 255\n"), "[E]"));
  CHECK(contains(t.send("profile standard\n"), "[E]") && generation() == previous);
  CHECK(contains(t.send("profile\n"), "dfrobot-isolated"));
  CHECK(!contains(t.send("useaddr 1\n"), "[E]"));
  CHECK(contains(t.send("profile standard\n"), "standard"));
  CHECK(t.owner.endpoint(0)->address == 1 && t.owner.physicalAttempts() == 0);
}

void prepareAndAttachTarget() {
  Terminal t;
  t.flush();
  CHECK(!contains(t.send("profile dfrobot-isolated\n"), "[E]"));
  CHECK(!contains(t.send("useaddr 249\n"), "[E]"));
  CHECK(!contains(t.send("host baud 19200\n"), "[E]"));
  CHECK(t.port.baud == 19200 && !t.owner.endpoint(0));
  CHECK(contains(t.send("model\n"), "unselected"));
  CHECK(contains(t.send("model SEN0592\n"), "SEN0592"));
  CHECK(t.owner.endpoint(0) && t.owner.endpoint(0)->address == 249 &&
        t.owner.endpoint(0)->bps == 19200);
  const auto generation = [&] {
    return t.owner.snapshot(0, SEN059x::MeasurementKind::Temperature, 0, 0).bindingGeneration;
  };
  const auto boundGeneration = generation();
  for (const char* command :
       {"model SEN0592\n", "profile dfrobot-isolated\n", "useaddr 249\n", "host baud 19200\n"}) {
    CHECK(!contains(t.send(command), "[E]") && generation() == boundGeneration);
  }

  // The application changes the binding before another operator attaches to this owner.
  CHECK(t.owner.bind(0, SEN059x::DeviceModel::SEN0591, 7, 57600));
  const auto appGeneration = generation();
  Sink secondSink;
  HumanConsole attached(secondSink, {}, t.owner);
  const auto drain = [&] {
    for (unsigned turns = 0; attached.outputPending(); ++turns) {
      CHECK(turns < 8192);
      attached.service();
    }
  };
  drain();
  CHECK(contains(secondSink.output, "=== SEN059x CLI Help ==="));
  secondSink.output.clear();
  for (const char byte : std::string_view("model\n")) {
    CHECK(attached.feed(byte));
  }
  drain();
  for (const char* field : {"SEN0591", "standard", "address 7", "57600"}) {
    CHECK(contains(secondSink.output, field));
  }
  CHECK(generation() == appGeneration && t.owner.physicalAttempts() == 0);
}

void targetChangeDuringWorkAndAfterFailedApply() {
  using namespace SEN059x;
  TestTerminal<scheduler_test::Port> t;
  t.flush();
  CHECK(!contains(t.send("model SEN0591\n"), "[E]"));
  const auto sample = [&] { return t.owner.snapshot(0, MeasurementKind::Temperature, 10, 100); };
  const auto generation = sample().bindingGeneration;
  std::uint64_t seed = 0;
  CHECK(t.owner.submit(0, MeasurementKind::Temperature, 1, 0, 1000000, false, seed));
  // The terminator dispatches while the request is queued. Only the subsequent output
  // flush services the owner and transmits the previously admitted temperature request.
  CHECK(t.owner.settled() && t.port.writes.empty());
  CHECK(contains(t.send("model SEN0592\n"), "[E]"));
  CHECK(t.port.writes == (std::vector<std::vector<std::uint8_t>>{{1, 3, 1, 2, 0, 1, 0x24, 0x36}}));
  CHECK(!t.owner.settled());
  CHECK(contains(t.send("useaddr 2\n"), "[E]") && t.owner.endpoint(0)->address == 1);
  CHECK(sample().bindingGeneration == generation);
  t.port.time = 695;
  t.turn();
  t.port.time = 10000;
  t.port.deliver({1, 3, 2, 0, 0xC8, 0xB9, 0xD2});  // 20.0 degrees Celsius
  t.turn();
  CHECK(t.owner.settled() && sample().usable.measurement.value == 20);
  const auto before = sample();
  const bool trustedBefore = t.owner.endpointTrusted(0);
  const auto hostBefore = t.send("host\n");
  t.port.configureStatus = {Err::IO, -23, "Adapter rate change failed"};
  const auto failure = t.send("host baud 9600\n");
  CHECK(contains(failure, "[E]") && contains(failure, "-23") &&
        contains(failure, "Adapter rate change failed"));
  CHECK(t.send("host\n") == hostBefore && t.owner.endpoint(0)->bps == 115200 &&
        t.port.baud == 115200 && t.owner.endpointTrusted(0) == trustedBefore);
  const auto after = sample();
  CHECK(after.bindingGeneration == generation &&
        after.raw.sampleGeneration == before.raw.sampleGeneration);
  CHECK(after.usable.measurement.rawWord == 200 && after.usable.measurement.completedMs == 10);
  ScheduledResult result;
  CHECK(t.owner.take(seed, result) && result.status &&
        result.measurement.model == DeviceModel::SEN0591);

  t.port.configureStatus = Ok();
  std::uint64_t retired = 0;
  CHECK(t.owner.submit(0, MeasurementKind::Temperature, 2, 0, 1000000, false, retired));
  t.turn();
  t.port.time = 10695;
  t.turn();
  CHECK(t.owner.cancel(retired) && !t.owner.settled());
  CHECK(contains(t.send("profile dfrobot-isolated\n"), "[E]"));
  CHECK(contains(t.send("profile\n"), "standard") && sample().bindingGeneration == generation);
  t.port.deliver({1, 3, 2, 0, 0xC8, 0xB9, 0xD2});
  CHECK(t.owner.recover());
  CHECK(!contains(t.send("host baud 9600\n"), "[E]"));
  CHECK(t.port.baud == 9600 && sample().bindingGeneration > generation);
  CHECK(sample().usable.freshness == Freshness::NeverSampled && !t.owner.endpointTrusted(0));
  CHECK(t.owner.take(retired, result) && result.status.code == Err::ABORTED);
  CHECK(t.owner.physicalAttempts() == 2 && t.port.writes.size() == 2);
}
}  // namespace

int main() {
  registryAndIdentity();
  lineLimitsAndRecovery();
  stalledSink();
  localTargetCommands();
  prepareAndAttachTarget();
  targetChangeDuringWorkAndAfterFailedApply();
}
