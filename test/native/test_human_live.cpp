#include <string>

#include "HumanConsoleFixture.h"

namespace {
using namespace SEN059x;
using namespace console_test;
using Bytes = std::vector<std::uint8_t>;

bool transcript = false;

void expect(const std::string& text, const std::string& fragment) {
  if (!contains(text, fragment)) {
    std::fprintf(stderr, "Missing <%s> in:\n%s\n", fragment.c_str(), text.c_str());
    std::exit(EXIT_FAILURE);
  }
}

std::string hex(std::uint16_t word) {
  char buffer[7];
  std::snprintf(buffer, sizeof(buffer), "0x%04X", static_cast<unsigned>(word));
  return buffer;
}

// Request and reply CRC bytes come from the independently checked cases in test_read_codec.
// No expected frame or interpreted unit comes from the production lookup or encoder.
struct Case {
  const char* command;
  std::uint16_t reg;
  std::uint8_t txLow, txHigh;
  std::uint16_t word;
  std::uint8_t rxLow, rxHigh;
  const char* description;

  Bytes tx() const {
    return {1, 3, std::uint8_t(reg >> 8), std::uint8_t(reg), 0, 1, txLow, txHigh};
  }
  Bytes rx() const { return {1, 3, 2, std::uint8_t(word >> 8), std::uint8_t(word), rxLow, rxHigh}; }
};

const Case CASES[] = {
    {"read processed", 0x0100, 0x85, 0xF6, 0x02F2, 0x38, 0xA1, "754 mm"},
    {"read realtime", 0x0101, 0xD4, 0x36, 0x02EF, 0xF8, 0xA8, "751 mm"},
    {"read temperature", 0x0102, 0x24, 0x36, 0x012C, 0xB8, 0x09,
     "30.0 \xC2\xB0"
     "C"},
    {"read echo", 0x010A, 0xA5, 0xF4, 0x1676, 0x37, 0xC2, "5750 \xC2\xB5s"},
    {"config read address", 0x0200, 0x85, 0xB2, 0x0001, 0x79, 0x84, "address"},
    {"config read baud", 0x0201, 0xD4, 0x72, 0x0003, 0xF8, 0x45, "9600 baud"},
    {"config read polarity", 0x0205, 0x95, 0xB3, 0x0001, 0x79, 0x84, "high below threshold"},
    {"config read threshold", 0x0206, 0x65, 0xB3, 0x0064, 0xB9, 0xAF, "100 mm"},
    {"config read angle", 0x0208, 0x04, 0x70, 0x0004, 0xB9, 0x87, "deg"},
    {"config read uart-unit", 0x0209, 0x55, 0xB0, 0x0000, 0xB8, 0x44,
     "automatic/controlled UART output"},
    {"config read noise", 0x021A, 0xA4, 0x75, 0x0001, 0x79, 0x84, "battery supply"},
    {"config read range", 0x021F, 0xB4, 0x74, 0x0001, 0x79, 0x84, "about 50 cm"},
};
const Bytes BLOCK_TX{1, 3, 1, 0, 0, 3, 4, 0x37};
const Bytes BLOCK_RX{1, 3, 6, 2, 0xF2, 2, 0xEF, 1, 0x2C, 0x29, 0x42};
const Bytes EXCEPTION{1, 0x83, 2, 0xC0, 0xF1};

struct Live : TestTerminal<scheduler_test::Port> {
  explicit Live(std::string model = "SEN0591") {
    flush();
    expect(command("model " + model), model);
    CHECK(port.writes.empty());
  }

  std::string command(const std::string& line) {
    const auto output = send(line + "\n");
    if (transcript) {
      std::printf("> %s\n%s\n", line.c_str(), output.c_str());
    }
    return output;
  }

  ExchangeRequest start(const std::string& line, const Bytes& tx) {
    const auto before = port.writes.size();
    const auto accepted = command(line);
    expect(accepted, "Accepted operation ");
    turn();
    CHECK(port.writes.size() == before + 1 && port.writes.back() == tx);
    if (transcript) {
      std::printf("  UART TX");
      for (auto byte : port.writes.back()) {
        std::printf(" %02X", static_cast<unsigned>(byte));
      }
      std::printf("\n");
    }
    turn();
    CHECK(!port.transmitting);
    const auto active = owner.activity();
    CHECK(active.phase == OwnerPhase::Active && active.request);
    expect(accepted, "Accepted operation " + std::to_string(active.request->operationId));
    return *active.request;
  }

