#pragma once

#include "Exchange.h"

namespace SEN059x::example {

// All calls, including recovery, are bounded and serialized on the bus owner thread.
class SchedulerPort : public ExchangePort {
 public:
  // Passive adapter capability query. Must not reconfigure the UART or transmit.
  virtual bool supportsBaud(std::uint32_t baud) const = 0;
  // Select the host's 8N1 format on a settled line, in RX. Preserve the old rate on failure.
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
  // This ticket's raw observation, with quality, units and optional engineering value.
  // Quality stays Unobserved until a valid measurement reply completes the operation.
  Measurement measurement;
  // This ticket's addressed setting observation, present only for a qualified FC03 word.
  std::optional<ConfigurationObservation> settingObservation;
  // FC06 evidence, its verification and cleanup, retained together on the original ticket.
  std::optional<ConfigurationOperation> write;
};

struct TargetSelection {
  std::optional<DeviceModel> model;
  AddressPolicy policy = AddressPolicy::StandardUnicast;
  std::uint8_t address = 1;
  std::uint32_t baud = 115200;
};

enum class OwnerPhase { Idle, Queued, Active, Recovery };

struct OwnerActivity {
  OwnerPhase phase = OwnerPhase::Idle;
  // Next queued request, current wire request, or retired wire request during recovery.
  // Idle has no request, even when a terminal result is still waiting to be collected.
  std::optional<ExchangeRequest> request;
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
  // preserves the current binding and all work. Select DFRobotIsolated only on an isolated bus.
  Status bind(std::size_t slot, DeviceModel model, std::uint8_t address, std::uint32_t baud,
              AddressPolicy policy = AddressPolicy::StandardUnicast);
  void disable(std::size_t slot);
  // Passive view of an enabled binding, including an application-supplied initial selection.
  std::optional<TargetSelection> selectedTarget(std::size_t slot) const;
  bool supportsBaud(std::uint32_t baud) const { return port_.supportsBaud(baud); }
  // Select local context only on an idle, settled bus. An absent model is allowed before
  // the first bind. An explicit host baud command also applies 8N1 to the local adapter.
  Status selectLocalTarget(std::size_t slot, const TargetSelection& next, bool configureBaud);
  // Disable all bindings and cancel their work. Pending results and settlement survive reset.
  void reset();

  // deadlineUs is exclusive, absolute on port.nowUs(), and covers queueing and retries.
  // UINT64_MAX is reserved for the no-wake return from service().
  // retryOnce explicitly authorizes one new acquisition after a failed transmitted attempt.
  // A new distance/echo read starts ranging again. Only success assigns ticket.
  Status submit(std::size_t slot, MeasurementKind kind, std::uint64_t operationId,
                std::uint32_t step, std::uint64_t deadlineUs, bool retryOnce,
                std::uint64_t& ticket);
  // Read one documented setting at the binding's selected endpoint. Uses the same queue,
  // absolute deadline and ticket lifetime as submit(), with one attempt and no write. An
  // explicit endpoint must be the last known or the candidate from a possibly transmitted write.
  Status submitSettingRead(std::size_t slot, Setting setting, std::uint64_t operationId,
                           std::uint32_t step, std::uint64_t deadlineUs, std::uint64_t& ticket,
                           std::optional<Endpoint> at = std::nullopt);
  // Preview an addressed write with readback, or a vendor send under the isolated profile. Requires
  // an enabled binding. No grant or live observation is needed. Copies the proposed endpoints,
  // typed word and current setting history to out on success. Does no I/O or admission.
  // Preview and confirmation report ILLEGAL_VALUE for an out-of-range word and DISALLOWED
  // for a destination or assigned address excluded by the selected write policy.
  Status previewWrite(std::size_t slot, Setting setting, SettingValue value,
                      ConfigurationOperation& out, WriteScope scope = WriteScope::Addressed) const;
  // The caller declares a physically isolated unit. A grant covers one admission on this
  // binding and session. Call sessionActivity for operator activity and on session changes.
  Status grantIsolatedMaintenance(std::size_t slot, std::uint64_t session);
  void sessionActivity(std::uint64_t session);
  void maintenanceOff() { grant_.reset(); }
  // Confirm one of the eight typed settings. Requires a grant, an idle, settled owner and an
  // observed endpoint. The exclusive deadline covers TX and addressed verification or vendor
  // send settlement. Following an uncertain outcome, settle the wire before explicit readback.
  Status submitWrite(std::size_t slot, Setting setting, SettingValue value, std::uint64_t session,
                     std::uint64_t operationId, std::uint32_t step, std::uint64_t deadlineUs,
                     std::uint64_t& ticket, WriteScope scope = WriteScope::Addressed);
  bool cancel(std::uint64_t ticket);
  // Copy and release exactly one terminal result. Until taken it occupies an operation slot,
  // including across later traffic, rebind and reset.
  bool take(std::uint64_t ticket, ScheduledResult& out);
  // One bounded exchange step. Wake on UART events or by the returned absolute time in us.
  // UINT64_MAX means no scheduled wake. Recovery needs an explicit call.
  std::uint64_t service();

