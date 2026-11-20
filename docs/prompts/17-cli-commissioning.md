# 17 — Expose all commissioning operations in the human console

## Start

Follow [COMMON](COMMON.md). Use [PROGRESS](PROGRESS.md) to rerun an addressed read and the owner commissioning tests. Locate the real maintenance admission and readback entry points. Own shared commissioning handlers, their human help and native replay. Stop after the human path and its operator examples are verified.

## Inputs

Consume `owner-contract` and `cli-live`. Produce `cli-commissioning`. Implement the complete [configuration grammar](CLI_CONTRACT.md#configuration-commands-for-both-example-applications) and all eight write rows in the [operation cases](OPERATIONS.csv).

Verify the independent audit handoff `owner-audit` before using the covered work.

## Work

1. Implement maintenance status, one-shot isolated grant, preview and literal confirmation. Reuse public typed validation for every key, including switch polarity, threshold and UART unit. Accept baud in bps. Route `setaddr` and `setbaud` through the canonical handler. Generate per-setting help for units, SKU bounds, applicability and verification.
2. Use one addressed FC06 and independent same-register FC03 to verify a change. Preserve old/new endpoints, primary and cleanup outcomes, previous observed value/time, and current trust. When a write could have taken effect, mark that setting unconfirmed until addressed readback. Address/baud also affect endpoint trust.
3. In the isolated DFRobot profile expose extended addressed reads/writes and the separate explicit `broadcast set` form. Each confirmed send consumes one grant. Distinguish local transmission from independent verification. Mode-dependent physical output and persistence have separate evidence.

## Acceptance

Rebuild the native suite and run `ctest --test-dir .pio/sen059x-native -C Release --output-on-failure` with the real dispatcher and scripted owner. [Native build recipe](VERIFICATION.md)

Through ordinary human input run every preview, confirmed addressed write and vendor send-only case on both models. Check frames, endpoint for readback and TX counts. Exercise 248/254 and restoration, forbidden assignments, all eight baud codes, threshold/range bounds, both polarities and UART units, stale grants, device exception, lost ACK, mismatched readback and local echo. Test zero TX on preview/refusal. A send-only terminal stays unverified even after a later successful, separately addressed read.

## Delegation and self-audit

Re-read original prompt 17 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Apply the [stage audit](AUDIT.md). Delegate the eight-key transcripts. An independent reviewer should start with a verified setting, lose the ACK after TX and query cached settings. Fix any false trust and rerun the addressed and send-only variants.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `cli-commissioning` in [PROGRESS](PROGRESS.md) with test commands, per-SKU addressed and send-only coverage, restored-address scenario and operator transcript.

Finish with a concise report of the audit findings, fixes made, and final verification results.
