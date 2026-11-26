#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Scheduler.h"

namespace SEN059x::example {

class ConsoleSink {
 public:
  virtual ~ConsoleSink() = default;
  // Make one nonblocking write. Return the number of leading bytes accepted (0..length).
  virtual std::size_t write(const char* bytes, std::size_t length) = 0;
};

struct ConsoleIdentity {
  // Application-owned, NUL-terminated build strings that outlive the console.
  // Missing values and values outside 1..64 printable ASCII bytes display as unknown.
  const char* firmwareRevision = nullptr;
  const char* dirty = nullptr;
  const char* framework = nullptr;
  const char* frameworkVersion = nullptr;
  const char* target = nullptr;
  const char* board = nullptr;
};

class HumanConsole {
 public:
  // Queue plain startup help and a prompt. Sink and scheduler are owned by the application.
  // The scheduler uses its injected port's monotonic clock.
  HumanConsole(ConsoleSink& sink, ConsoleIdentity identity, Scheduler& owner);
  HumanConsole(const HumanConsole&) = delete;
  HumanConsole& operator=(const HumanConsole&) = delete;

  // Consume one input byte. On false, keep the byte and retry after servicing output.
  bool feed(std::uint8_t byte);
  // Service the owner once and offer up to 64 console bytes to the sink once. Returns the
  // owner's next absolute wake in microseconds. Also service on sink readiness or UART events.
  std::uint64_t service();
  bool outputPending() const { return sent_ < used_; }

 private:
  void finishLine();
  void dispatch(const char* const* words, std::size_t count);
  void help();
  void showTarget(const TargetSelection& target);
  void status(const TargetSelection& target);
  void drv();
  void health();
  void stats();
  void settings();
  void selftest();
  void latest(std::string_view kind);
  void sampleHistory(const SampleHistory& history);
  void hexWord(std::uint16_t word);
  void hexByte(std::uint8_t byte);
  void append(std::string_view text);
  void info(std::string_view text);
  void error(std::string_view text);
  template <class Integer>
  void number(Integer value) {
    char bytes[21];  // Includes space for the sign of INT64_MIN.
    const auto result = std::to_chars(bytes, bytes + sizeof(bytes), value);
    field({bytes, static_cast<std::size_t>(result.ptr - bytes)});
  }
  void counterField(std::string_view name, std::optional<std::uint64_t> value);
  void statusError(Status status);
  void statusFields(Status status, bool statsLabels = false);
  void field(std::string_view text);
  void prompt();

  ConsoleSink& sink_;
  ConsoleIdentity identity_;
  Scheduler& owner_;
  std::array<char, 193> line_{};
  std::array<std::size_t, 10> starts_{};
  std::size_t length_ = 0;
  std::size_t tokens_ = 0;
  bool poisoned_ = false;
  bool afterCr_ = false;
  bool color_ = false;
  bool verbose_ = false;
  std::array<char, 2048> output_{};
  std::size_t used_ = 0;
  std::size_t sent_ = 0;
  bool overflow_ = false;
};

}  // namespace SEN059x::example
