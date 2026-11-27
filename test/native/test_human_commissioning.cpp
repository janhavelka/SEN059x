#include "HumanConsoleFixture.h"

namespace {
using namespace SEN059x;
using namespace console_test;
using Bytes = std::vector<std::uint8_t>;

bool transcript = false;

void expect(const std::string& output, const std::string& fragment) {
  if (!contains(output, fragment)) {
    std::fprintf(stderr, "Missing <%s> in:\n%s\n", fragment.c_str(), output.c_str());
    std::exit(EXIT_FAILURE);
  }
}

std::string hex(std::uint16_t word) {
  char out[7];
  std::snprintf(out, sizeof(out), "0x%04X", static_cast<unsigned>(word));
  return out;
}

// Operator values and raw register words come from the FC06 rows of OPERATIONS.csv.
// A separate RTU encoder in this fixture makes the frame expectation independent of the codec.
struct WriteCase {
  const char* key;
  const char* value;
  std::uint16_t reg, word, oldWord;
  const char* helpFirst;
  const char* helpSecond;
};
constexpr WriteCase CASES[]{
    {"address", "5", 0x0200, 5, 1, "1..247", "1..254"},
    {"baud", "9600", 0x0201, 3, 9, "2 = 4800 bps", "9 = 115200 bps"},
    {"polarity", "0", 0x0205, 0, 1, "0 = low below threshold", "1 = high below threshold"},
    {"threshold", "750", 0x0206, 750, 100, "(mm) 30..3000", "(mm) 30..5000"},
    {"angle", "2", 0x0208, 2, 4, "4 = about 60 deg", "80 deg dual vertical"},
    {"uart-unit", "us", 0x0209, 1, 0, "0 = mm", "1 = us"},
    {"noise", "4", 0x021A, 4, 1, "1 = battery supply", "5 = switching supply"},
    {"range", "3", 0x021F, 3, 1, "rated 3 m", "rated 5 m"},
};
constexpr Endpoint DEFAULT{1, 115200};

Bytes rtu(Bytes data) {
  std::uint16_t crc = 0xFFFF;
  for (const auto byte : data) {
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; ++bit) {
      crc = (crc & 1u) ? static_cast<std::uint16_t>((crc >> 1) ^ 0xA001u) : crc >> 1;
    }
  }
  data.push_back(static_cast<std::uint8_t>(crc));
  data.push_back(static_cast<std::uint8_t>(crc >> 8));
  return data;
}

Bytes request(std::uint8_t address, std::uint8_t fc, std::uint16_t reg, std::uint16_t word) {
  return rtu({address, fc, static_cast<std::uint8_t>(reg >> 8), static_cast<std::uint8_t>(reg),
              static_cast<std::uint8_t>(word >> 8), static_cast<std::uint8_t>(word)});
}

Bytes readReply(std::uint8_t address, std::uint16_t word) {
  return rtu(
      {address, 3, 2, static_cast<std::uint8_t>(word >> 8), static_cast<std::uint8_t>(word)});
}

struct Replay : TestTerminal<scheduler_test::Port> {
  explicit Replay(const std::string& model, bool isolated = false) {
    flush();
    expect(command("model " + model), model);
    if (isolated) {
      expect(command("profile dfrobot-isolated"), "dfrobot-isolated");
    }
  }

  std::string command(const std::string& line) {
    const auto out = send(line + "\n");
    if (transcript) {
      std::printf("> %s\n%s\n", line.c_str(), out.c_str());
    }
    return out;
  }

  void grant() { expect(command("maintenance on isolated"), "Maintenance on isolated"); }

  void wire(std::size_t index, const Bytes& frame, std::uint32_t rate) {
    for (unsigned i = 0; i < 4 && port.writes.size() <= index; ++i) {
      turn();
    }
    CHECK(port.writes.size() == index + 1 && port.writes[index] == frame && port.baud == rate);
    if (transcript) {
      std::printf("  UART TX at %u bps:", static_cast<unsigned>(rate));
      for (auto byte : frame) {
        std::printf(" %02X", static_cast<unsigned>(byte));
      }
      std::puts("");
    }
    turn();
    CHECK(!port.transmitting);
  }