  std::string output() {
    turn();
    flush();
    if (transcript && !sink.output.empty()) {
      std::puts(sink.output.c_str());
    }
    return sink.output;
  }

  std::string reply(const Bytes& rx, unsigned deltaUs = 3000) {
    sink.output.clear();
    port.time += deltaUs;
    port.deliver(rx);
    return output();
  }

  std::string expire(const ExchangeRequest& active) {
    sink.output.clear();
    CHECK(port.time < active.deadlineUs);
    port.time = active.deadlineUs;
    return output();
  }
};

void matrix(const std::string& model) {
  Live t(model);
  for (const auto& c : CASES) {
    std::vector<std::string> commands{c.command, "readreg " + hex(c.reg)};
    if (c.reg >= 0x0200) {
      commands.push_back("config read " + std::to_string(c.reg));
    }
    if (c.reg == 0x0201) {
      commands.emplace_back("readbaud");
    }
    for (const auto& command : commands) {
      const auto req = t.start(command, c.tx());
      const auto done = t.reply(c.rx());
      expect(done, "Operation " + std::to_string(req.operationId) + " OK");
      for (const auto& field : {model, hex(c.reg), hex(c.word), std::string(c.description)}) {
        expect(done, field);
      }
      expect(done, "elapsed 3 ms");
      if (c.reg < 0x0200) {
        const auto s = t.owner.snapshot(0, static_cast<MeasurementKind>(c.reg), t.owner.nowMs(), 1);
        CHECK(s.usable.measurement.rawWord == c.word && s.usable.ageMs == 0);
        expect(done, "sample generation " + std::to_string(s.usable.sampleGeneration));
        expect(done, "observed " + std::to_string(t.owner.nowMs()) + " ms");
        expect(done, "age 0 ms");
      } else {
        const auto s = t.owner.settingSnapshot(0, static_cast<Setting>(c.reg));
        CHECK(s.trusted && s.observed && s.observed->value.rawWord == c.word);
        CHECK(s.observed->completedMs == t.owner.nowMs());
      }
      if (c.reg == 0x0208) {
        expect(done, model == "SEN0591" ? "about 60 deg" : "80 deg dual vertical");
      }
      if (c.reg == 0x021F) {
        expect(done, model == "SEN0591" ? "rated 3 m" : "rated 5 m");
      }
    }
  }
  CHECK(t.port.writes.size() == 33 && t.owner.physicalAttempts() == 33);
}

void maintenanceControls(const std::string& model) {
  Live t(model);
  const auto& address = CASES[4];
  t.start(address.command, address.tx());
  expect(t.reply(address.rx()), "OK");
  const auto settings = t.command("settings");
  expect(settings, "observed 0x0001 (1) at 3 ms, address 1 115200 baud, trusted");
  const auto result = t.command("result");
  expect(t.command("maintenance"), "Maintenance off");
  for (const char* bad : {"maintenance on", "maintenance isolated", "maintenance on shared",
                          "maintenance on isolated extra"}) {
    expect(t.command(bad), "[E]");
    expect(t.command("maintenance"), "Maintenance off");
  }
  expect(t.command("maintenance on isolated"), "Maintenance on isolated");
  expect(t.command("maintenance off extra"), "[E]");
  expect(t.command("maintenance"), "Maintenance on isolated");
  expect(t.command("maintenance off"), "Maintenance off");
  expect(t.command("maintenance"), "Maintenance off");

  expect(t.command("maintenance on isolated"), "Maintenance on isolated");
  t.port.time += 59999999;
  expect(t.command("version"), "[I]");  // Operator activity renews the idle deadline.
  t.port.time += 59999999;
  expect(t.command("maintenance"), "Maintenance on isolated");
  t.port.time += 30000000;
  t.turn();  // Servicing the console alone does not extend the grant.
  t.port.time += 30000000;
  expect(t.command("maintenance"), "Maintenance off");
  CHECK(t.command("settings") == settings && t.command("result") == result);

  expect(t.command("maintenance on isolated"), "Maintenance on isolated");
  expect(t.command("useaddr 2"), "[I]");
  expect(t.command("maintenance"), "Maintenance off");
  CHECK(t.port.writes.size() == 1 && t.owner.physicalAttempts() == 1);
}

void typedFailures() {
  Live t;
  const auto& temp = CASES[2];
  t.start(temp.command, temp.tx());
  expect(t.reply(temp.rx()), "30.0");
  const auto goodTime = t.owner.nowMs();
  t.start(temp.command, temp.tx());
  const auto ambiguous = t.reply({1, 3, 2, 0xFF, 0xFF, 0xB9, 0xF4});
  expect(ambiguous, "0xFFFF");
  expect(ambiguous, "ambiguous");
  t.start(temp.command, temp.tx());
  const auto exception = t.reply(EXCEPTION);
  expect(exception, "EXCEPTION detail 2");
  expect(exception, "Illegal Data Address");
  expect(exception, "30.0");
  expect(exception, "observed " + std::to_string(goodTime) + " ms");
  const auto req = t.start(temp.command, temp.tx());
  expect(t.expire(req), "TIMEOUT");
  expect(t.command("result " + std::to_string(req.operationId)), "30.0");
  const auto s = t.owner.snapshot(0, MeasurementKind::Temperature, t.owner.nowMs(), 1000);
  CHECK(s.raw.measurement.rawWord == 65535 && s.usable.measurement.completedMs == goodTime);
  CHECK(t.port.writes.size() == 4 && t.owner.stats().retryCount == 0);
}

void diagnoseAndRecover() {
  Live t;
  t.start("diagnose", CASES[4].tx());
  const auto match = t.reply(CASES[4].rx());
  expect(match, "expected address 1, observed 1");
  t.start("diag", CASES[4].tx());
  expect(t.reply(EXCEPTION), "EXCEPTION detail 2");
  t.start("diag", CASES[4].tx());
  expect(t.reply(CASES[5].rx()), "expected address 1, observed 3");
  const auto dist = t.start("read realtime", CASES[1].tx());
  expect(t.expire(dist), "TIMEOUT");
  CHECK(!t.owner.settled());
  t.command("recover");
  CHECK(!t.owner.settled());
  // Identical payload would decode as 30 C for a new temperature request.
  t.port.deliver(CASES[2].rx());
  t.command("recover");
  CHECK(t.owner.settled());
  expect(t.command("latest realtime"), "never sampled");
  expect(t.command("latest temperature"), "never sampled");
  CHECK(t.port.writes.size() == 4);
  t.start("read temperature", CASES[2].tx());
  expect(t.reply(CASES[2].rx()), "30.0");
}

void rawReads() {
  Live t("SEN0592");
  for (const auto* bad :
       {"readregs 0x0100 0", "readregs 0x0100 126", "readregs 0x0101 3", "readregs 0xffff 2",
        "readregs 0x0100 0x3", "readregs 65536 1", "useaddr 255", "readregs 0x0100 3 255",
        "config read 0x0101", "readreg 0x0103", "readreg 0x0101x"}) {
    expect(t.command(bad), "[E]");
  }
  CHECK(t.port.writes.empty());
  t.start("read processed", CASES[0].tx());
  t.reply(CASES[0].rx());
  t.start("config read baud", CASES[5].tx());
  t.reply(CASES[5].rx());
  const auto history = t.owner.snapshot(0, MeasurementKind::ProcessedDistance, 0, 0);
  const auto settings = t.command("settings");
  const auto block = t.start("readregs 256 3", BLOCK_TX);
  CHECK(block.deadlineUs - t.port.time > 1030000);
  const auto done = t.reply(BLOCK_RX);
  const auto first = done.find("0x0100 = 0x02F2");
  const auto second = done.find("0x0101 = 0x02EF");
  const auto third = done.find("0x0102 = 0x012C");
  CHECK(first < second && second < third && third != std::string::npos);
  t.start("readregs 256 1", CASES[0].tx());
  expect(t.reply(CASES[1].rx()), "0x0100 = 0x02EF");
  t.start("readregs 0x0201 1", CASES[5].tx());
  expect(t.reply(CASES[4].rx()), "0x0201 = 0x0001");
  CHECK(t.command("settings") == settings);
  const auto after = t.owner.snapshot(0, MeasurementKind::ProcessedDistance, 0, 0);
  CHECK(after.bindingGeneration == history.bindingGeneration);
  CHECK(after.raw.sampleGeneration == history.raw.sampleGeneration);
  CHECK(after.usable.measurement.completedMs == history.usable.measurement.completedMs);
  CHECK(after.raw.measurement.rawWord == history.raw.measurement.rawWord);
  expect(t.command("latest temperature"), "never sampled");
  t.start("readregs 0x0100 3", BLOCK_TX);
  expect(t.reply(EXCEPTION), "EXCEPTION detail 2");
  t.start("readregs 0x0100 3", BLOCK_TX);
  expect(t.reply(CASES[0].rx()), "FRAME_ERROR");
  if (!t.owner.settled()) {
    t.port.deliver(BLOCK_RX);
    t.command("recover");
  }
  const auto missing = t.start("readregs 0x0100 3", BLOCK_TX);
  const auto timeout = t.expire(missing);
  expect(timeout, "TIMEOUT");
  CHECK(!contains(timeout, " = 0x") && !t.owner.settled());
  t.port.deliver(BLOCK_RX);
  t.command("recover");
  CHECK(t.owner.settled() && t.port.writes.size() == 8 && t.owner.stats().retryCount == 0);
  expect(t.command("latest temperature"), "never sampled");
}

void watchFailures() {
  Live t;
  // Cross the 32-bit ms rollover during this watch. Deadlines share the
  // port's absolute microsecond clock; only cached observation ticks wrap.
  t.port.time = (std::uint64_t{1} << 32) * 1000 - 23000;
  for (const char* bad :
       {"watch temperature 0 10", "watch temperature 101 10", "watch temperature 2 0",
        "watch temperature 2 60001", "watch temperature 2 10ms"}) {
    expect(t.command(bad), "[E]");
  }
  CHECK(t.port.writes.empty());
  const auto old = t.start("read temperature", CASES[2].tx());
  t.reply(CASES[2].rx());
  const auto firstStart = t.port.time;
  const auto first = t.start("watch temperature 2 10", CASES[2].tx());
  CHECK(old.operationId != first.operationId);
  const auto id = std::to_string(first.operationId);
  expect(t.command("cancel " + std::to_string(old.operationId)), "[E]");
  expect(t.command("read echo"), "BUS_BUSY");
  expect(t.command("latest temperature"), "30.0");
  expect(t.command("job status"), id);
  CHECK(t.port.writes.size() == 2 && t.owner.activity().request->operationId == first.operationId);
  // Three interval boundaries pass while the first child waits for RX.
  t.reply(CASES[2].rx(), 35000);
  const auto goodTime = t.owner.nowMs();
  CHECK(t.owner.settled() && t.port.writes.size() == 2);
  expect(t.command("model SEN0592"), "[E]");
  const auto wake = t.console.service();
  CHECK(wake == firstStart + 40000);
  t.port.time = wake - 1;
  t.turn();
  CHECK(t.port.writes.size() == 2);
  t.port.time = wake;
  t.turn();
  t.turn();
  t.turn();
  CHECK(t.port.writes.size() == 3 && t.port.writes.back() == CASES[2].tx());
  const auto second = t.owner.activity().request;
  CHECK(second && second->operationId == first.operationId);
  const auto done = t.expire(*second);
  expect(done, "Operation " + id);
  expect(done, "TIMEOUT");
  expect(done, "requested 2, completed 1, failed 1");
  expect(done, "skipped intervals 3,");
  const auto retained = t.command("result " + id);
  CHECK(retained == t.command("result"));
  expect(retained, "transfers 2");
  expect(retained, "TIMEOUT");
  expect(t.command("cancel " + std::to_string(old.operationId)), "[E]");
  t.port.deliver(CASES[2].rx());
  t.command("recover");
  expect(t.command("latest temperature"), "observed " + std::to_string(goodTime) + " ms");
  t.port.time += 10000000;
  t.turn();
  CHECK(t.owner.settled() && t.port.writes.size() == 3);
}

void watchBounds() {
  for (const auto count : {1u, 100u}) {
    Live t;
    const auto intervalMs = count == 1 ? 1u : 60000u;
    const auto first = t.start(
        "watch echo " + std::to_string(count) + " " + std::to_string(intervalMs), CASES[3].tx());
    std::string last;
    for (unsigned read = 0; read < count; ++read) {
      if (read) {
        const auto due = t.console.service();
        CHECK(due == std::uint64_t{read} * intervalMs * 1000);
        t.port.time = due;
        t.turn();
        t.turn();
        t.turn();
      }
      CHECK(t.port.writes.size() == read + 1 && t.port.writes.back() == CASES[3].tx());
      CHECK(t.owner.activity().request &&
            t.owner.activity().request->operationId == first.operationId);
      last = t.reply(CASES[3].rx());
    }
    expect(last, "completed " + std::to_string(count) + ", failed 0");
    expect(last, "5750");
    CHECK(t.console.service() == UINT64_MAX && t.owner.settled());
    CHECK(t.owner.physicalAttempts() == count);
  }
}

void cancellation() {
  Live t;
  const auto watch = t.start("watch realtime 2 100", CASES[1].tx());
  t.reply(CASES[1].rx());
  CHECK(t.owner.settled());
  t.command("stop");  // Waiting for the next tick, before the next TX.
  const auto canceled = t.command("result " + std::to_string(watch.operationId));
  expect(canceled, "ABORTED");
  expect(canceled, "transfers 1");
  t.port.time += 200000;
  t.turn();
  CHECK(t.port.writes.size() == 1);
  const auto temp = t.start("read temperature", CASES[2].tx());
  t.command("cancel " + std::to_string(temp.operationId));
  const auto interrupted = t.command("result");
  expect(interrupted, "ABORTED");
  expect(interrupted, "transfers 1");
  CHECK(!t.owner.settled());
  t.port.deliver(CASES[2].rx());
  t.command("recover");
  CHECK(t.owner.settled() && t.port.writes.size() == 2);
  expect(t.command("latest temperature"), "never sampled");
  expect(t.command("result " + std::to_string(watch.operationId)), "[E]");
  t.command("stop");
  CHECK(t.command("result") == interrupted);
}

void cancelAfterWatchTimeout() {
  for (const bool recover : {false, true}) {
    Live t;
    const auto child = t.start("watch temperature 2 1000", CASES[2].tx());
    const auto id = std::to_string(child.operationId);
    t.expire(child);
    CHECK(!t.owner.settled());
    if (recover) {
      t.port.deliver(CASES[2].rx());
      expect(t.command("recover"), "Host transport settled");
      CHECK(t.owner.settled());
    }
    const auto status = t.command("status");
    expect(status, recover ? "Owner idle" : "Owner recovery");
    expect(status, "Operation " + id + " waiting, next read deadline");
    t.command("stop " + id);
    const auto terminal = t.command("result " + id);
    expect(terminal, "ABORTED");
    expect(terminal, recover ? "transfers 1, transfer cleanup complete"
                             : "transfers 1, wire settlement required");
    expect(terminal, "completed 0, failed 1");
    expect(terminal, "last result TIMEOUT");
    CHECK(t.port.writes.size() == 1);
  }
}

void cancelDuringPeerRecovery() {
  Live t;
  const auto child = t.start("watch temperature 2 1000", CASES[2].tx());
  t.reply(CASES[2].rx());
  // Another caller can use the same operation ID and binding. Only its ticket is unique.
  std::uint64_t peer = 0;
  const auto deadline = t.port.time + 100000;
  CHECK(
      t.owner.submit(0, MeasurementKind::Temperature, child.operationId, 1, deadline, false, peer));
  t.turn();
  t.turn();
  t.port.time = deadline;
  t.turn();
  ScheduledResult result;
  CHECK(t.owner.take(peer, result) && result.status.code == Err::TIMEOUT && !t.owner.settled());
  t.command("stop");
  expect(t.command("result"), "transfers 1, transfer cleanup complete");
  CHECK(t.port.writes.size() == 2);
}

void cancelBeforeTx() {
  Live t;
  CHECK(t.owner.bind(1, DeviceModel::SEN0592, 2, 115200));
  const auto warmup = t.start("read temperature", CASES[2].tx());
  t.reply(CASES[2].rx());
  // Round robin now favors slot 1. A peer queues after human admission and
  // wins the wire while the console is delivering the admission message.
  t.sink.output.clear();
  for (auto byte : std::string_view("read temperature\n")) {
    CHECK(t.console.feed(byte));
  }
  const auto queued = t.owner.activity().request;
  CHECK(queued && queued->operationId != warmup.operationId && t.port.writes.size() == 1);
  std::uint64_t peer = 0;
  CHECK(t.owner.submit(1, MeasurementKind::Temperature, 70, 0, t.port.time + 100000, false, peer));
  t.flush();
  t.turn();
  CHECK(t.owner.activity().request && t.owner.activity().request->operationId == 70);
  CHECK(t.port.writes.size() == 2 && t.port.writes.back()[0] == 2);
  const auto id = std::to_string(queued->operationId);
  t.command("cancel " + id);
  const auto result = t.command("result " + id);
  expect(result, "ABORTED");
  expect(result, "transfers 0");
  CHECK(t.owner.activity().request && t.owner.activity().request->operationId == 70);
  CHECK(t.port.writes.size() == 2);
}

void cancelAfterChildCompletion() {
  for (const auto count : {1u, 3u}) {
    Live t;
    const auto active = t.start("watch echo " + std::to_string(count) + " 100", CASES[3].tx());
    t.port.time = 3000;
    t.port.deliver(CASES[3].rx());
    t.owner.service();  // UART event finishes the child before the console consumes its ticket.
    CHECK(t.owner.settled());
    t.command("cancel " + std::to_string(active.operationId));
    const auto retained = t.command("result");
    expect(retained,
           "Operation " + std::to_string(active.operationId) + (count == 1 ? " OK" : " ABORTED"));
    expect(retained, "requested " + std::to_string(count) + ", completed 1, failed 0");
    expect(retained, "last result OK");
    t.port.time += 300000;
    t.turn();
    CHECK(t.port.writes.size() == 1 && t.owner.settled());
  }
}

void stalledOutput() {
  Live t;
  t.sink.output.clear();
  t.sink.limit = 0;
  for (auto byte : std::string_view("read realtime\n")) {
    CHECK(t.console.feed(byte));
  }
  t.turn();
  t.turn();
  CHECK(t.port.writes.size() == 1 && t.port.writes.back() == CASES[1].tx());
  const auto active = t.owner.activity().request;
  CHECK(active);
  t.port.time += 42000;
  t.port.deliver(CASES[1].rx());
  t.turn();
  t.turn();
  CHECK(t.owner.settled() && t.sink.output.empty());
  // Change the live cache before the preserved terminal has reached a stalled sink.
  std::uint64_t peerTicket = 0;
  CHECK(t.owner.submit(0, MeasurementKind::RealTimeDistance, 7000, 0, t.port.time + 100000, false,
                       peerTicket));
  t.turn();
  t.turn();
  t.port.time += 5000;
  t.port.deliver(CASES[0].rx());
  t.turn();
  ScheduledResult peer;
  CHECK(t.owner.take(peerTicket, peer) && peer.measurement.rawWord == 754);
  t.sink.limit = 2;
  t.flush();
  t.turn();
  t.flush();
  const auto stream = t.sink.output;
  const auto id = std::to_string(active->operationId);
  const auto marker = "Operation " + id + " OK";
  expect(stream, "Accepted operation " + id);
  expect(stream, marker);
  CHECK(stream.find(marker) == stream.rfind(marker));
  expect(stream, "751 mm");
  CHECK(!contains(stream, "754 mm"));
  expect(t.command("latest realtime"), "754 mm");
  expect(t.command("result " + id), "751 mm");
  CHECK(t.owner.physicalAttempts() == 2);
}
}  // namespace

int main(int argc, char** argv) {
  transcript = argc == 2 && std::string_view(argv[1]) == "--transcript";
  matrix("SEN0591");
  matrix("SEN0592");
  maintenanceControls("SEN0591");
  maintenanceControls("SEN0592");
  typedFailures();
  diagnoseAndRecover();
  rawReads();
  watchFailures();
  watchBounds();
  cancellation();
  cancelAfterWatchTimeout();
  cancelDuringPeerRecovery();
  cancelBeforeTx();
  cancelAfterChildCompletion();
  stalledOutput();
}
