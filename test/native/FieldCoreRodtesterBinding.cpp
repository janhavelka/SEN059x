#include "FieldCoreRodtesterBinding.h"

#include <algorithm>
#include <limits>

#include "../../examples/common/ExchangeTiming.h"

namespace FieldCoreRodtester {
namespace {
using namespace FieldCoreMain;
using Reason = Rs485TransportTerminalReason;

bool sameStep(const Rs485StepIdentity& a, const Rs485StepIdentity& b) {
  return a.operationId == b.operationId && a.targetId == b.targetId &&
         a.generation == b.generation && a.address == b.address && a.step == b.step;
}
}  // namespace

Rs485BusObservation observe(const Rs485ModuleStep& step, const Delivery& d) {
  const auto& tx = step.transaction;
  Rs485BusObservation out;
  out.identity = tx.identity;
  out.clockValid = d.clockValid;
  out.observedUs = d.clockValid ? d.observedUs : 0;
  out.completedUptimeMs = d.observedUs / 1000;
  out.txCount = d.txAccepted;
  const bool failedDrain = d.txAccepted > 0 && !d.drained;
  out.busAvailable = !failedDrain;  // These scripted steps start on a ready bus.
  out.txDrainComplete = d.txAccepted > 0 && d.drained;
  out.txComplete = out.txDrainComplete && d.txAccepted == tx.txLength;
  out.rxBytes = d.bytes;
  const auto capacity = std::min(tx.rxCapacity, FieldCoreMain::kRs485TransactionRxCapacity);
  out.rxLength = d.bytes ? std::min(d.length, capacity) : 0;
  out.rxCount = out.rxLength;
  out.rxTotalCount = std::max<std::uint32_t>(d.totalBytes, d.length);
  out.wireErrors = d.wireErrors;
  if (failedDrain || d.txAccepted > tx.txLength) {
    out.wireErrors |= 1U << 3;  // Rs485WireBackend
  }
  if (failedDrain) {
    out.settlementError = d.drainError.code == ErrorCode::Ok
                              ? ErrorDetail{ErrorCode::InternalFault, 0}
                              : d.drainError;
  }
  if (out.rxLength != out.rxTotalCount) {
    out.wireErrors |= 1U << 4;  // Rs485WireTruncated
  }
  const bool validTxTime = tx.eligibleUs <= d.txAttemptEndUs && d.txAttemptEndUs <= d.observedUs;
  out.timingQualified = d.clockValid && out.txDrainComplete && validTxTime && d.timingQualified &&
                        out.wireErrors == 0 && d.txAttemptEndUs <= d.earliestUs &&
                        d.earliestUs <= d.latestUs && d.latestUs <= d.observedUs;
  if (out.timingQualified) {
    out.closureEarliestUs = d.earliestUs;
    out.closureLatestUs = d.latestUs;
  }
  if (validTxTime && d.txAttemptEndUs >= tx.deadlineUs) {
    out.reason = Reason::DeadlineExpired;
    out.error = {ErrorCode::DeadlineExpired, 0};
    return out;
  }
  if (!out.txComplete || !validTxTime) {
    out.reason = Reason::BackendError;
    out.error = failedDrain ? out.settlementError : ErrorDetail{ErrorCode::InternalFault, 0};
    return out;
  }
  if (out.wireErrors || !d.clockValid) {
    out.reason = Reason::BackendError;
    out.error = {ErrorCode::Rs485BadFrame, 0};
    return out;
  }
  // The owner starts its millisecond response timer after drain. Bound the
  // addition by the remaining command window before converting back to us.
  const auto drainMs = d.txAttemptEndUs / 1000;
  const auto waitMs = std::min<std::uint64_t>(tx.responseTimeoutMs, tx.deadlineUs / 1000 - drainMs);
  const auto responseDeadlineUs = (drainMs + waitMs) * 1000;
  if (out.timingQualified && d.latestUs <= responseDeadlineUs) {
    out.reason = Reason::Complete;
    out.responseConfirmed = tx.echoPolicy == Rs485EchoPolicy::QualifiedResponse;
  } else if (d.observedUs >= tx.deadlineUs) {
    out.reason = Reason::DeadlineExpired;
    out.error = {ErrorCode::DeadlineExpired, 0};
  } else if (d.observedUs >= responseDeadlineUs) {
    out.reason = Reason::ResponseTimeout;
    out.error = {ErrorCode::Timeout, 0};
  } else {
    out.reason = Reason::Complete;  // Closed by foreground silence, source unconfirmed.
  }
  return out;
}

bool Binding::takeStep(Rs485ModuleStep& step, std::uint64_t nowMs) {
  step = {};
  if (poll(nowMs) != DeviceModulePollState::Running) {
    return false;
  }
  if (phase_ == Phase::RetryWait) {
    step.action = Rs485ModuleAction::WaitUntil;
    step.wakeUptimeMs = wakeMs_;
    return true;
  }
  if (phase_ != Phase::Ready) {
    return false;
  }
  auto& tx = step.transaction;
  constexpr auto maxMs = std::numeric_limits<std::uint64_t>::max() / 1000;
  if (nowMs > maxMs || command_.deadline.expiresAtMs > maxMs) {
    finish(DeviceResultStatus::Failed, nowMs, {ErrorCode::InvalidArgument, 0});
    return false;
  }
  tx.eligibleUs = nowMs * 1000;
  tx.deadlineUs = command_.deadline.expiresAtMs * 1000;
  tx.format = format_;
  tx.format.baud = commandConfig_.baud;
  tx.responseTimeoutMs = commandConfig_.responseTimeoutMs;
  tx.echoPolicy = policy_;
  tx.txLength = buildRequest(tx.txBytes, sizeof(tx.txBytes));
  std::uint32_t interCharacterUs{};
  if (!tx.txLength ||
      !SEN059x::example::rtuTiming(tx.format.baud, interCharacterUs, tx.frameGapUs)) {
    finish(DeviceResultStatus::Failed, nowMs, {ErrorCode::InvalidArgument, 0});
    return false;
  }
  tx.identity = {command_.requestId, command_.deviceId, ++generation_, commandConfig_.address, 0};
  pending_ = tx;
  step.action = Rs485ModuleAction::RequestTransaction;
  phase_ = Phase::AwaitObservation;
  return true;
}

bool qualifiedResponse(const Rs485BusTransaction& pending, const Rs485BusObservation& obs) {
  return sameStep(obs.identity, pending.identity) && obs.reason == Reason::Complete &&
         pending.echoPolicy == Rs485EchoPolicy::QualifiedResponse && obs.responseConfirmed &&
         obs.rxBytes && obs.clockValid && obs.timingQualified && obs.txComplete &&
         obs.txDrainComplete && obs.txCount == pending.txLength && obs.wireErrors == 0 &&
         obs.rxTotalCount == obs.rxLength && obs.rxLength <= pending.rxCapacity &&
         obs.closureEarliestUs >= pending.eligibleUs &&
         obs.closureEarliestUs <= obs.closureLatestUs &&
         obs.closureLatestUs <= pending.deadlineUs && obs.closureLatestUs <= obs.observedUs;
}

void Binding::acceptObservation(const Rs485BusObservation& obs, std::uint64_t nowMs) {
  if (phase_ != Phase::AwaitObservation || !sameStep(obs.identity, pending_.identity)) {
    return;
  }
  FieldCoreMain::Rs485BusObservation read = obs;
  const bool qualified = qualifiedResponse(pending_, obs);
  if (!qualified && obs.reason != Reason::ResponseTimeout &&
      obs.reason != Reason::DeadlineExpired) {
    read.reason = Reason::BackendError;
  }
  consumeObservation(read, nowMs, qualified);
}

}  // namespace FieldCoreRodtester