  ExchangeRequest start(const std::string& line, const Bytes& frame, std::uint32_t rate) {
    const auto index = port.writes.size();
    const auto accepted = command(line);
    expect(accepted, "Accepted operation ");
    wire(index, frame, rate);
    const auto active = owner.activity();
    CHECK(active.phase == OwnerPhase::Active && active.request);
    expect(accepted, "Accepted operation " + std::to_string(active.request->operationId));
    return *active.request;
  }

  std::string poll() {
    sink.output.clear();
    turn();
    flush();
    if (transcript && !sink.output.empty()) {
      std::puts(sink.output.c_str());
    }
    return sink.output;
  }

  std::string reply(const Bytes& frame, AckOrigin origin = AckOrigin::Device) {
    // Advance through the supplied frame's 8N1 wire time before reporting its closure.
    port.time += (frame.size() * 10000000u + port.baud - 1) / port.baud + 2000;
    port.deliver(frame, origin);
    return poll();
  }

  void read(const WriteCase& c, std::uint16_t word, Endpoint endpoint) {
    start(std::string("config read ") + c.key, request(endpoint.address, 3, c.reg, 1),
          endpoint.bps);
    const auto done = reply(readReply(endpoint.address, word));
    expect(done, " OK");
    expect(done, hex(word));
    const auto state = owner.settingSnapshot(0, static_cast<Setting>(c.reg));
    CHECK(state.trusted && state.observed && state.observed->value.rawWord == word);
    CHECK(state.observed->endpoint.address == endpoint.address &&
          state.observed->endpoint.bps == endpoint.bps &&
          state.observed->completedMs == owner.nowMs());
  }

  void historical(const WriteCase& c, std::uint32_t seedMs, bool endpointChanged) {
    const auto state = owner.settingSnapshot(0, static_cast<Setting>(c.reg));
    CHECK(!state.trusted && state.observed && state.observed->value.rawWord == c.oldWord);
    CHECK(state.observed->completedMs == seedMs && owner.endpointTrusted(0) == !endpointChanged);
    const auto settings = command("settings");
    expect(settings, "observed " + hex(c.oldWord));
    expect(settings,
           "at " + std::to_string(seedMs) + " ms, address 1 115200 baud, historical, unconfirmed");
    if (endpointChanged) {
      const auto status = command("status");
      expect(status, "endpoint unconfirmed");
      expect(status, "Last-known endpoint address 1, 115200 baud");
      expect(status, "Intended endpoint");
    }
  }
};

