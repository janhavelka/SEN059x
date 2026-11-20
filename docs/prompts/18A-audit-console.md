# 18A — Audit the human console and machine protocol

## Start

Re-read original prompts 15–18 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md). Use [PROGRESS](PROGRESS.md) to locate each base and tested revision. Open the integrated shared dispatcher, its command registry, both renderers and real fake-owner transcripts. Use the public [CLI contract](CLI_CONTRACT.md) and [operation cases](OPERATIONS.csv) as the expected behavior.

## Inputs

Original prompts 15–18: [15](15-cli-console.md), [16](16-cli-live-commands.md), [17](17-cli-commissioning.md), [18](18-cli-machine-protocol.md).

Consume `cli-console`, `cli-live`, `cli-commissioning`, `shared-cli`. Produce `cli-audit`.

## Work

Divide the review across human command coverage, live and commissioning semantics, and machine-session correctness. Replay full input lines through the dispatcher. Try an oversized input line, a passive query during slow ranging, a preview, an ambiguous write, a replayed request ID and console backpressure. Check that one source of command names drives help and dispatch and that terminal outcomes retain boot, session, request and operation identity.

## Acceptance

Run the original native console and owner cases for both SKUs. Match the actual transfers for the 20 addressed operations, `readregs` and the isolated send-only paths. Check zero TX on previews and passive queries. A cancelled original operation must yield `ABORTED`/`Cancelled`, and a successful cancel request must keep its own identity.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary. Follow the [audit procedure](AUDIT.md). Reproduce findings at the public entry point and re-review the corrected diff.

## Handoff

Record `cli-audit` in [PROGRESS](PROGRESS.md) with the reviewed revisions, evidence for each original, and the final checked revision. Finish with a concise report of the audit findings, fixes made, and final verification results.
