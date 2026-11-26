#include "HumanConsoleFixture.h"

namespace {
using namespace SEN059x;
using namespace SEN059x::example;
using console_test::contains;
using console_test::TestTerminal;

void emptyReports() {
  console_test::Terminal t;
  t.flush();
  for (std::string name : {"health", "stats", "settings"}) {
    CHECK(contains(t.sink.output, "  " + name + " "));
  }
  for (std::string name : {"health", "stats", "settings"}) {
    CHECK(contains(t.send("help " + name + "\n"), "No sensor transfer"));
    CHECK(contains(t.send(name + " extra\n"), "[E]"));
  }
  const auto health = t.send("health\n");
  for (const char* expected :
       {"state UNINIT", "initialized false", "consecutiveFailures 0", "totalFailures 0",
        "totalSuccess 0", "lastOkMs unknown", "lastErrorMs unknown", "lastOkTimeValid false",
        "lastErrorTimeValid false", "lastError OK detail 0"}) {
    CHECK(contains(health, expected));
  }
  const auto stats = t.send("stats\n");
  for (const char* expected : {"Shared bus", "successCount 0", "failCount 0", "physicalAttempts 0",
                               "retryCount 0", "crcErrors 0", "timeouts 0", "exceptions 0",
                               "frameErrors 0", "lastError OK", "lastErrorDetail 0", "lastErrorMsg",
                               "lastErrorTimestampMs unknown", "lastSuccessTimestampMs unknown"}) {
    CHECK(contains(stats, expected));
  }
  const auto settings = t.send("settings\n");
  for (const char* key : {"address 0x0200", "baud 0x0201", "polarity 0x0205", "threshold 0x0206",
                          "angle 0x0208", "uart-unit 0x0209", "noise 0x021A", "range 0x021F"}) {
    const auto row = settings.find(key);
    CHECK(row != std::string::npos);
    CHECK(contains(settings.substr(row, settings.find('\n', row) - row),
                   "desired none, observed none, unconfirmed"));
  }
  CHECK(t.owner.physicalAttempts() == 0 && t.port.writes.empty());
}

struct FailingRxPort : scheduler_test::Port {
  Status rxStatus;
  RxBatch read(std::uint8_t* bytes, std::size_t capacity) override {
    auto batch = Port::read(bytes, capacity);
    batch.uartStatus = rxStatus;
    return batch;
  }
};

// Admit one independent FC03 and leave it waiting for the device's response.
template <class Port>
std::uint64_t startRead(TestTerminal<Port>& t, Setting setting) {
  std::uint64_t ticket = 0;
  CHECK(t.owner.submitSettingRead(0, setting, t.port.writes.size() + 1, 0, t.port.time + 100000,
                                  ticket));
  t.turn();
  t.port.time += 695;
  t.turn();
  CHECK(!t.port.transmitting && !t.owner.settled());
  return ticket;
}

void healthAndTransport() {
  TestTerminal<FailingRxPort> t;
  t.flush();
  CHECK(contains(t.send("model SEN0591\n"), "SEN0591"));
  auto ticket = startRead(t, Setting::Address);
  // The fake closes this valid frame within monotonic millisecond zero.
  t.port.deliver({1, 3, 2, 0, 1, 0x79, 0x84});
  t.turn();
  ScheduledResult result;
  CHECK(t.owner.take(ticket, result) && result.status);
  CHECK(contains(t.send("health\n"), "lastOkMs 0\n"));
  CHECK(contains(t.send("stats\n"), "lastSuccessTimestampMs 0\n"));

  ticket = startRead(t, Setting::Angle);
  const auto busy = t.send("health\n");
  CHECK(contains(busy, "state READY") && contains(busy, "totalSuccess 1"));
  CHECK(contains(t.send("stats\n"), "failCount 0"));
  CHECK(contains(t.send("settings\n"), "observed 0x0001"));
  CHECK(!t.owner.take(ticket, result) && t.port.writes.size() == 2);
  t.port.deliver({1, 0x83, 2, 0xC0, 0xF1});
  t.turn();
  const auto exception = t.send("stats\n");
  for (const char* expected :
       {"exceptions 1", "failCount 1", "successCount 1", "lastError EXCEPTION", "lastErrorDetail 2",
        "0x02", "Illegal Data Address"}) {
    CHECK(contains(exception, expected));
  }
  CHECK(contains(t.send("health\n"), "totalSuccess 2"));
  CHECK(t.owner.take(ticket, result) && result.status.code == Err::EXCEPTION);

  ticket = startRead(t, Setting::Angle);
  t.port.time = 17000;
  t.port.rxStatus = {Err::IO, -59, "receiver failed"};
  t.turn();
  const auto failed = t.send("health\n");
  for (const char* expected :
       {"state DEGRADED", "initialized true", "consecutiveFailures 1", "totalFailures 1",
        "totalSuccess 2", "lastErrorTimeValid true", "lastOkTimeValid true", "lastErrorMs 17",
        "lastError IO detail -59", "receiver failed"}) {
    CHECK(contains(failed, expected));
  }
  const auto stats = t.send("stats\n");
  for (const char* expected :
       {"successCount 1", "failCount 2", "physicalAttempts 3", "lastError IO",
        "lastErrorDetail -59", "lastErrorMsg receiver failed", "lastErrorTimestampMs 17"}) {
    CHECK(contains(stats, expected));
  }
  CHECK(t.owner.take(ticket, result) && result.status.code == Err::IO);
  CHECK(t.port.writes.size() == 3);
}

void configurationHistory() {
  TestTerminal<scheduler_test::Port> t;
  t.flush();
  CHECK(contains(t.send("model SEN0592\n"), "SEN0592"));
  // Exercise a full report with both desired and observed values on all eight rows.
  // The longest table-2 labels are from the 5 m SKU. Begin at a ten-digit millisecond tick.
  t.port.time = 3000000000000ULL;
  struct Case {
    Setting setting;
    SettingValue desired;
    std::vector<std::uint8_t> reply;
    const char* description;
  };
  const Case cases[] = {
      {Setting::Address, {SettingUnit::Address, 1}, {1, 3, 2, 0, 1, 0x79, 0x84}, "(1) at"},
      {Setting::Baud, {SettingUnit::BaudCode, 9}, {1, 3, 2, 0, 9, 0x78, 0x42}, "115200 baud)"},
      {Setting::SwitchPolarity,
       {SettingUnit::PolarityCode, 0},
       {1, 3, 2, 0, 1, 0x79, 0x84},
       "high below threshold"},
      {Setting::SwitchThreshold,
       {SettingUnit::Millimetres, 750},
       {1, 3, 2, 0, 100, 0xB9, 0xAF},
       "100 mm"},
      {Setting::Angle,
       {SettingUnit::AngleLevel, 4},
       {1, 3, 2, 0, 4, 0xB9, 0x87},
       "65 deg dual horizontal, 80 deg dual vertical"},
      {Setting::UartUnit, {SettingUnit::UartUnitCode, 0}, {1, 3, 2, 0, 1, 0x79, 0x84}, "(us) at"},
      {Setting::Noise,
       {SettingUnit::NoiseLevel, 5},
       {1, 3, 2, 0, 5, 0x78, 0x47},
       "environmental interference"},
      {Setting::Range,
       {SettingUnit::RangeLevel, 5},
       {1, 3, 2, 0, 5, 0x78, 0x47},
       "about 500 cm, rated 5 m"},
  };
  ScheduledResult result;
  for (const auto& c : cases) {
    const auto read = startRead(t, c.setting);
    t.port.deliver(c.reply);
    t.turn();
    CHECK(t.owner.take(read, result) && result.status);
    // Proven zero TX preserves trust in the observed word. The desired word records admission.
    CHECK(t.owner.grantIsolatedMaintenance(0, 17));
    std::uint64_t write = 0;
    CHECK(t.owner.submitWrite(0, c.setting, c.desired, 17, 23, 0, t.port.time + 100000, write));
    CHECK(t.owner.cancel(write) && t.owner.take(write, result));
  }
  const auto populated = t.send("settings\n");
  for (const auto& c : cases) {
    CHECK(contains(populated, c.description));
  }
  CHECK(!contains(populated, "desired none") && !contains(populated, "unconfirmed"));
  CHECK(contains(populated, "threshold 0x0206 (switch-mode) desired 0x02EE, observed 0x0064"));
  CHECK(contains(populated, "automatic/controlled UART output"));
  t.send("color on\n");
  const auto colored = t.send("settings\n");
  CHECK(!contains(colored, "[E]") && contains(colored, "range") && contains(colored, "rated 5 m"));
  t.send("color off\n");

  ConfigurationOperation preview;
  CHECK(
      t.owner.previewWrite(0, Setting::SwitchThreshold, {SettingUnit::Millimetres, 800}, preview));
  CHECK(t.send("settings\n") == populated);
  CHECK(t.owner.grantIsolatedMaintenance(0, 17));
  std::uint64_t write = 0;
  const auto deadline = t.port.time + 50000;
  CHECK(t.owner.submitWrite(0, Setting::SwitchThreshold, {SettingUnit::Millimetres, 800}, 17, 24, 0,
                            deadline, write));
  t.turn();
  t.port.time += 695;
  t.turn();
  t.port.time = deadline;
  t.turn();
  const auto lost = t.send("settings\n");
  CHECK(contains(lost,
                 "threshold 0x0206 (switch-mode) desired 0x0320, observed 0x0064 (100 mm) "
                 "at 3000000002 ms, address 1 115200 baud, historical, unconfirmed"));
  CHECK(t.owner.take(write, result) && result.status.code == Err::TIMEOUT);
  CHECK(t.send("settings\n") == lost && t.port.writes.size() == 9);
  t.port.isolationVerified = true;
  CHECK(t.owner.recoverIsolated());
  const auto read = startRead(t, Setting::SwitchThreshold);
  t.port.deliver({1, 3, 2, 0xFF, 0xFF, 0xB9, 0xF4});
  t.turn();
  CHECK(t.owner.take(read, result) && result.status);
  CHECK(
      contains(t.send("settings\n"), "desired 0x0320, observed 0xFFFF (outside published range)"));
  CHECK(contains(t.send("model SEN0591\n"), "SEN0591"));
  const auto rebound = t.send("settings\n");
  CHECK(!contains(rebound, "desired 0x") && !contains(rebound, "observed 0x"));
  CHECK(t.port.writes.size() == 10 && t.owner.physicalAttempts() == 10);
}
}  // namespace

int main() {
  emptyReports();
  healthAndTransport();
  configurationHistory();
}
