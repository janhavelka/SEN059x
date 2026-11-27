#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "HumanConsole.h"

namespace {
using namespace SEN059x;
using namespace SEN059x::example;
using Clock = std::chrono::steady_clock;
using Microseconds = std::chrono::microseconds;

// One in-memory sensor. Addressed FC06 returns a simulated device ACK on the old link.
// Vendor FC06 updates the register silently. A new process restores the initial words.
class SimulatedPort : public SchedulerPort {
 public:
  const Clock::time_point epoch = Clock::now();
  std::uint64_t nowUs() const override {
    return std::chrono::duration_cast<Microseconds>(Clock::now() - epoch).count();
  }
  bool supportsBaud(std::uint32_t baud) const override {
    Baud code;
    return valueToBaud(baud, code);
  }
  Status configure8N1(std::uint32_t baud) override {
    baud_ = baud;
    return Ok();
  }
  void setTransmit(bool) override {}
  Status txDrained(bool& drained) override {
    drained = true;
    return Ok();
  }
  std::size_t write(const std::uint8_t* bytes, std::size_t length) override {
    size_ = 0;
    if (length != 8 || baud_ != deviceBaud_ || (bytes[0] != address_ && bytes[0] != 255)) {
      return length;
    }
    const auto crc = calcCrc16(bytes, 6);
    if (bytes[6] != static_cast<std::uint8_t>(crc) || bytes[7] != crc >> 8) {
      return length;
    }
    const unsigned start = (unsigned{bytes[2]} << 8) | bytes[3];
    if (bytes[1] == 6) {
      const auto entry = findRegister(start);
      if (entry == registers_.end() || start < 0x0200) {
        return length;
      }
      const auto word = static_cast<std::uint16_t>((unsigned{bytes[4]} << 8) | bytes[5]);
      if (start == 0x0201) {
        std::uint32_t rate = 0;
        if (!baudToValue(static_cast<Baud>(word), rate)) {
          return length;
        }
        deviceBaud_ = rate;
      } else if (start == 0x0200) {
        address_ = static_cast<std::uint8_t>(word);
      }
      entry->second = word;
      if (bytes[0] != 255) {
        std::copy_n(bytes, length, reply_.begin());
        size_ = length;
        readyUs_ = nowUs() + 20000;
      }
      return length;
    }
    if (bytes[0] == 255 || bytes[1] != 3 || bytes[4] != 0 || bytes[5] == 0 || bytes[5] > 3) {
      return length;
    }
    reply_[0] = address_;
    reply_[1] = 3;
    reply_[2] = 2 * bytes[5];
    for (unsigned i = 0; i < bytes[5]; ++i) {
      const auto word = findRegister(start + i);
      if (word == registers_.end()) {
        return length;
      }
      reply_[3 + 2 * i] = static_cast<std::uint8_t>(word->second >> 8);
      reply_[4 + 2 * i] = static_cast<std::uint8_t>(word->second);
    }
    size_ = 3 + reply_[2];
    const auto replyCrc = calcCrc16(reply_.data(), size_);
    reply_[size_++] = static_cast<std::uint8_t>(replyCrc);
    reply_[size_++] = static_cast<std::uint8_t>(replyCrc >> 8);
    readyUs_ = nowUs() + 20000;  // Synthetic delay gives the operator an asynchronous result.
    return length;
  }
  RxBatch read(std::uint8_t* dst, std::size_t capacity) override {
    RxBatch batch;
    if (size_ && nowUs() >= readyUs_) {
      batch.total = size_;
      batch.copied = std::min(size_, capacity);
      std::copy_n(reply_.data(), batch.copied, dst);
      batch.origin = AckOrigin::Device;
      batch.wireFrameStarted = batch.wireFrameClosed = true;
      size_ = 0;
    }
    return batch;
  }
  bool confirmIsolation(const ExchangeRequest&) override {
    size_ = 0;  // Reset the entire simulated line and discard its sole pending reply.
    return true;
  }