  // True when the wire is available to dispatch another operation.
  bool settled() const { return !exchange_; }
  // Passive snapshot of the shared bus, across all bindings. Leaves queue order and tickets intact.
  OwnerActivity activity() const;
  // On a retired exchange, poll physical drain once or consume one bounded UART batch.
  // A complete, independently sourced and framed reply under the sole old FC03 or FC06 context
  // settles a fully transmitted request without publishing. Returns true once settled.
  bool recover();
  // Alternate recovery for partial TX, lost/fragmented replies or failed hardware drain.
  // The adapter confirms physical isolation, RX and disposal of old callbacks (see above).
  bool recoverIsolated();

  const Stats& stats() const { return stats_; }
  // Same monotonic millisecond tick used to timestamp cached observations.
  std::uint32_t nowMs() const { return static_cast<std::uint32_t>(port_.nowUs() / 1000); }
  std::uint64_t physicalAttempts() const { return physicalAttempts_; }
  HealthSnapshot health(std::size_t slot) const;
  // Passive queries. Disabled and newly bound slots have no trusted endpoint or setting.
  bool endpointTrusted(std::size_t slot) const;
  // Last observed endpoint (initial selection before first contact) and the candidate left
  // by a possible address/baud write. Disabled slots return empty optionals.
  std::optional<Endpoint> endpoint(std::size_t slot) const;
  std::optional<Endpoint> possibleEndpoint(std::size_t slot) const;
  ConfigurationSnapshot settingSnapshot(std::size_t slot, Setting setting) const;
  SampleSnapshot snapshot(std::size_t slot, MeasurementKind kind, std::uint32_t nowMs,
                          std::uint32_t maxAgeMs) const;

 private:
  struct Binding {
    DeviceModel model{};
    std::uint8_t address = 0;  // Cache/health endpoint selected for traffic. Zero while disabled.
    std::uint32_t baud = 0;
    AddressPolicy policy{};
    MeasurementCache cache;
    Configuration configuration;
    DeviceHealth health;

    std::uint64_t generation() const {
      return cache.snapshot(MeasurementKind::ProcessedDistance, 0, 0).bindingGeneration;
    }
  };
  enum class State { Free, Queued, Active, Done };
  struct Operation {
    State state = State::Free;
    std::size_t binding = 0;
    bool retryOnce = false;
    ScheduledResult result;
  };

  std::optional<std::size_t> nextOperation() const;
  bool idle() const;
  Status setTarget(std::size_t slot, const TargetSelection& next, bool rebind, bool configureBaud);
  Status enqueue(std::size_t slot, std::uint16_t reg, bool measurement, std::uint64_t operationId,
                 std::uint32_t step, std::uint64_t deadlineUs, bool retryOnce,
                 std::uint64_t& ticket, const SettingValue* write = nullptr,
                 WriteScope scope = WriteScope::Addressed,
                 std::optional<Endpoint> at = std::nullopt);
  bool addressInUse(std::size_t slot, std::uint8_t address) const;
  Status selectEndpoint(Binding& binding, Endpoint at);
  Status startAttempt(Operation& op);
  void complete(Operation& op, Status status, std::uint64_t now, Status cleanup = Ok());
  Status beginVerification(Operation& op, ExchangeRequest request);
  void finishAttempt();
  std::uint64_t queueWake(std::uint64_t wake) const;
  SchedulerPort& port_;
  std::array<Binding, BINDINGS> bindings_{};
  std::array<Operation, OPERATIONS> operations_{};
  std::array<std::uint8_t, 11> rx_{};
  std::optional<Exchange> exchange_;  // Kept on the old baud throughout retirement.
  Operation* active_ = nullptr;
  ConfigurationToken settingRead_;  // Active setting read or write verification.
  ConfigurationToken writeToken_;   // Exclusive from admission through verification.
  struct Grant {
    std::size_t slot;
    std::uint64_t session;
    std::uint64_t lastActivityUs;
  };
  std::optional<Grant> grant_;
  bool needsDrain_ = false;
  std::size_t nextBinding_ = 0;
  std::uint64_t lastTicket_ = 0;
  std::uint64_t physicalAttempts_ = 0;
  Stats stats_{};
};

}  // namespace SEN059x::example