void matrix(const std::string& model, const WriteCase& c, bool vendor) {
  Replay t(model, vendor);
  t.read(c, c.oldWord, DEFAULT);
  const auto seedMs = t.owner.nowMs();
  const auto settings = t.command("settings");
  const auto seedResult = t.command("result");
  const std::string verb = vendor ? "broadcast set " : "config set ";
  const auto plan = verb + c.key + " " + c.value;
  const auto confirmed = plan + " confirm";
  const bool moveAddress = c.reg == 0x0200;
  const bool moveBaud = c.reg == 0x0201;
  Endpoint intended{static_cast<std::uint8_t>(moveAddress ? 5 : 1), moveBaud ? 9600U : 115200U};

  const auto help = t.command("help " + verb + c.key);
  expect(help, hex(c.reg));
  expect(help, c.helpFirst);
  expect(help, c.helpSecond);
  expect(help, "No ranging");
  const auto preview = t.command(plan);
  for (const auto& field :
       {std::string("Preview"), model, std::string(vendor ? "dfrobot-isolated" : "standard"),
        hex(c.reg), hex(c.word)}) {
    expect(preview, field);
  }
  expect(t.command(confirmed), "DISALLOWED");
  CHECK(t.command("settings") == settings && t.command("result") == seedResult);
  t.grant();
  expect(t.command(plan + " CONFIRM"), "[E]");
  expect(t.command(plan + " confirm extra"), "[E]");
  expect(t.command("maintenance"), "Maintenance on isolated");
  CHECK(t.port.writes.size() == 1 && t.owner.physicalAttempts() == 1);

  const auto frame = request(vendor ? 255 : 1, 6, c.reg, c.word);
  const auto operation = t.start(confirmed, frame, DEFAULT.bps);
  const auto lookup = "result " + std::to_string(operation.operationId);
  expect(t.command("maintenance"), "Maintenance off");
  t.historical(c, seedMs, moveAddress || moveBaud);
  if (vendor) {
    t.reply(frame, AckOrigin::LocalEcho);
    t.port.time += 2000;
    const auto done = t.poll();
    expect(done, "verified=false");
    expect(done, "transfers 1");
    std::string incidental = "RX observed 8 bytes, origin local echo, captured";
    for (const auto byte : frame) {
      incidental += " 0x" + hex(byte).substr(4);
    }
    expect(done, incidental);
    expect(done, "TX accepted 8 bytes");
    expect(done, "Primary OK");
    expect(done, "Cleanup OK");
    expect(done, "historical, unconfirmed");
    expect(done, moveAddress || moveBaud ? "Endpoint at completion unconfirmed"
                                         : "Endpoint at completion trusted");
    CHECK(t.port.writes.size() == 2 && t.owner.settled());
    t.historical(c, seedMs, moveAddress || moveBaud);
    const auto sendResult = t.command(lookup);
    expect(sendResult, "verified=false");
    if (moveAddress) {
      expect(t.command("useaddr 5"), "address 5");
    }
    if (moveBaud) {
      expect(t.command("host baud 9600"), "9600");
    }
    t.read(c, c.word, intended);
    CHECK(t.command(lookup) == sendResult);
  } else {
    t.reply(frame);
    t.wire(2, request(intended.address, 3, c.reg, 1), intended.bps);
    t.historical(c, seedMs, moveAddress || moveBaud);
    const auto done = t.reply(readReply(intended.address, c.word));
    expect(done, "verified=true");
    expect(done, "transfers 2");
    const auto result = t.command(lookup);
    for (const char* field :
         {"Original endpoint address 1, 115200 baud", "Primary OK", "Cleanup OK", "FC06 ACK OK",
          "FC03 readback OK", "Endpoint at completion trusted", "Previous setting",
          "Setting at completion"}) {
      expect(result, field);
    }
    expect(result, "Intended endpoint address " + std::to_string(intended.address) + ", " +
                       std::to_string(intended.bps) + " baud");
    expect(result, "observed " + hex(c.oldWord));
    expect(result, "at " + std::to_string(seedMs) + " ms, address 1 115200 baud, trusted");
    expect(result, "observed " + hex(c.word));
    expect(result, "at " + std::to_string(t.owner.nowMs()) + " ms, address " +
                       std::to_string(intended.address) + " " + std::to_string(intended.bps) +
                       " baud, trusted");
    const auto state = t.owner.settingSnapshot(0, static_cast<Setting>(c.reg));
    CHECK(state.trusted && state.observed && state.observed->value.rawWord == c.word);
    CHECK(state.observed->endpoint.address == intended.address &&
          state.observed->endpoint.bps == intended.bps);
  }
  CHECK(t.port.writes.size() == 3 && t.owner.physicalAttempts() == 3 && t.owner.endpointTrusted(0));
  CHECK(t.owner.stats().retryCount == 0);
}

