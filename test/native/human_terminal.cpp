#include <algorithm>
#include <array>
#include <cstdio>

#include "HumanConsole.h"
#include "SchedulerFixture.h"

namespace {
using namespace SEN059x::example;

// Keep blocking desktop stdio outside the processor's cooperative write callback.
struct TerminalSink : ConsoleSink {
  std::array<char, 64> bytes{};
  std::size_t used = 0;

  std::size_t write(const char* src, std::size_t length) override {
    used = std::min(length, bytes.size());
    std::copy_n(src, used, bytes.begin());
    return used;
  }
};
}  // namespace

int main() {
  scheduler_test::NoTxPort port;
  Scheduler owner(port);
  TerminalSink sink;
  HumanConsole console(sink, {nullptr, nullptr, "standalone", nullptr, "desktop", nullptr}, owner);
  for (;;) {
    while (console.outputPending()) {
      console.service();
      if (std::fwrite(sink.bytes.data(), 1, sink.used, stdout) != sink.used) {
        return 1;
      }
      if (!console.outputPending() && std::fflush(stdout) != 0) {
        return 1;
      }
    }
    const int byte = std::getchar();
    if (byte == EOF) {
      return std::ferror(stdin) ? 1 : 0;
    }
    CHECK(console.feed(static_cast<std::uint8_t>(byte)));
  }
}
