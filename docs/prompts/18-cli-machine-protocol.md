# 18 — Add machine sessions and prove dispatcher parity

## Start

Follow [COMMON](COMMON.md) and replay the confirmed-write and cached-query evidence in [PROGRESS](PROGRESS.md). Locate the actual terminal publication and bounded output queue. Own the shared machine renderer, request/session parser, help parity tests and example protocol transcripts. Stop when both renderers exercise the same commands and handlers.

## Inputs

Consume `cli-commissioning`. Produce `shared-cli`. Follow the exact [machine channel grammar and schema](CLI_CONTRACT.md#machine-channel-for-the-host-runner), [parser limits](CLI_CONTRACT.md#parser-and-cooperative-service) and [addressed operation matrix](OPERATIONS.csv).

## Work

1. Implement the versioned session handshake, boot identity, correlated request IDs, admission and terminal records. Enforce the precise prefix, line size, fields and canonical command names from the contract. Keep human output readable and machine records independently framed. Separate library revision/dirty state, firmware identity and actual framework/board information.
2. Use the same registry and handlers as the human terminal. Reserve bounded terminal storage before admitting work and handle stalled sinks cooperatively. Preserve original boot/session/request/operation attribution across cancellation and reattachment. Distinguish the cancellation request's outcome from its target's final disposition.
3. Publish a compact operator reference generated or checked against the registry and executable transcripts from the actual dispatcher on the scripted owner. Include help, live success/failure with signed detail, last-good age, conditional write/readback and an isolated send followed by independent verification.

## Acceptance

Rebuild the native suite and run `ctest --test-dir .pio/sen059x-native -C Release --output-on-failure`. Record the transcript replay invocation beside this result. [Native build recipe](VERIFICATION.md)

Replay all twenty addressed operations per SKU through human and machine inputs and compare TX bytes, transfer counts and typed outcomes. Cover eight isolated sends, block reads and jobs separately. Compare against six manufacturer exchanges and independent synthetic fixtures. Inject duplicate IDs, wrong session, stale generation, reboot, busy work, oversized lines and sink backpressure across completion. Assert each admitted job delivers one attributable terminal, human color cannot alter machine records and passive commands cause zero TX.

## Delegation and self-audit

Re-read original prompt 18 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Apply the [stage audit](AUDIT.md). Delegate schema fixture coverage. Have an independent reviewer stall the sink, reattach during a write and cancel under a different request ID. Fix and rerun any lost or misattributed terminal.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `shared-cli` in [PROGRESS](PROGRESS.md) with dispatcher paths, schema/version, native commands, operation totals and replayable transcripts for both renderers.

Finish with a concise report of the audit findings, fixes made, and final verification results.
