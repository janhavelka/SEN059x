#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "JsonWriter.h"
#include "Scheduler.h"

namespace SEN059x::example {

struct SampleKind {
  std::string_view name;
  MeasurementKind kind;
};
inline constexpr SampleKind SAMPLE_KINDS[] = {
    {"processed", MeasurementKind::ProcessedDistance},
    {"realtime", MeasurementKind::RealTimeDistance},
    {"temperature", MeasurementKind::Temperature},
    {"echo", MeasurementKind::EchoTime},
};
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
  // Start a fresh operator session, release any prior grant and queue plain help and a prompt.
  // The application owns the sink and scheduler and supplies a new bootId each boot.
  // The scheduler uses its injected port's monotonic clock.
  HumanConsole(ConsoleSink& sink, ConsoleIdentity identity, Scheduler& owner, std::uint64_t bootId);
  HumanConsole(const HumanConsole&) = delete;
  HumanConsole& operator=(const HumanConsole&) = delete;

  // Consume one input byte. On false, keep the byte and retry after servicing output.
  bool feed(std::uint8_t byte);
  // Readiness for this particular byte, including machine controls during output backpressure.
  bool canFeed(std::uint8_t byte) const;
  // Service the owner once and offer up to 64 console bytes to the sink once. Returns the
  // earlier of the next owner or watch wake in absolute microseconds. Also service on sink
  // readiness or UART events. A terminal result survives output stalls and later reads.
  std::uint64_t service();
  bool outputPending() const { return sent_ < used_ || terminalPending_; }

 private:
  void finishLine();
  void machineLine(const char* const* words, std::size_t count);
  void beginRecord();
  struct Job;
  void finishRecord(const Job* terminal = nullptr);
  void identityData();
  void targetData(const TargetSelection& target);
  void jsonStatus(Status status);
  void cacheData(std::string_view cmd, std::string_view arg = {});
  void sampleData(MeasurementKind kind, const SampleSnapshot& snapshot, const TargetSelection& at);
  void previewData(const ConfigurationOperation& plan);
  void dispatch(const char* const* words, std::size_t count);
  void help();
  void settingHelp(const SettingKey& key, std::string_view usage, bool write, WriteScope scope);
  void writeCommand(std::string_view key, std::string_view input, bool confirm, WriteScope scope);
  void showTarget(const TargetSelection& target);
  void status(const TargetSelection& target);
  void drv();
  void health();
  void stats();
  void settings();
  void settingLine(const SettingKey& key, const ConfigurationSnapshot& snapshot,
                   bool includeDesired);
  void selftest();
  void latest(std::string_view kind);
  void sampleLine(MeasurementKind kind, const SampleSnapshot& snapshot, bool previous);
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

  enum class JobMode { Measurement, Setting, Diagnose, Raw, Watch, Write };
  struct Job {
    std::uint64_t id = 0;
    std::array<char, 33> nonce{};
    std::uint32_t req = 0;  // Zero for jobs admitted from human input.
    std::string_view cmd;   // Static canonical name from the registry.
    JobMode mode = JobMode::Measurement;
    TargetSelection target;
    std::uint64_t generation = 0;
    std::uint16_t reg = 0;
    std::uint16_t words = 1;
    unsigned requested = 1;
    unsigned completed = 0;
    unsigned failed = 0;
    unsigned unusable = 0;
    unsigned transfers = 0;
    std::uint64_t skipped = 0;
    std::uint64_t intervalUs = 0;
    std::uint64_t startedUs = 0;
    std::uint64_t dueUs = 0;
    std::uint64_t budgetUs = 0;
    std::uint64_t deadlineUs = 0;
    std::uint64_t ticket = 0;  // Zero between watch readings.
    std::uint64_t lastTransferTicket = 0;
    ScheduledResult last;
    Status outcome;  // First failure, or cooperative cancellation.
    std::uint64_t endedUs = 0;
    bool settlementRequired = false;
    bool stopAfterStep = false;
  };
  void jobData(const Job& job, bool terminal);
  void terminalRecord(const Job& job);
  void envelope(std::string_view nonce, std::uint32_t req, std::string_view cmd, std::uint64_t op,
                const Status* status);
  void startJob(JobMode mode, std::uint16_t reg, std::uint16_t words = 1, unsigned requested = 1,
                unsigned intervalMs = 0);
  void startWrite(const ConfigurationOperation& plan);
  void admitJob(Job& job, const ConfigurationOperation* plan);
  Status submitStep(Job& job, std::uint64_t now, const ConfigurationOperation* plan = nullptr);
  std::uint64_t serviceJob(std::uint64_t ownerWake);
  void finishJob(Status status, std::uint64_t now);
  void jobStatus();
  void jobResult(std::optional<std::uint64_t> id);
  void cancelJob(std::optional<std::uint64_t> id);
  void showResult(const Job& job);
  void showWriteResult(const ScheduledResult& result);

  ConsoleSink& sink_;
  ConsoleIdentity identity_;
  Scheduler& owner_;
  // Noncopyable console instances have distinct addresses while their sessions overlap.
  std::uint64_t grantSession() const {
    return reinterpret_cast<std::uintptr_t>(machine_ ? static_cast<const void*>(&nonce_) : this);
  }
  std::array<char, 17> boot_{};
  std::array<char, 33> nonce_{};
  std::uint32_t highRequest_ = 0;
  std::uint64_t protocolErrors_ = 0;
  std::uint64_t droppedOutput_ = 0;
  bool humanLineOpen_ = false;
  // Valid only for the synchronous dispatch. A Job copies its origin before admission.
  bool machine_ = false;
  std::uint32_t request_ = 0;
  std::string_view command_;
  std::uint64_t admitted_ = 0;
  Status replyStatus_;
  JsonWriter json_;
  std::array<char, 193> line_{};
  std::array<std::size_t, 10> starts_{};
  std::size_t length_ = 0;
  std::size_t tokens_ = 0;
  bool poisoned_ = false;
  bool afterCr_ = false;
  bool color_ = false;
  bool verbose_ = false;
  std::optional<Job> active_;
  std::optional<Job> retained_;
  std::optional<Job> lastWrite_;  // Preserve the write terminal across separate readback jobs.
  bool terminalPending_ = false;
  static constexpr std::size_t MAX_RECORD = 2048;  // Includes prefix and final LF.
  // Credit for an admission and two control replies, even when the sink stalls mid-record.
  std::array<char, 3 * MAX_RECORD + 1> output_{};
  std::size_t used_ = 0;
  std::size_t sent_ = 0;
  bool overflow_ = false;
};

}  // namespace SEN059x::example