void boundaries(const std::string& model) {
  Replay t(model);
  const bool shortRange = model == "SEN0591";
  const auto maximum = shortRange ? 3000u : 5000u;
  const auto range = shortRange ? 4u : 5u;
  const auto snapshot = t.command("settings");
  const std::string previewCommands[]{"config set polarity 1",
                                      "config set uart-unit mm",
                                      "config set threshold 30",
                                      "config set threshold " + std::to_string(maximum),
                                      "config set range 1",
                                      "config set range " + std::to_string(range),
                                      "setaddr 247"};
  for (const auto& line : previewCommands) {
    expect(t.command(line), "Preview");
  }
  const unsigned rates[]{4800, 9600, 14400, 19200, 38400, 57600, 76800, 115200};
  for (std::uint16_t code = 2; code <= 9; ++code) {
    const auto line = "setbaud " + std::to_string(rates[code - 2]);
    expect(t.command(line), hex(code));
  }
  t.grant();
  const std::string badCommands[]{"config set address 0",
                                  "setaddr 255",
                                  "setaddr 248",
                                  "setaddr 254",
                                  "setaddr -1",
                                  "setaddr 256",
                                  "setbaud 3",
                                  "setbaud 9600bps",
                                  "config set threshold 29",
                                  "config set threshold " + std::to_string(maximum + 1),
                                  "config set uart-unit 1",
                                  "config set range " + std::to_string(range + 1),
                                  "config set polarity 2",
                                  "config set noise 6",
                                  "config set angle 0",
                                  "config set 0x0200 5",
                                  "config set threshold 65536"};
  for (const auto& line : badCommands) {
    expect(t.command(line + " confirm"), "[E]");
  }
  expect(t.command("broadcast set threshold 750"), "DISALLOWED");
  expect(t.command("broadcast set threshold 750 confirm"), "DISALLOWED");
  expect(t.command("maintenance"), "Maintenance on isolated");
  t.port.time += 60000000;
  expect(t.command("setaddr 5 confirm"), "DISALLOWED");
  CHECK(t.command("settings") == snapshot && t.port.writes.empty() &&
        t.owner.physicalAttempts() == 0);
  expect(t.command("profile dfrobot-isolated"), "dfrobot-isolated");
  t.grant();
  for (const char* line :
       {"setaddr 0 confirm", "setaddr 255 confirm", "broadcast set address 0 confirm",
        "broadcast set address 255 confirm", "useaddr 0", "useaddr 255"}) {
    expect(t.command(line), "[E]");
  }
  CHECK(t.port.writes.empty());
}

void settingLimits(const std::string& model) {
  Replay t(model);
  t.read(CASES[0], 1, DEFAULT);
  const bool shortRange = model == "SEN0591";
  struct Limit {
    std::string command;
    std::uint16_t reg, word;
  };
  const std::uint16_t threshold = shortRange ? 3000 : 5000;
  const std::uint16_t range = shortRange ? 4 : 5;
  const Limit limits[]{
      {"polarity 1", 0x0205, 1},    {"uart-unit mm", 0x0209, 0},
      {"threshold 30", 0x0206, 30}, {"threshold " + std::to_string(threshold), 0x0206, threshold},
      {"range 1", 0x021F, 1},       {"range " + std::to_string(range), 0x021F, range},
  };
  for (const auto& entry : limits) {
    t.grant();
    const auto index = t.port.writes.size();
    const auto frame = request(1, 6, entry.reg, entry.word);
    t.start("config set " + entry.command + " confirm", frame, DEFAULT.bps);
    t.reply(frame);
    t.wire(index + 1, request(1, 3, entry.reg, 1), DEFAULT.bps);
    expect(t.reply(readReply(1, entry.word)), "verified=true");
  }
  CHECK(t.port.writes.size() == 13 && t.owner.stats().retryCount == 0);
}

void extendedAndAliases(const std::string& model) {
  Replay t(model, true);
  t.read(CASES[0], 1, DEFAULT);
  std::uint8_t from = 1;
  for (std::uint8_t to : {248, 254, 1}) {
    t.grant();
    const auto index = t.port.writes.size();
    const auto frame = request(from, 6, 0x0200, to);
    t.start("setaddr " + std::to_string(to) + " confirm", frame, DEFAULT.bps);
    t.reply(frame);
    t.wire(index + 1, request(to, 3, 0x0200, 1), DEFAULT.bps);
    expect(t.reply(readReply(to, to)), "verified=true");
    t.read(CASES[0], to, {to, DEFAULT.bps});
    if (to > 247) {
      expect(t.command("profile standard"), "[E]");
    }
    from = to;
  }
  expect(t.command("profile standard"), "profile standard");
  t.read(CASES[1], 9, DEFAULT);
  std::uint32_t fromRate = DEFAULT.bps;
  std::uint16_t code = 2;
  for (const unsigned rate : {4800, 9600, 14400, 19200, 38400, 57600, 76800, 115200}) {
    const auto index = t.port.writes.size();
    const auto frame = request(1, 6, 0x0201, code);
    t.grant();
    t.start("setbaud " + std::to_string(rate) + " confirm", frame, fromRate);
    t.reply(frame);
    t.wire(index + 1, request(1, 3, 0x0201, 1), rate);
    expect(t.reply(readReply(1, code)), "verified=true");
    fromRate = rate;
    ++code;
  }
  CHECK(t.port.writes.size() == 27 && t.owner.stats().retryCount == 0);
}

