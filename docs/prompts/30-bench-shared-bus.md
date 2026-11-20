# 30 — Observe multidrop recovery and acoustic interference

## Start

Read [COMMON](COMMON.md) and the actual fixture entries in [PROGRESS](PROGRESS.md). First inventory the available device IDs, unique addresses, common serial format, topology and transceiver direction. Own the multidrop report. Carry out the read-only rows that this fixture enables.

## Inputs

Consume `owner-scheduler` and `runner-evidence`. Produce `bench-shared-bus`. Use the [hardware guide](../reference/hardware.md), [bus timing](../reference/protocol/Modbus_RTU_notes.md) and [coverage](COVERAGE.md).

Verify the independent audit handoff `runner-audit` before using the covered work.

## Work

1. With one owner, interleave operations on independently identified devices and registers. Record request order, timestamps, returned bytes, target, units, completion and sample freshness per binding. Vary slow processed and faster reads. Verify that a healthy peer is scheduled after settlement and retains its own samples and counters.
2. Where the bench supports it, observe a missed reply, disconnect and reconnect. Inject a late or malformed reply only with suitable isolated instrumentation. Distinguish a delayed response from a fresh response using available evidence. Record owner recovery, duplicate terminals, unconfirmed writes or stale data without upgrading an ambiguous observation.
3. With four uniquely addressed heads, vary firing sequence, target geometry and emitter placement. Record actual intervals, observed interference and which unit produced each reading. The product manual supplies no guaranteed cross-talk interval. Keep acoustic behavior separate from the electrical and protocol results.

## Acceptance

To verify fairness and cache isolation, run two or more simultaneously retained device bindings on the same shared owner using an instrumented bench application or an existing suitable product fixture. Successively selecting addresses via `useaddr` invalidates the previous console binding. Use that setup only for claims about individual addressed transfers or acoustic behavior.

Attach at least one attributable exchange per participant for any claimed multidrop result, alongside the schedule, firmware revision and capture resolution. With fewer than four heads, retain the supported electrical observations and mark the four-head acoustic rows untested. A fake transport supplies software evidence only.

## Delegation and self-audit

Re-read original prompt 30 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Use [AUDIT](AUDIT.md). Ask a subagent to reconstruct attribution from capture and schedule. Have an independent reviewer try to associate a late frame with the wrong binding and challenge any cross-talk claim that lacks a controlled sequence. Rerun valid checks and correct the report.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `bench-shared-bus` or its partial/blocked status in [PROGRESS](PROGRESS.md), including exact device count, tested recovery cases, raw evidence and open installation conditions.

Finish with a concise report of the audit findings, fixes made, and final verification results.
