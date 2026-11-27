#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "HumanConsoleFixture.h"

#include <SEN059x/Version.h>

namespace {
using namespace SEN059x::example;

using namespace console_test;

bool listsAlias(const std::string& help, std::string_view name, std::string_view alias) {
  const auto row = help.find(name);
  if (row == std::string::npos) {
    return false;
  }
  const auto afterName = row + name.size();
  return contains(help.substr(afterName, help.find('\n', afterName) - afterName), alias);
}

void advertisedCommands() {
  Terminal t;
  t.flush();
  const auto help = t.sink.output;
  CHECK(contains(t.send("model SEN0591\n"), "SEN0591"));
  // Read exactly the canonical rows in each group. Aliases and detailed help are checked
  // separately. Leave the rendering order within a group up to the command registry.
  const auto group = [&](const char* heading, std::vector<std::string> expected) {
    const std::string title = std::string("\n") + heading + "\n";
    const auto start = help.find(title);
    CHECK(start != std::string::npos);
    auto row = start + title.size();
    while (help.compare(row, 2, "  ") == 0) {
      const auto end = help.find(' ', row + 2);
      CHECK(end != std::string::npos);
      const auto name = help.substr(row + 2, end - row - 2);
      const auto entry = std::find(expected.begin(), expected.end(), name);
      CHECK(entry != expected.end());
      expected.erase(entry);
      const auto detail = t.send("help " + name + "\n");
      CHECK(detail.find(name) == 0 && !contains(detail, "[E]"));
      if (contains(detail, "No sensor transfer") && name != "job" && name != "result") {
        CHECK(!contains(t.send(name + (name == "useaddr" ? " 1\n" : "\n")), "[E]"));
      }
      const auto newline = help.find('\n', end);
      CHECK(newline != std::string::npos);
      row = newline + 1;
    }
    CHECK(expected.empty());
  };
  group("Console", {"help", "version", "color", "verbose"});
  group("Target", {"model", "profile", "useaddr", "host"});
  group("Cached", {"status", "drv", "health", "stats", "settings", "latest"});
  group("Measurements", {"read", "watch"});
  group("Configuration", {"config", "readbaud", "readreg"});
  group("Block diagnostic", {"readregs"});
  group("Commissioning", {"maintenance"});
  group("Jobs", {"job", "result", "cancel"});
  group("Diagnostics", {"diagnose", "recover", "selftest"});
  for (const auto& key : SETTING_KEYS) {
    CHECK(contains(t.send("help config read " + std::string(key.key) + "\n"), "register 0x02"));
  }
  CHECK(contains(t.send("help config read threshold\n"), "SEN0592 switch threshold (mm) 30..5000"));
  CHECK(contains(t.send("help config read made-up\n"), "[E]"));
  CHECK(contains(t.send("help job status\n"), "deadline"));
  CHECK(t.owner.physicalAttempts() == 0);
}

void offlineSelftest() {
  Terminal t;
  t.flush();
  const auto result = t.send("selftest\n");
  for (const char* name : {"read_processed", "read_realtime", "read_temperature", "set_address_5",
                           "read_baud", "set_baud_9600"}) {
    CHECK(contains(result, std::string("[I] ") + name + "\n"));
  }
  CHECK(contains(result, "Passed 6/6 offline frame vectors") && !contains(result, "[E]"));
  CHECK(!t.owner.selectedTarget(0));
  CHECK(contains(t.send("selftest extra\n"), "[E]"));
  CHECK(contains(t.send("model SEN0592\n"), "SEN0592"));
  CHECK(t.send("selftest\n") == result);
  CHECK(t.owner.physicalAttempts() == 0 && !t.owner.endpointTrusted(0));
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
  CHECK(contains(host, "Host UART unknown"));
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

std::string modelOnAttach(Scheduler& owner) {
  Sink sink;
  HumanConsole console(sink, {}, owner);
  const auto flush = [&] {
    for (unsigned turns = 0; console.outputPending(); ++turns) {
      CHECK(turns < 8192);
      console.service();
    }
  };
  flush();
  CHECK(contains(sink.output, "=== SEN059x CLI Help ==="));
  sink.output.clear();
  for (const char byte : std::string_view("model\n")) {
    CHECK(console.feed(byte));
  }
  flush();
  return sink.output;
}

void prepareAndAttachTarget() {
  Terminal t;
  t.flush();
  CHECK(contains(t.send("maintenance on isolated\n"), "DISALLOWED"));
  CHECK(contains(t.send("maintenance\n"), "Maintenance off"));
  CHECK(!contains(t.send("profile dfrobot-isolated\n"), "[E]"));
  CHECK(!contains(t.send("useaddr 249\n"), "[E]"));
  CHECK(!contains(t.send("host baud 19200\n"), "[E]"));
  CHECK(t.port.baud == 19200 && !t.owner.endpoint(0));
  CHECK(contains(t.send("model\n"), "unselected"));
  CHECK(contains(modelOnAttach(t.owner),
                 "Model unselected, profile dfrobot-isolated, address 249, 19200 baud"));
  CHECK(contains(t.send("model SEN0592\n"), "SEN0592"));
  CHECK(t.owner.endpoint(0) && t.owner.endpoint(0)->address == 249 &&
        t.owner.endpoint(0)->bps == 19200);
  const auto generation = [&] {
    return t.owner.snapshot(0, SEN059x::MeasurementKind::Temperature, 0, 0).bindingGeneration;
  };
  const auto boundGeneration = generation();
  CHECK(contains(t.send("maintenance on isolated\n"), "Maintenance on isolated"));
  CHECK(contains(modelOnAttach(t.owner), "SEN0592"));
  CHECK(contains(t.send("maintenance\n"), "Maintenance off"));
  for (const char* command :
       {"model SEN0592\n", "profile dfrobot-isolated\n", "useaddr 249\n", "host baud 19200\n"}) {
    CHECK(!contains(t.send(command), "[E]") && generation() == boundGeneration);
  }

  // The application changes the binding before another operator attaches to this owner.
  CHECK(t.owner.bind(0, SEN059x::DeviceModel::SEN0591, 7, 57600));
  const auto appGeneration = generation();
  CHECK(contains(modelOnAttach(t.owner), "Model SEN0591, profile standard, address 7, 57600 baud"));
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

void cachedQueryCommands() {
  Terminal t;
  t.flush();
  CHECK(contains(t.sink.output, "Cached") &&
        listsAlias(t.sink.output, "latest [processed|realtime|temperature|echo]", "sample"));
  const auto detail = t.send("help latest\n");
  CHECK(contains(detail, "latest") && !contains(detail, "[E]") && t.send("? sample\n") == detail);
  CHECK(contains(t.send("model SEN0592\n"), "SEN0592"));
  const auto all = t.send("latest\n");
  CHECK(t.send("sample\n") == all);
  for (const char* kind : {"processed", "realtime", "temperature", "echo"}) {
    const auto filtered = t.send(std::string("sample ") + kind + "\n");
    CHECK(contains(filtered, kind) && contains(filtered, "never sampled") &&
          !contains(filtered, "[E]"));
    CHECK(contains(all, kind));
  }
  for (const char* invalid : {"latest therm\n", "sample Echo\n", "latest processed echo\n"}) {
    CHECK(contains(t.send(invalid), "[E]"));
  }
  CHECK(t.owner.physicalAttempts() == 0);
}

void passiveStatus() {
  using namespace SEN059x;
  TestTerminal<scheduler_test::Port> t;
  t.flush();
  CHECK(contains(t.sink.output, "  status "));
  const auto help = t.send("help status\n");
  CHECK(contains(help, "status") && contains(help, "No sensor transfer"));
  CHECK(contains(t.send("status extra\n"), "[E]"));
  const auto unbound = t.send("status\n");
  CHECK(contains(unbound, "Model unselected, profile standard, address 1, 115200 baud 8N1"));
  CHECK(contains(unbound, "Owner idle, current operation none"));
  CHECK(!contains(t.send("profile dfrobot-isolated\n"), "[E]"));
  CHECK(!contains(t.send("model SEN0592\n"), "[E]"));
  const auto selected = t.send("status\n");
  CHECK(contains(selected, "Model SEN0592, profile dfrobot-isolated, address 1, 115200 baud 8N1"));
  CHECK(contains(selected, "Binding generation 1, endpoint unconfirmed"));
  CHECK(contains(selected, "Selected endpoint address 1, 115200 baud"));
  CHECK(t.port.writes.empty() && t.owner.physicalAttempts() == 0);

  // Dispatch sees the queued read. Output service starts that one previously admitted transfer.
  std::uint64_t read = 0;
  CHECK(t.owner.submitSettingRead(0, Setting::Address, 71, 0, 100000, read));
  const auto queued = t.send("status\n");
  CHECK(contains(queued, "Owner queued, current operation 71"));
  CHECK(contains(queued, "address 1, register 0x0200, deadline 100000 us"));
  const auto active = t.send("status\n");
  CHECK(contains(active, "Owner active, current operation 71"));
  CHECK(contains(active, "address 1, register 0x0200, deadline 100000 us"));
  CHECK(t.port.writes == (std::vector<std::vector<std::uint8_t>>{{1, 3, 2, 0, 0, 1, 0x85, 0xB2}}));
  t.port.time = 1000;
  t.turn();
  t.port.time = 20000;
  t.port.deliver({1, 3, 2, 0, 1, 0x79, 0x84});
  t.turn();
  const auto observed = t.send("status\n");
  CHECK(contains(observed, "Binding generation 1, endpoint trusted"));
  CHECK(contains(observed, "Owner idle, current operation none"));
  ScheduledResult result;
  CHECK(t.owner.take(read, result) && result.status && result.settingObservation);
  CHECK(!t.owner.take(read, result));

  // A transmitted address change with a lost acknowledgement leaves two possible endpoints.
  std::uint64_t write = 0;
  CHECK(t.owner.grantIsolatedMaintenance(0, 17));
  CHECK(t.owner.submitWrite(0, Setting::Address, {SettingUnit::Address, 5}, 17, 72, 0, 120000,
                            write));
  t.turn();
  t.port.time = 21000;
  t.turn();
  t.port.time = 120000;
  t.turn();
  const auto recovery = t.send("status\n");
  CHECK(contains(recovery, "Owner recovery, current operation none, retired operation 72"));
  CHECK(contains(recovery, "Binding generation 1, endpoint unconfirmed"));
  CHECK(contains(recovery, "Last-known endpoint address 1, 115200 baud"));
  CHECK(contains(recovery, "Intended endpoint address 5, 115200 baud"));
  CHECK(t.port.writes.size() == 2 && t.owner.physicalAttempts() == 2);
  CHECK(t.owner.take(write, result) && !result.status && result.write && !result.write->verified);
  CHECK(!t.owner.take(write, result));
  CHECK(t.send("status\n") == recovery);  // Releasing a ticket does not settle the wire.
  t.port.isolationVerified = true;
  CHECK(t.owner.recoverIsolated());
  const auto settled = t.send("status\n");
  CHECK(contains(settled, "Owner idle, current operation none"));
  CHECK(contains(settled, "Intended endpoint address 5, 115200 baud"));
  CHECK(t.port.writes.size() == 2 && t.owner.physicalAttempts() == 2);

  // Selecting either candidate locally leaves the uncertain write and its history intact.
  for (const char* change : {"useaddr 5\n", "useaddr 1\n", "useaddr 5\n"}) {
    CHECK(!contains(t.send(change), "[E]"));
    const auto candidates = t.send("status\n");
    CHECK(contains(candidates, "Last-known endpoint address 1, 115200 baud"));
    CHECK(contains(candidates, "Intended endpoint address 5, 115200 baud"));
    CHECK(contains(t.send("settings\n"),
                   "desired 0x0005, observed 0x0001 (1) at 20 ms, address 1 115200 baud, "
                   "historical, unconfirmed"));
    CHECK(!t.owner.submit(0, MeasurementKind::RealTimeDistance, 73, 0, 200000, false, read));
    CHECK(t.port.writes.size() == 2);
  }
  CHECK(t.owner.submitSettingRead(0, Setting::Address, 74, 0, 200000, read));
  t.turn();
  CHECK(t.port.writes.back() == (std::vector<std::uint8_t>{5, 3, 2, 0, 0, 1, 0x84, 0x36}));
  t.port.time = 121000;
  t.turn();
  t.port.time = 125000;
  t.port.deliver({5, 3, 2, 0, 5, 0x89, 0x87});
  t.turn();
  CHECK(t.owner.take(read, result) && result.status);
  const auto confirmed = t.send("status\n");
  CHECK(!contains(confirmed, "Intended endpoint") && contains(confirmed, "endpoint trusted"));
  CHECK(contains(t.send("settings\n"),
                 "desired 0x0005, observed 0x0005 (5) at 125 ms, address 5 115200 baud, trusted"));
}

void cachedObservations() {
  using namespace SEN059x;
  TestTerminal<scheduler_test::Port> t;
  t.flush();
  CHECK(contains(t.send("model SEN0591\n"), "SEN0591"));
  struct Case {
    MeasurementKind kind;
    const char* command;
    const char* source;
    const char* word;
    const char* value;
    std::vector<std::uint8_t> reply;
  };
  // The distance words come from manufacturer examples 1 and 2. Temperature and echo
  // are synthetic samples, including a half-degree to exercise decimal formatting.
  const Case cases[] = {
      {MeasurementKind::ProcessedDistance,
       "latest processed\n",
       "processed 0x0100",
       "raw 0x02F2",
       "754 mm",
       {1, 3, 2, 2, 0xF2, 0x38, 0xA1}},
      {MeasurementKind::RealTimeDistance,
       "latest realtime\n",
       "realtime 0x0101",
       "raw 0x02EF",
       "751 mm",
       {1, 3, 2, 2, 0xEF, 0xF8, 0xA8}},
      {MeasurementKind::Temperature,
       "sample temperature\n",
       "temperature 0x0102",
       "raw 0x0131",
       "30.5 \xC2\xB0"
       "C",
       {1, 3, 2, 1, 0x31, 0x78, 0}},
      {MeasurementKind::EchoTime,
       "latest echo\n",
       "echo 0x010A",
       "raw 0x0D7A",
       "3450 \xC2\xB5s",
       {1, 3, 2, 0x0D, 0x7A, 0x3D, 0x37}},
  };
  std::uint64_t sequence = 0;
  ScheduledResult result;
  for (const auto& c : cases) {
    std::uint64_t ticket = 0;
    ++sequence;
    CHECK(t.owner.submit(0, c.kind, sequence, 0, 1000000, false, ticket));
    t.turn();
    t.port.time += 695;
    t.turn();
    t.port.time = sequence * 10000;
    t.port.deliver(c.reply);
    t.turn();
    const auto report = t.send(c.command);
    CHECK(contains(report, "SEN0591 address 1, binding generation 1"));
    CHECK(contains(report, c.source) && contains(report, c.word) && contains(report, c.value));
    CHECK(contains(report, "sample generation " + std::to_string(sequence)));
    CHECK(contains(report, "observed " + std::to_string(sequence * 10) + " ms"));
    CHECK(contains(report, "age 0 ms") && contains(report, "valid, fresh"));
    for (const auto& other : cases) {
      CHECK(contains(report, other.source) == (&other == &c));
    }
    CHECK(t.owner.take(ticket, result) && result.status && result.ticket == ticket);
  }
  const auto all = t.send("latest\n");
  for (const auto& c : cases) {
    CHECK(contains(all, c.source) && contains(all, c.value));
  }
  t.port.time = 1030000;
  const auto boundary = t.send("latest temperature\n");
  CHECK(contains(boundary, "age 1000 ms") && contains(boundary, "valid, fresh"));

  // Query the previous sample while a second temperature read is awaiting its reply.
  // Its ticket must still complete, and the historical good sample must survive.
  std::uint64_t ticket = 0;
  CHECK(t.owner.submit(0, MeasurementKind::Temperature, 5, 0, 2000000, false, ticket));
  t.turn();
  t.port.time += 1000;
  t.turn();
  const auto busy = t.send("sample temperature\n");
  CHECK(!t.owner.settled() && contains(busy, "age 1001 ms") && contains(busy, "valid, stale"));
  CHECK(contains(busy, "sample generation 3") && contains(busy,
                                                          "30.5 \xC2\xB0"
                                                          "C"));
  CHECK(!t.owner.take(ticket, result) && t.port.writes.size() == 5);
  t.port.time += 9000;
  t.port.deliver({1, 3, 2, 0xFF, 0xFF, 0xB9, 0xF4});
  t.turn();
  const auto ambiguous = t.send("latest temperature\n");
  for (const char* field :
       {"raw 0xFFFF", "invalid, ambiguous temperature", "sample generation 5", "previous usable",
        "30.5 \xC2\xB0"
        "C",
        "age 1010 ms", "valid, stale"}) {
    CHECK(contains(ambiguous, field));
  }
  CHECK(t.owner.take(ticket, result) && result.ticket == ticket);
  CHECK(!t.owner.take(ticket, result));
  CHECK(t.owner.physicalAttempts() == 5 && t.port.writes.size() == 5);
}
}  // namespace

int main() {
  advertisedCommands();
  offlineSelftest();
  registryAndIdentity();
  lineLimitsAndRecovery();
  stalledSink();
  localTargetCommands();
  prepareAndAttachTarget();
  targetChangeDuringWorkAndAfterFailedApply();
  cachedQueryCommands();
  passiveStatus();
  cachedObservations();
}
