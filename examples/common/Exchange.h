#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <SEN059x/SEN059x.h>

namespace SEN059x::example {

// Evidence for one bounded UART read. total includes any bytes consumed but dropped because
// the destination was full. An overrun with unknown loss sets overflow, even with total == 0.
struct RxBatch {
  std::size_t copied = 0;
  std::size_t total = 0;
  AckOrigin origin = AckOrigin::Unknown;
  // Longest observed silence between bytes of this candidate, including across batches.
  // Leave empty when only foreground delivery times are available.
  std::optional<std::uint32_t> wireGapUs;
  Status uartStatus;  // IO and platform detail for UART errors, including framing.
  // Qualified t3.5 after the last byte. Later bytes start a new frame.
  bool wireFrameClosed = false;
  bool overflow = false;
  // Driver observed an RTU frame boundary before the first byte in this batch. Together
  // with wireFrameClosed this identifies one whole frame for retired-request recovery.
  bool wireFrameStarted = false;
};

// The application supplies one exclusively owned 8N1 UART, its direction pin and its clock.
// Every call must be bounded. Keep RX events (including possible TX echo) across DE changes.
class ExchangePort {
 public:
  virtual ~ExchangePort() = default;
  virtual std::uint64_t nowUs() const = 0;  // Monotonic microseconds, same epoch as deadlineUs.
  virtual void setTransmit(bool enabled) = 0;
  // Copy at most length bytes before returning. Report the count actually accepted.
  virtual std::size_t write(const std::uint8_t* bytes, std::size_t length) = 0;
  // Sample once. True means the last stop bit has left the UART. IO carries driver detail.
  virtual Status txDrained(bool& drained) = 0;
  // Consume at most one bounded batch. An empty successful read means the RX queue has been
  // checked and is empty at this poll. Even with capacity zero, account for queued bytes.
  // Device origin excludes local echo on the settled link. Buffered delivery alone qualifies
  // neither origin nor wire timing. Report qualified gap/closure using this baud's RTU limits.
  virtual RxBatch read(std::uint8_t* dst, std::size_t capacity) = 0;
};

struct ExchangeRequest {
  std::uint64_t operationId = 0;
  std::uint64_t bindingGeneration = 0;
  RequestContext context;
  std::uint32_t baud = 0;
  std::uint64_t deadlineUs = 0;     // Exclusive absolute deadline. See exchangeTiming().
  bool publishMeasurement = false;  // Otherwise FC03 returns ordered diagnostic words.
};

struct ExchangeResult {
  ExchangeRequest request;
  Status status{Err::NO_DATA, 0, "Exchange pending"};
  std::size_t txAccepted = 0;
  bool txDrained = false;
  std::size_t rxAccessible = 0;      // Bytes available in the caller's RX buffer for this frame.
  std::size_t rxTotal = 0;           // Observed bytes for this frame, saturates on overflow.
  std::uint64_t lastDeliveryUs = 0;  // Foreground clock, meaningful when rxTotal > 0.
  std::uint64_t completedUs = 0;     // Foreground terminal time, meaningful when done().
  bool overflow = false;
  AckOrigin origin = AckOrigin::Unknown;
  bool wireGapError = false;
  Status uartStatus;
  bool wireFrameClosed = false;  // False for closure based only on foreground observations.
  ReadResponseError readError{};
  WriteResponseError writeError{};
  std::array<std::uint16_t, 3> words{};  // Only wordCount entries are published.
  std::size_t wordCount = 0;
};

class Exchange {
 public:
  // Keep the port and writable RX buffer alive and exclusively owned through completion.
  // Storage of 11 bytes fits the longest documented FC03 reply. A smaller buffer is allowed
  // and produces overflow if bytes are lost. TX storage belongs to this object.
  Exchange(ExchangePort& port, std::uint8_t* rx, std::size_t capacity);
  Exchange(const Exchange&) = delete;
  Exchange& operator=(const Exchange&) = delete;

  // Admit one already authorized exchange on a settled bus, with the UART set to request.baud
  // and DE in receive. Use exchangeTiming() to budget the absolute deadline, including an
  // explicit device allowance for a block diagnostic or setting access. Bind the health
  // record and optional measurement cache to the selected SKU/endpoint and keep them exclusive
  // through done(). Service an admitted exchange through completion before destroying it.
  // The caller settles late traffic before reuse and recovers a failed drain.
  Status begin(ExchangeRequest request, DeviceHealth& health, MeasurementCache* cache = nullptr);

  // Call on UART events or by the returned absolute wake time in microseconds. A progress step
  // may request immediate service. Idle and completed exchanges return UINT64_MAX.
  std::uint64_t service();
  // Retire publication and health tokens immediately. Leaves DE and buffered traffic alone.
  // The caller must settle any accepted TX and possible response before the next admission.
  void cancel();
  bool done() const { return phase_ == Phase::Done; }
  const ExchangeResult& result() const { return result_; }  // Retained until next admission.

 private:
  enum class Phase { Idle, Send, Drain, Receive, Done };
  std::uint64_t receive(std::uint64_t readStart);
  void parse(std::uint64_t now);
  void finish(Status status, std::uint64_t now, bool attributable = true);
  std::uint64_t nextWake(std::uint64_t now) const;

  ExchangePort& port_;
  std::uint8_t* rx_;
  std::size_t capacity_;
  Phase phase_ = Phase::Idle;
  std::array<std::uint8_t, 8> tx_{};
  ExchangeResult result_;
  DeviceHealth* health_ = nullptr;
  HealthAttempt attempt_;
  MeasurementCache* cache_ = nullptr;
  SampleRequest sample_;
  std::uint32_t interCharacterUs_ = 0;
  std::uint32_t closureUs_ = 0;
  bool ignoredFrame_ = false;
};

}  // namespace SEN059x::example
