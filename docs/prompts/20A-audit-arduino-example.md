# 20A — Audit both Arduino example builds

## Start

Re-read original prompts 19–20 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md) and use [PROGRESS](PROGRESS.md) to locate the original work. Identify the implementation commits for the port and each build environment. Inspect the example source, both PlatformIO profiles, board overrides, linked artifacts and changes since those commits.

## Inputs

Original prompts 19–20: [19](19-arduino-port.md), [20](20-arduino-peer-parity.md).

Consume `arduino-primary` and `arduino-example`. Produce `arduino-audit`.

## Work

Trace one operator command from `Serial` through the compiled shared registry to the independent sensor UART. Compare both profiles' source lists, resolved Arduino/Core versions and the provenance reported by the CLI. Check startup and passive queries for zero TX, bounded TX drain under a stalled port and pin-conflict rejection before TX. Look for duplicate handlers or accidental framework dependencies in the public core.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Acceptance

From separate tool environments, execute both full example builds and the original root and nested probe builds using the [Arduino verification commands](VERIFICATION.md#arduino-esp32-s3). Replay the [20 operation cases](OPERATIONS.csv) for both SKUs through the actual injected composition. Rerun the negative port case and record separate evidence for each ELF, host transcript and any attached-board observation.

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Have one reviewer follow source selection and public calls, another inspect transport timing and edge cases, and a third compare independent expected operations and test coverage. The lead reproduces each reported issue before changing the adapter or tests. Use the [audit procedure](AUDIT.md) to record the challenge and reruns.

## Handoff

Finish with a concise report of the audit findings, fixes made, and final verification results. Record `arduino-audit` in [PROGRESS](PROGRESS.md) with the two environment names, tested SHAs and artifacts, reproduced findings, corrections, exact reruns and resulting software or hardware evidence.
