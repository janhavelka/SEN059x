# 12A — Audit shared-bus scheduling and commissioning

## Start

Re-read original prompts 10–12 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md). Use [PROGRESS](PROGRESS.md) to locate each base and tested revision. Identify the committed and current owner, scheduler and commissioning changes. Review an actual owner call sequence with the scripted port and clock, including its TX-drain and reply-boundary evidence.

## Inputs

Original prompts 10–12: [10](10-shared-bus-owner.md), [11](11-bus-settlement.md), [12](12-commissioning-owner.md).

Consume `owner-exchange`, `owner-scheduler`, `owner-contract`. Produce `owner-audit`.

## Work

Assign one reviewer the ownership and lifetime of UART, direction, buffers and deadlines. In parallel, exercise a slow measurement and queued peer, cancellation at different TX phases, and a delayed identical FC03 reply. Inspect isolated commissioning through the same owner. Check that a preview or expiry before TX has zero effects, that a failed drain cannot silently free the line, and that a vendor FC06 transmission cannot mark the unit healthy or its setting verified. Look for duplicated recovery or scheduling code.

## Acceptance

Rerun the original [native owner tests](VERIFICATION.md#native-c-and-consumer-fixtures), including two live bindings, timer wrap, late callback and local echo. Inject a verification timeout after one FC06 and account for actual TX count, endpoint trust, retained terminal result and peer progress. Replay failures after any simplification.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary. Follow the [audit procedure](AUDIT.md). Reproduce findings at the public entry point and re-review the corrected diff.

## Handoff

Record `owner-audit` in [PROGRESS](PROGRESS.md) with the reviewed revisions, evidence for each original, and the final checked revision. Finish with a concise report of the audit findings, fixes made, and final verification results.
