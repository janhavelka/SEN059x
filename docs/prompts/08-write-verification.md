# 08 — Model commissioning readback and uncertain effects

## Start

Apply [COMMON](COMMON.md), find `setting-access` in [PROGRESS](PROGRESS.md), and compile a caller that builds a setting write and its independent readback. Own the portable configuration evidence/state helpers and scripted tests. Stop at policy-neutral observations that a later bus owner can feed.

## Inputs

Consume `setting-access` and produce `configuration`. Use [configuration evidence](../reference/measurement-and-configuration.md), [wire notes](../reference/protocol/Modbus_RTU_notes.md) and [public results](CONVENTIONS.md).

## Work

1. Track desired value, last observed word/time, current trust, original/intended endpoint, TX progress, ACK shape/origin, readback comparison and cleanup separately. Factory defaults do not establish a live setting. Bind/recovery writes nothing.
2. For ordinary addressed writes plan FC03 at the same endpoint and register. Verify an address change at the new address using `0x0200` and compare its address word. For baud, keep the same address and verify `0x0201` at the new bps. The published address ACK uses the old address. Baud changes immediately and ACK baud is unknown. Preserve primary and cleanup results.
3. A preview, refusal or proven zero-byte TX leaves prior trust intact. Once a write may have reached the device, preserve historical observation/time but mark the setting unconfirmed. Address/baud writes also invalidate endpoint trust. A genuine ACK alone cannot restore it. An independent attributable FC03 can establish the observed word. Use `CONFIG_MISMATCH` or `LINK_LOST` with underlying evidence where appropriate.
4. A vendor send-only completion can establish local TX only. Preserve uncertainty on echo, silence, partial TX and unsolicited replies. A separate addressed observation can establish a later state. Keep actual UART changes, grants, retries and deadlines with the owner.

## Acceptance

Script all eight setting changes on both SKUs with matching readback. Start with verified state and inject lost/ambiguous ACK, wrong echoed register/value, mismatch, valid exception, power interruption, silent new link and failed cleanup. Check extended-endpoint restoration, zero-byte vs possible-TX trust, pending versus verified state and retention of original observations. No automatic write replay, rollback, scanning or persistence claim.

## Delegation and self-audit

Re-read original prompt 08 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Follow [AUDIT](AUDIT.md). Delegate event sequences and independent expected trust transitions. Have a separate reviewer lose an address ACK after TX and query state before and after verified readback. Fix the owning transition and rerun addressed and send-only cases.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `configuration` in [PROGRESS](PROGRESS.md) with the event-to-state transitions, API calls, endpoint/readback evidence and scoped test results required by the future owner.

Finish with a concise report of the audit findings, fixes made, and final verification results.
