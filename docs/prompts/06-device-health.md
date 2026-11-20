# 06 — Track passive device health and transaction outcomes

## Inputs

Consume `measurements` and `configuration`. Apply [COMMON](COMMON.md). Use the health/status and counter definitions in [CONVENTIONS](CONVENTIONS.md), the [owner model](../ARCHITECTURE.md), TMP1x2's actual `HealthSnapshot` and SHZK's passive `Stats`. Produce `device-health`.

## Work

1. Add a caller-owned `DeviceHealth` per explicitly selected binding with `state()`, `isOnline()`, `healthSnapshot()`, `resetStatistics()`, `DriverState`, and `driverStateName`. Return a value snapshot with exactly the names and units in CONVENTIONS. Define an explicit local bind/teardown and a bounded observation entry point. A new binding begins `UNINIT` with no observations and no valid timestamps. Begin/bind/end, inspecting health or clearing totals perform no I/O. Losing a binding invalidates its cached sample/configuration authority. Stale completions from its previous generation cannot revive it.
2. Record one final communication outcome per **actual tracked exchange** the owner attempts on this binding. The codec itself never increments counters. A complete, attributable reply (including a valid addressed exception or an out-of-range raw value) proves an answering device. Transition to `READY`, mark the binding initialized for communication, clear `consecutiveFailures`, increment `totalSuccess` and record `lastOkMs` and validity. Keep the previous `lastError` and its time. An initial bind or an owner request merely accepted for TX cannot manufacture this observation.
3. An attributable failed exchange, including a response deadline after TX, increments `totalFailures` and `consecutiveFailures`, stores its `Status` in `lastError`, and updates the error time. Set `DEGRADED` until `offlineThreshold` consecutive failures and `OFFLINE` at or above it. Adopt TMP1x2's default 5 and positive configurable uint8_t threshold. Unanswered startup attempts may degrade or go offline while `initialized` remains false. After first successful contact preserve `initialized` across later failures on that binding. Define `isOnline()` as `initialized && (state == READY || state == DEGRADED)`. The owner may attempt a normal operation after OFFLINE. One attributable reply restores READY.
4. Make event attribution explicit. Foreign frames, bus scanning traffic, pending work, policy refusals, local queue expiry before TX, diagnostic renderings, cached reads and cancelled unstarted jobs never manufacture a sensor fault/success. Count an exchange once when resolved, even if intermediate noise, a CRC error and a deadline were all recorded for it. Distinguish owner-side transport availability from per-device response failure. An exact addressed exception stores `Err::EXCEPTION` and its byte on the operation result, independently of physical link success. A mismatching valid configuration readback similarly leaves the operation unverified.
5. Provide SHZK-compatible passive `Stats` for owners that want the common layout. Preserve the literal names, signed detail and reset behavior in CONVENTIONS. State the counting unit of each field. Apply saturating increments and use timestamp-valid flags wherever zero is a valid observed time. `resetStatistics()` clears only `totalFailures` and `totalSuccess`. Preserve consecutive count, state, last errors, validity, sample, and timestamps. The separate full `Stats::reset()` clears owner diagnostics. Keep operation generation, availability/freshness, pending write trust and FieldCore's aggregate policy independent of both structures.

## Acceptance

- Exercise the real API with two bindings and independent owner-fed times. Cover UNINIT, failures before first contact, READY, below/at-threshold DEGRADED/OFFLINE, recovery, repeated failures, isolated rebind and a late old-generation event. A receive timestamp of 0 is valid. Test elapsed-time arithmetic over uint32_t wrap and saturation of counters at their actual widths.
- For a read that times out, retries once and succeeds, expect device totals one failed and one successful exchange, owner `successCount=1`, `failCount=0`, `retryCount=1`, `timeouts=1`. For a terminal, addressed valid exception expect one physical success, one owner exception, one failed operation, its raw detail, and the previous measurement untouched. A local preview or health query changes none of these.
- Prove actual `resetStatistics()` leaves last-error evidence intact, a subsequent healthy read retains that fault history, and a failed or out-of-profile reading cannot acquire a fresh usable value. Test invalid threshold and unknown enum text. Keep UART clocks, retries, logging and locks outside this helper.

## Delegation and self-audit

Apply the [stage audit](AUDIT.md). Give the reviewer one successful read, one valid exception and one timeout in two independent bindings. Reconcile exchange counters, logical outcomes and sample ages before and after reset, rebind and recovery.

## Handoff

Record `device-health` in [PROGRESS](PROGRESS.md), including the transition/event table, binding lifetime, example counter deltas and the tested observation entry point that stage 07 will feed.
