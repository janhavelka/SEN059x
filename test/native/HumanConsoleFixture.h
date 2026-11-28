#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "HumanConsole.h"
#include "SchedulerFixture.h"

namespace console_test {
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
  HumanConsole console{sink,
                       {"abc123", "clean", "native-fw", "1.2", "host-cpu", "test-board"},
                       owner,
                       0x1234567890abcdefULL};

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

inline bool contains(const std::string& output, std::string_view field) {
  return output.find(field) != std::string::npos;
}

}  // namespace console_test
