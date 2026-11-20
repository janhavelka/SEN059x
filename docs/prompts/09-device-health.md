# 09 — Track passive device health and operation statistics

## Start

Apply [COMMON](COMMON.md). Check the measurement cache and configuration observation APIs recorded in [PROGRESS](PROGRESS.md), then replay one good observation and one failed readback in the native fixtures. Own portable `DeviceHealth`/`Stats` and tests. Stop with explicit observation inputs and query-only diagnostics.

## Inputs

Consume `measurements` and `configuration` and produce `device-health`. Read all [health fields, states and counter definitions](CONVENTIONS.md) and the pinned TMP1x2/SHZK implementations in the [source map](SOURCE_MAP.md).

## Work

1. Implement caller-owned `DeviceHealth` per binding, `state()`, `isOnline()`, `healthSnapshot()`, `driverStateName` and `resetStatistics()`. Bind starts `UNINIT` with invalid times. Track one outcome per attempted exchange, supplied by the owner. A complete attributable reply (including an exception or out-of-range word) sets `READY`, establishes initialization for communication, increments `totalSuccess` and clears the failure streak. Retain last-error history.
2. Attributable failures increment both failure counts and set `DEGRADED` below the positive configurable threshold (default 5), then `OFFLINE`. Preserve initialization across later losses on the same binding. `isOnline()` requires initialization and READY or DEGRADED. One subsequent attributable reply restores READY. Allow an owner to attempt another exchange after OFFLINE. Keep timestamps valid independently and saturate counters.
3. Implement SHZK-compatible `Stats` for logical operations, extra attempts and specific attempt errors. Inspect/reset perform zero I/O. `resetStatistics()` clears cumulative device success/failure totals only. `Stats::reset()` clears owner diagnostics. Old-generation completions, idle polls, foreign bytes, local refusals, expiry before TX and cancelled unstarted jobs create zero device outcomes. A failed operation and a responding device can coexist.

## Acceptance

Keep the setting name `offlineThreshold`, a positive `uint8_t` with default 5. Verify bind and teardown produce zero I/O, a rebind starts with no previous health/sample/configuration authority, and a late completion from the old binding cannot restore it.

Run native tests with two bindings, first-contact failures, below/at threshold, recovery, invalid threshold, time zero, uint32_t wrap, saturation, reset and old-generation RX. Script timeout→retry→success: device 1 fail/1 success, owner one success, one retry, one timeout. A terminal addressed exception yields one communication success, one exception, one operation failure and no new usable sample. Check UART-free queries and both reset behaviors.

## Delegation and self-audit

Re-read original prompt 09 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Follow [AUDIT](AUDIT.md). Delegate peer field/counter comparison. Have a separate reviewer challenge double-counting on a noisy timeout and read the cache after a valid exception. Fix attribution and rerun both counter sequences.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `device-health` in [PROGRESS](PROGRESS.md) with transition and attribution rules, snapshot/reset API, counter deltas, timestamp behavior and the native test command.

Finish with a concise report of the audit findings, fixes made, and final verification results.
