# 32 — Audit the software delivery and record physical evidence

## Start

Use [COMMON](COMMON.md) and [PROGRESS](PROGRESS.md). Check the exact SHA and artifacts for the final build matrix and operator instructions. Own the delivery report and corrections needed to validate its claims. End with an accurate software and per-fixture handoff.

## Inputs

Consume `verified-package` and `operator-docs`. Review `bench-measurements`, `bench-commissioning` and `bench-shared-bus` when available, including any partial/blocked entries. Produce `delivery-review`.

Verify the independent audit handoff `package-audit` before using the covered work. Review `bench-audit` when available. If original hardware evidence exists ahead of its independent review, label it observed and awaiting audit. Keep missing fixture results open.

## Work

1. Trace the [20 operation cases](OPERATIONS.csv) on each SKU through public symbols, the shared dispatcher and both full console compositions. Keep host runs, firmware builds and connected boards distinct. Trace all conditional settings, all eight rates, send-only vendor writes, the contiguous `readregs` diagnostic, timing/CRC evidence and six published exchanges.
2. Check stale readings, exceptions, cancellation, owner recovery, uncertain write effects and cached trust. Compare the two FieldCore source revisions and preserve each one's limits. Reconcile literal status/health names, CLI grammar and package identities with the installed binaries or their recorded tests.
3. Reproduce selected user walkthroughs. Report the tested SHA and resolved versions for both Arduino profiles, native IDF console and both installed-package consumers. Identify each physical claim by SKU, fixture and trace. Software delivery can close with the named hardware questions open. A later observation reopens the affected claim.

## Acceptance

Run the applicable [verification checks](VERIFICATION.md) on the delivered revision and confirm the remote and CI result for that SHA. A missing setter or untested required software lane leaves software incomplete. Fix findings, rerun affected tests and update the ledger with the resulting evidence.

## Delegation and self-audit

Re-read original prompt 32 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Use [AUDIT](AUDIT.md). Assign a fresh reviewer one conditional write, a normal read, an extended address and a fault case on each SKU. Have another review the evidence behind a physical claim or mark its gap. The lead reproduces the findings and verifies the fixes.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `delivery-review` in [PROGRESS](PROGRESS.md) with checked SHA, artifact inventories, commands and job links, review outcomes, real product integration status and the open hardware rows. Bumping, tagging and publication belong to a requested release.

Finish with a concise report of the audit findings, fixes made, and final verification results.
