# 29 — Verify settings and restoration on an isolated unit

## Start

Follow [COMMON](COMMON.md) and [PROGRESS](PROGRESS.md). Run this prompt for one identified SKU on one authorized isolated fixture. Verify the rated supply and current endpoint. For each change, first read the original word and establish how to restore it. Leave the change untested if either prerequisite fails. Own the fixture report through observed restoration or an explicit unresolved endpoint.

## Inputs

Consume `runner-evidence`. Produce `bench-commissioning`. Use the [eight setting commands](CLI_CONTRACT.md#configuration-commands-for-both-example-applications), [cases](OPERATIONS.csv) and [manufacturer settings](../reference/measurement-and-configuration.md).

Verify the independent audit handoff `runner-audit` before using the covered work.

## Work

1. For every writable register on each available SKU record original value, requested word, actual FC06 bytes or exception, ACK evidence, separate same-register FC03, any externally observed physical effect, and restoration to the original value. Keep unauthorised or untested rows open.
2. For address and baud changes record old and intended endpoints, observed ACK baud when measurable, and read at the new endpoint. Check addresses 248–254 only in the isolated manufacturer profile. Verify restoration through addressed reads. Preserve uncertainty after a lost reply, partial transmission or an unmatched endpoint.
3. If permitted, test each isolated vendor `0xFF` send-only key separately. Record TX/drain/echo and any raw RX, then request an independent addressed read at a known endpoint. For switch polarity/threshold and UART units, separate register evidence from physical outputs absent on a four-wire device. Check that RS485 distances keep their units. An authorized power cycle supplies a separate persistence observation.
4. When a SEN0591 range level is changed for the test, capture its real-time and processed timing and associate the evidence with that verified setting. Restore the original level.

## Acceptance

Preview each authorized change and confirm it once. Use per-key pass/fail/untested rows for addressed writes and vendor sends. List all eight baud rates for that SKU, showing observed addressed readback or the specific untested condition for each. Record restoration and persistence independently. If restoration fails, surface the last verifiable state and stop further writes pending recovery. Attribute any observed switch or UART effect on a DYP fixture to that exact part.

## Delegation and self-audit

Re-read original prompt 29 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Apply [AUDIT](AUDIT.md). Delegate comparison of setting bounds and original/readback values. Have another reviewer challenge an echo-only success or a restoration claim lacking an addressed response. Correct conclusions and repeat only authorized observations.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `bench-commissioning` or its partial/blocked status in [PROGRESS](PROGRESS.md), with affected ports, raw evidence, original/current/unknown state and remaining observations.

Finish with a concise report of the audit findings, fixes made, and final verification results.
