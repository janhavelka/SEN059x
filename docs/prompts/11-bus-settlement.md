# 11 — Schedule shared-bus work and settle late traffic

## Start

Apply [COMMON](COMMON.md), find `owner-exchange` in [PROGRESS](PROGRESS.md), and run its fragmented-frame and drain-failure cases before extending it. Own multi-binding scheduling, cancellation/recovery and its native fake timelines. Stop after safe bus reuse and retained terminal results.

## Inputs

Consume `owner-exchange` and produce `owner-scheduler`. Read the [ownership notes](../ARCHITECTURE.md) and [late-frame rules](../reference/protocol/Modbus_RTU_notes.md).

## Work

1. Admit at most one outstanding wire exchange on the entire bus, including sensor ranging time. Bind each distinct, enabled address to its explicit SKU, serial format, generation, cache and observer. Reject duplicate active addresses and preserve pending work on competing admissions. Queue with a bounded deadline that includes wait time. Use a finite service step and allow other eligible work between settled exchanges.
2. Retain request/step identity through exactly one completion per accepted operation. Distinguish queue expiry from on-wire timeout. Keep the original deadline across explicitly permitted extra attempts. Retrying distance/echo starts a fresh acquisition. Count physical exchanges and logical completions separately.
3. On cancellation, disable, reset, partial TX or timeout, retire the old generation and settle the line before reuse. A fixed idle delay, buffer flush or local ID cannot attribute a late FC03. Defer indistinguishable follow-up until attribution can be restored. Keep a failed-drain line unavailable until explicit recovery. Document a concrete recovery route for same-address/same-shape traffic. Idle diagnostics, recovery and cache reads issue zero FC06.

## Acceptance

Interleave four distinct bindings and overlapping submissions. Exercise slow processed reply, exception, pre-TX queue expiry, disable/re-enable, cancellation during drain, stale callback, rollover, exactly-once retained completion and recovery. After a timed-out distance request ask the same device for temperature, inject the old reply, and prove it cannot publish a temperature. Verify a healthy peer, cache timestamps and owner counters throughout. Run native owner and exchange suites.

## Delegation and self-audit

Re-read original prompt 11 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Follow [AUDIT](AUDIT.md). Delegate adversarial interleavings and expected counter deltas. Have an independent reviewer cancel one request while another waits, then release its late bytes after rebind. Fix stale publication or unsafe reuse and repeat the scenario.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `owner-scheduler` in [PROGRESS](PROGRESS.md) with scheduling/settlement policy, explicit recovery call, tested timelines, pending-result lifetime and commands for reproducible native tests.

Finish with a concise report of the audit findings, fixes made, and final verification results.
