#include <algorithm>
#include <limits>

#include "HumanConsole.h"

namespace SEN059x::example {
namespace {
constexpr auto NO_WAKE = std::numeric_limits<std::uint64_t>::max();
constexpr Status CANCELLED{Err::ABORTED, 0, "Operation cancelled"};
constexpr Status CLOCK_LIMIT{Err::INVALID_CONFIG, 0, "Deadline exceeds the owner clock"};
}  // namespace

void HumanConsole::startJob(ReadMode mode, std::uint16_t reg, std::uint16_t words,
                            unsigned requested, unsigned intervalMs) {
  if (active_ || owner_.activity().phase != OwnerPhase::Idle) {
    statusError({Err::BUS_BUSY, 0, "Finish active work and settle the line first"});
    return;
  }
  if (retained_ && retained_->id == NO_WAKE) {
    statusError({Err::INVALID_CONFIG, 0, "Operation IDs exhausted"});
    return;
  }
  Job job;
  job.id = retained_ ? retained_->id + 1 : 1;
  job.mode = mode;
  job.reg = reg;
  job.words = words;
  job.requested = requested;
  job.intervalUs = std::uint64_t{intervalMs} * 1000;
  job.target = owner_.localTarget(0);
  job.generation = owner_.snapshot(0, MeasurementKind::Temperature, 0, 0).bindingGeneration;
  auto status = owner_.readBudget(0, reg, words, job.budgetUs);
  if (status) {
    job.startedUs = job.dueUs = owner_.nowUs();
    status = submitStep(job, job.startedUs);
  }
  if (!status) {
    statusError(status);
    return;
  }
  active_ = job;
  append("[I] Accepted operation ");
  number(job.id);
  append(", ");
  field(*job.target.model == DeviceModel::SEN0591 ? "SEN0591" : "SEN0592");
  append(" address ");
  number(job.target.address);
  append(", register ");
  hexWord(reg);
  append(", timeoutMs ");
  number((job.budgetUs + 999) / 1000);
  append("\n");
}

Status HumanConsole::submitStep(Job& job, std::uint64_t now) {
  if (job.budgetUs >= NO_WAKE - now) {
    return CLOCK_LIMIT;
  }
  job.deadlineUs = now + job.budgetUs;
  const auto step = job.completed + job.failed + 1;
  switch (job.mode) {
    case ReadMode::Measurement:
    case ReadMode::Watch:
      return owner_.submit(0, static_cast<MeasurementKind>(job.reg), job.id, step, job.deadlineUs,
                           false, job.ticket);
    case ReadMode::Setting:
    case ReadMode::Diagnose:
      return owner_.submitSettingRead(0, static_cast<Setting>(job.reg), job.id, step,
                                      job.deadlineUs, job.ticket);
    case ReadMode::Raw:
      return owner_.submitRawRead(0, job.reg, job.words, job.id, step, job.deadlineUs, job.ticket);
  }
  return {Err::INVALID_CONFIG, 0, "Unknown read mode"};
}

void HumanConsole::finishJob(Status status, std::uint64_t now) {
  const auto bus = owner_.activity();
  active_->settlementRequired = bus.phase == OwnerPhase::Recovery && bus.request &&
                                bus.request->schedulerTicket == active_->lastTransferTicket;
  active_->outcome = status;
  active_->endedUs = now;
  retained_ = *active_;
  active_.reset();
  terminalPending_ = true;
}

std::uint64_t HumanConsole::serviceJob(std::uint64_t ownerWake) {
  if (!active_) {
    return ownerWake;
  }
  auto& job = *active_;
  const auto now = owner_.nowUs();
  if (job.ticket) {
    if (!owner_.take(job.ticket, job.last)) {
      return ownerWake;
    }
    job.ticket = 0;
    job.transfers += job.last.attempts;
    if (job.last.attempts) {
      job.lastTransferTicket = job.last.ticket;
    }
    if (job.mode == ReadMode::Diagnose && job.last.settingObservation &&
        job.last.settingObservation->value.rawWord != job.target.address && job.last.status) {
      job.last.status = {Err::CONFIG_MISMATCH, job.last.settingObservation->value.rawWord,
                         "Device address differs from the selected address"};
    }
    if (job.last.status) {
      ++job.completed;
    } else {
      ++job.failed;
      if (job.outcome) {
        job.outcome = job.last.status;
      }
    }
    if (job.last.status.code == Err::ABORTED || job.completed + job.failed == job.requested) {
      finishJob(job.last.status.code == Err::ABORTED ? job.last.status : job.outcome,
                job.last.completedUs);
      return ownerWake;
    }
    if (job.stopAfterStep) {
      finishJob(CANCELLED, now);
      return ownerWake;
    }
    // Plan the first interval at or after the actual completion. Missed ticks never queue reads.
    const auto elapsed = job.last.completedUs - job.dueUs;
    const auto periods =
        std::max<std::uint64_t>(1, elapsed / job.intervalUs + (elapsed % job.intervalUs != 0));
    if (periods > (NO_WAKE - 1 - job.dueUs) / job.intervalUs) {
      finishJob(CLOCK_LIMIT, now);
      return ownerWake;
    }
    job.skipped += periods - 1;
    job.dueUs += periods * job.intervalUs;
  }
  if (now < job.dueUs) {
    return std::min(ownerWake, job.dueUs);
  }
  // Service may itself arrive late. Send only the newest due acquisition.
  const auto missed = (now - job.dueUs) / job.intervalUs;
  job.skipped += missed;
  job.dueUs += missed * job.intervalUs;
  if (job.generation != owner_.snapshot(0, MeasurementKind::Temperature, 0, 0).bindingGeneration) {
    finishJob({Err::ABORTED, 0, "Watch binding changed"}, now);
    return ownerWake;
  }
  const auto status = submitStep(job, now);
  if (!status) {
    finishJob(status, now);
    return ownerWake;
  }
  return now;
}

void HumanConsole::jobStatus() {
  if (!active_ && !retained_) {
    info("No operation.");
    return;
  }
  const auto& job = active_ ? *active_ : *retained_;
  append("[I] Operation ");
  number(job.id);
  if (!active_) {
    append(" complete, deadline ");
    number(job.deadlineUs);
  } else if (job.ticket) {
    const auto bus = owner_.activity();
    const bool onWire = bus.phase == OwnerPhase::Active && bus.request &&
                        bus.request->schedulerTicket == job.ticket;
    append(onWire ? " running, deadline " : " queued, deadline ");
    number(job.deadlineUs);
  } else {
    append(" waiting, next read deadline ");
    number(job.dueUs);
  }
  append(" us\n");
}

void HumanConsole::jobResult(std::optional<std::uint64_t> id) {
  if (retained_ && (!id || *id == retained_->id)) {
    showResult(*retained_);
  } else {
    statusError({Err::NO_DATA, 0, "No retained terminal result for this operation"});
  }
}

void HumanConsole::cancelJob(std::optional<std::uint64_t> id) {
  if (!active_ || (id && *id != active_->id)) {
    statusError({Err::NO_DATA, 0, "No matching active operation"});
    return;
  }
  if (active_->ticket && !owner_.cancel(active_->ticket)) {
    info("Read already completed.");
    // Another owner service may have finished the current child before this control arrived.
    // Preserve that result, and stop a watch before it can submit its next child.
    if (active_->mode == ReadMode::Watch) {
      active_->stopAfterStep = true;
    }
    return;
  }
  append("[I] Cancellation requested for operation ");
  number(active_->id);
  append("\n");
  if (!active_->ticket) {
    finishJob(CANCELLED, owner_.nowUs());
  }
}

void HumanConsole::showResult(const Job& job) {
  append(job.outcome ? "[I] Operation " : "[E] Operation ");
  number(job.id);
  append(" ");
  statusFields(job.outcome);
  append(", ");
  field(*job.target.model == DeviceModel::SEN0591 ? "SEN0591" : "SEN0592");
  append(" address ");
  number(job.target.address);
  append(", binding generation ");
  number(job.generation);
  append(", register ");
  hexWord(job.reg);
  append(", elapsed ");
  number((job.endedUs - job.startedUs) / 1000);
  append(" ms, transfers ");
  number(job.transfers);
  append(job.settlementRequired ? ", wire settlement required\n" : ", transfer cleanup complete\n");
  if (job.mode == ReadMode::Watch) {
    append("[I] Watch requested ");
    number(job.requested);
    append(", completed ");
    number(job.completed);
    append(", failed ");
    number(job.failed);
    append(", skipped intervals ");
    number(job.skipped);
    if (job.last.ticket) {
      append(", last result ");
      statusFields(job.last.status);
    }
    append("\n");
  }
  if (job.mode == ReadMode::Raw) {
    append("[I] timeoutMs ");
    number((job.budgetUs + 999) / 1000);
    append(", count ");
    number(job.words);
    append("\n");
    if (job.last.status) {
      for (std::size_t i = 0; i < job.last.exchange.wordCount; ++i) {
        append("    ");
        hexWord(static_cast<std::uint16_t>(job.reg + i));
        append(" = ");
        hexWord(job.last.exchange.words[i]);
        append("\n");
      }
    }
  } else if (job.mode == ReadMode::Setting || job.mode == ReadMode::Diagnose) {
    if (job.mode == ReadMode::Diagnose) {
      append("[I] expected address ");
      number(job.target.address);
      append(", observed ");
      if (job.last.settingObservation) {
        number(job.last.settingObservation->value.rawWord);
        if (job.last.status) {
          append(" (addressed communication confirmed)");
        }
      } else {
        append("unknown");
      }
      append("\n");
    }
    for (const auto& entry : SETTING_KEYS) {
      if (static_cast<std::uint16_t>(entry.setting) == job.reg) {
        if (!job.last.status && !job.last.settingObservation) {
          info("Previous setting observation:");
        }
        settingLine(entry, job.last.settingHistory, false);
        if (const auto& observed = job.last.settingHistory.observed) {
          const auto completedMs = static_cast<std::uint32_t>(job.last.completedUs / 1000);
          append("    observation age at completion ");
          number(completedMs - observed->completedMs);
          append(" ms\n");
        }
        break;
      }
    }
  } else {
    sampleLine(static_cast<MeasurementKind>(job.reg), job.last.sampleHistory, !job.last.status);
  }
}
}  // namespace SEN059x::example
