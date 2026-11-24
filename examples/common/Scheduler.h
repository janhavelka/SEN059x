#pragma once

#include "Exchange.h"

namespace SEN059x::example {

// All calls, including recovery, are bounded and serialized on the bus owner thread.
class SchedulerPort : public ExchangePort {
 public:
  // Select the host's 8N1 format on a settled line, in RX. Return IO on configuration failure.
  virtual Status configure8N1(std::uint32_t baud) = 0;
  // Confirm externally performed recovery of this retired endpoint. Success guarantees
  // local TX physically stopped (drained or reset), DE in RX, the remote source isolated
  // or reset, all old bytes/events retired, and an idle RTU boundary. A flush, silence,
  // or a local ID alone is insufficient. Never send a sensor request here.
  virtual bool confirmIsolation(const ExchangeRequest& retired) = 0;
};

struct ScheduledResult {
  std::uint64_t ticket = 0;  // Unique for the lifetime of this scheduler.
  std::uint32_t step = 0;
  std::uint32_t attempts = 0;  // Transmissions with at least one byte accepted.
  bool queueExpired = false;   // Deadline reached while waiting to transmit.
  Status status{Err::NO_DATA, 0, "Operation pending"};
  std::uint64_t completedUs = 0;
  ExchangeResult exchange;  // Original request and last physical attempt, if any.
};

// One scheduler per exclusively owned, initially settled bus. UART callbacks only wake
// service(). Never feed a saved buffer directly into a binding. Keep the port and scheduler
// alive through physical settlement, including after logical cancellation or reset.
class Scheduler {
 public:
  static constexpr std::size_t BINDINGS = 4;
  static constexpr std::size_t OPERATIONS = 4;

  explicit Scheduler(SchedulerPort& port) : port_(port) {}
  // A successful bind starts a fresh generation even at the same address. A rejected bind
  // preserves the current binding and all work. Every binding uses standard unicast, 8N1.
  Status bind(std::size_t slot, DeviceModel model, std::uint8_t address, std::uint32_t baud);
  void disable(std::size_t slot);
  // Disable all bindings and cancel their work. Pending results and settlement survive reset.
  void reset();

  // deadlineUs is exclusive, absolute on port.nowUs(), and covers queueing and retries.
  // retryOnce explicitly authorizes one new acquisition after a failed transmitted attempt.
  // A new distance/echo read starts ranging again. Only success assigns ticket.
  Status submit(std::size_t slot, MeasurementKind kind, std::uint64_t operationId,
                std::uint32_t step, std::uint64_t deadlineUs, bool retryOnce,
                std::uint64_t& ticket);
  bool cancel(std::uint64_t ticket);
  // Copy and release exactly one terminal result. Until taken it occupies an operation slot,
  // including across later traffic, rebind and reset.
  bool take(std::uint64_t ticket, ScheduledResult& out);
  // One bounded exchange step. Wake on UART events or by the returned absolute time in us.
  // UINT64_MAX means no scheduled wake. Recovery needs an explicit call.
  std::uint64_t service();

  // True when the wire is available to dispatch another operation.
  bool settled() const { return !exchange_; }
  // On a retired exchange, poll physical drain once or consume one bounded UART batch.
  // A complete, independently sourced and framed reply under the sole old FC03 context
  // settles a fully transmitted request without publishing. Returns true once settled.
  bool recover();
  // Alternate recovery for partial TX, lost/fragmented replies or failed hardware drain.
  // The adapter confirms physical isolation, RX and disposal of old callbacks (see above).
  bool recoverIsolated();

  const Stats& stats() const { return stats_; }
  std::uint64_t physicalAttempts() const { return physicalAttempts_; }
  HealthSnapshot health(std::size_t slot) const;
  SampleSnapshot snapshot(std::size_t slot, MeasurementKind kind, std::uint32_t nowMs,
                          std::uint32_t maxAgeMs) const;

 private:
  struct Binding {
    DeviceModel model{};
    std::uint8_t address = 0;  // Zero while disabled.
    std::uint32_t baud = 0;
    MeasurementCache cache;
    DeviceHealth health;
  };
  enum class State { Free, Queued, Active, Done };
  struct Operation {
    State state = State::Free;
    std::size_t binding = 0;
    bool retryOnce = false;
    ScheduledResult result;
  };

  Operation* nextOperation();
  void complete(Operation& op, Status status, std::uint64_t now);
  void finishAttempt();
  std::uint64_t queueWake(std::uint64_t wake) const;
  SchedulerPort& port_;
  std::array<Binding, BINDINGS> bindings_{};
  std::array<Operation, OPERATIONS> operations_{};
  std::array<std::uint8_t, 11> rx_{};
  std::optional<Exchange> exchange_;  // Kept on the old baud throughout retirement.
  Operation* active_ = nullptr;
  bool needsDrain_ = false;
  std::size_t nextBinding_ = 0;
  std::uint64_t lastTicket_ = 0;
  std::uint64_t physicalAttempts_ = 0;
  Stats stats_{};
};

}  // namespace SEN059x::example
