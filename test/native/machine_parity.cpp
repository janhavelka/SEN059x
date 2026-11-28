#include <iostream>
#include <limits>
#include <sstream>

#include "HumanConsoleFixture.h"

namespace {
using namespace console_test;

// The script controls only peripheral bytes, the clock and sink capacity. The console under
// test receives every command via feed() and advances its actual scheduler via service().
struct RecordingPort : scheduler_test::Port {
  std::size_t write(const std::uint8_t* src, std::size_t length) override {
    const auto accepted = Port::write(src, length);
    std::printf("tx %u ", static_cast<unsigned>(baud));
    for (auto byte : writes.back()) {
      std::printf("%02x", static_cast<unsigned>(byte));
    }
    std::puts("");
    return accepted;
  }
};

void dumpOutput(const std::string& bytes) {
  std::printf("out ");
  for (unsigned char byte : bytes) {
    std::printf("%02x", static_cast<unsigned>(byte));
  }
  std::puts("");
}

void pump(TestTerminal<RecordingPort>& terminal) {
  for (unsigned i = 0; i < 4; ++i) {
    terminal.turn();
  }
  if (terminal.sink.limit) {
    terminal.flush();
  }
}
}  // namespace

int main() {
  TestTerminal<RecordingPort> terminal;
  terminal.flush();
  dumpOutput(terminal.sink.output);
  for (std::string line; std::getline(std::cin, line);) {
    terminal.sink.output.clear();
    if (!line.empty() && (line.front() == '>' || line.front() == '!')) {
      for (unsigned char byte : line.substr(1) + "\n") {
        CHECK(terminal.console.feed(byte));
      }
      if (line.front() == '>') {
        pump(terminal);
      }
    } else {
      std::istringstream input(line);
      std::string action;
      input >> action;
      if (action == "tick") {
        std::uint64_t delta = 0;
        CHECK(input >> delta);
        terminal.port.time += delta;
      } else if (action == "rx") {
        std::vector<std::uint8_t> reply;
        unsigned byte;
        while (input >> std::hex >> byte) {
          CHECK(byte < 256);
          reply.push_back(static_cast<std::uint8_t>(byte));
        }
        CHECK(!reply.empty() && !terminal.port.transmitting);
        terminal.port.deliver(std::move(reply));
      } else if (action == "limit") {
        CHECK(input >> terminal.sink.limit);
      } else if (action == "escaped-drain-fault") {
        // JSON escaping expands each of these adapter bytes to six bytes on the wire.
        static std::array<char, 129> message;
        message.fill('\x01');
        message.back() = '\0';
        terminal.port.drainStatus = {SEN059x::Err::IO, std::numeric_limits<std::int32_t>::min(),
                                     message.data()};
      } else if (action == "drain-fault") {
        terminal.port.drainStatus = {SEN059x::Err::IO, -59, "injected \"drain\" fault"};
      } else {
        CHECK(false && "unknown script instruction");
      }
      pump(terminal);
    }
    dumpOutput(terminal.sink.output);
  }
}