 private:
  using Registers = std::array<std::pair<unsigned, unsigned>, 12>;
  Registers::iterator findRegister(unsigned reg) {
    return std::find_if(registers_.begin(), registers_.end(),
                        [reg](const auto& entry) { return entry.first == reg; });
  }
  Registers registers_{{{0x0100, 754},
                        {0x0101, 751},
                        {0x0102, 300},
                        {0x010A, 5750},
                        {0x0200, 1},
                        {0x0201, 9},
                        {0x0205, 1},
                        {0x0206, 100},
                        {0x0208, 4},
                        {0x0209, 0},
                        {0x021A, 1},
                        {0x021F, 1}}};
  std::uint8_t address_ = 1;
  std::uint32_t deviceBaud_ = 115200;
  std::uint32_t baud_ = 0;
  std::uint64_t readyUs_ = 0;
  std::array<std::uint8_t, 11> reply_{};
  std::size_t size_ = 0;
};

// The owner holds mutex while feeding/servicing the console. Blocking stdio runs on its
// own threads. A stalled output writer leaves one bounded chunk pending in the sink.
struct TerminalIo : ConsoleSink {
  std::mutex mutex;
  std::condition_variable changed;
  std::array<std::uint8_t, 512> input{};
  std::size_t head = 0;
  std::size_t count = 0;
  bool eof = false;
  bool done = false;
  std::array<char, 64> output{};
  std::size_t used = 0;

  std::size_t write(const char* src, std::size_t length) override {
    if (used) {
      return 0;
    }
    used = std::min(length, output.size());
    std::copy_n(src, used, output.begin());
    return used;
  }

  void readInput() {
    int byte;
    while ((byte = std::getchar()) != EOF) {
      std::unique_lock lock(mutex);
      changed.wait(lock, [&] { return count < input.size(); });
      input[(head + count) % input.size()] = static_cast<std::uint8_t>(byte);
      ++count;
      changed.notify_all();
    }
    std::lock_guard lock(mutex);
    eof = true;
    changed.notify_all();
  }

  void writeOutput() {
    for (;;) {
      std::unique_lock lock(mutex);
      changed.wait(lock, [&] { return done || used != 0; });
      if (!used) {
        return;
      }
      const auto chunk = output;
      const auto size = used;
      used = 0;
      lock.unlock();
      changed.notify_all();
      if (std::fwrite(chunk.data(), 1, size, stdout) != size || std::fflush(stdout) != 0) {
        std::exit(EXIT_FAILURE);
      }
    }
  }
};
}  // namespace

int main() {
#ifdef _WIN32
  // A redirected 0x1A is an input byte. Text mode treats it as end-of-file.
  const int input = _fileno(stdin);
  if (!_isatty(input) && _setmode(input, _O_BINARY) == -1) {
    return 1;
  }
#endif
  std::fputs("Desktop FC03/FC06 simulator starts at address 1, 115200 baud.\n", stderr);
  SimulatedPort port;
  Scheduler owner(port);
  TerminalIo io;
  HumanConsole console(
      io, {nullptr, nullptr, "standalone", nullptr, "desktop", "FC03/FC06 simulator"}, owner);
  std::thread reader(&TerminalIo::readInput, &io);
  std::thread writer(&TerminalIo::writeOutput, &io);
  std::unique_lock lock(io.mutex);
  constexpr auto noWake = std::numeric_limits<std::uint64_t>::max();
  for (;;) {
    while (io.count && console.feed(io.input[io.head])) {
      io.head = (io.head + 1) % io.input.size();
      --io.count;
    }
    const auto wake = console.service();
    io.changed.notify_all();
    if (io.eof && io.count == 0 && !console.outputPending() && wake == noWake) {
      break;
    }
    const bool hadEof = io.eof;
    const auto ready = [&] {
      return (!hadEof && io.eof) || (console.outputPending() ? io.used == 0 : io.count != 0);
    };
    if (wake == noWake) {
      io.changed.wait(lock, ready);
    } else {
      io.changed.wait_until(lock, port.epoch + Microseconds(wake), ready);
    }
  }
  io.done = true;
  lock.unlock();
  io.changed.notify_all();
  reader.join();
  writer.join();
  return std::ferror(stdin) ? 1 : 0;
}