enum class Fault { LostAck, LocalEcho, Exception, Mismatch };

void failure(const std::string& model, Fault fault) {
  const auto& c = fault == Fault::Exception ? CASES[3] : CASES[0];
  Replay t(model);
  t.read(c, c.oldWord, DEFAULT);
  const auto seedMs = t.owner.nowMs();
  t.grant();
  const auto frame = request(1, 6, c.reg, c.word);
  const auto op =
      t.start(std::string("config set ") + c.key + " " + c.value + " confirm", frame, DEFAULT.bps);
  std::string done;
  if (fault == Fault::Exception) {
    done = t.reply(rtu({1, 0x86, 2}));
    expect(done, "Primary EXCEPTION detail 2");
    expect(done, "Cleanup OK");
    expect(done, "FC06 ACK EXCEPTION");
    expect(done, "FC03 readback none");
    expect(done, "0x02 Illegal Data Address");
    t.historical(c, seedMs, false);
  } else if (fault == Fault::Mismatch) {
    t.reply(frame);
    t.wire(2, request(5, 3, c.reg, 1), DEFAULT.bps);
    done = t.reply(readReply(5, c.oldWord));
    expect(done, "Primary CONFIG_MISMATCH detail 1");
    expect(done, "Cleanup OK");
    expect(done, "FC03 readback OK");
    expect(done, "Endpoint at completion trusted");
    expect(done, "at " + std::to_string(t.owner.nowMs()) + " ms, address 5 115200 baud, trusted");
    const auto state = t.owner.settingSnapshot(0, static_cast<Setting>(c.reg));
    CHECK(state.trusted && state.observed && state.observed->completedMs > seedMs &&
          state.observed->endpoint.address == 5 && state.observed->value.rawWord == c.oldWord);
    CHECK(t.owner.endpointTrusted(0));
  } else {
    if (fault == Fault::LocalEcho) {
      t.reply(frame, AckOrigin::LocalEcho);
      CHECK(t.owner.activity().phase == OwnerPhase::Active && t.port.writes.size() == 2);
    }
    t.port.time = op.deadlineUs;
    done = t.poll();
    expect(done, "Primary TIMEOUT");
    expect(done, "Cleanup BUS_BUSY");
    expect(done, "FC06 ACK TIMEOUT");
    expect(done, "FC03 readback none");
    expect(done, "Endpoint at completion unconfirmed");
    expect(done, "historical, unconfirmed");
    if (fault == Fault::LocalEcho) {
      expect(done, "observed RX origin local echo");
    }
    t.historical(c, seedMs, true);
  }
  expect(done, "verified=false");
  expect(done, fault == Fault::Mismatch ? "transfers 2" : "transfers 1");
  CHECK(t.port.writes.size() == (fault == Fault::Mismatch ? 3u : 2u) &&
        !t.owner.stats().retryCount);
}
}  // namespace

int main(int argc, char** argv) {
  transcript = argc == 2 && std::string_view(argv[1]) == "--transcript";
  for (const std::string model : {"SEN0591", "SEN0592"}) {
    for (const auto& c : CASES) {
      matrix(model, c, false);
      matrix(model, c, true);
    }
    boundaries(model);
    settingLimits(model);
    extendedAndAliases(model);
    for (auto fault : {Fault::LostAck, Fault::LocalEcho, Fault::Exception, Fault::Mismatch}) {
      failure(model, fault);
    }
  }
}
