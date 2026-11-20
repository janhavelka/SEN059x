# 32A — Audit the final delivery and operator instructions

## Start

Re-read original prompts 31–32 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md) and use [PROGRESS](PROGRESS.md) to locate the original work. Identify the revisions of the published user instructions, original delivery report and corresponding implementation. Inspect the current public files, installed archive, exact commit and attached build/evidence records before evaluating the release claims.

## Inputs

Original prompts 31–32: [31](31-operator-documentation.md), [32](32-delivery-review.md).

Consume `operator-docs` and `delivery-review`. Produce `delivery-audit`.

## Work

Perform a cold read as a portable library consumer, a console operator and a shared-bus maintainer. Trace each claimed feature to the public entry point, reference, test and real evidence. Check both SKUs and both example compositions, raw blocks, mode-specific writes, extended addresses, uncertain effects, status literals, per-profile versions and outstanding physical rows. Preserve the distinction between software delivery, real FieldCore integration and observed hardware.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Acceptance

Compile and run the documented minimal caller from an installed archive. Execute the quick-start replay, a conditional write preview and the documented lost-ACK recovery with a scripted device. Run `python scripts/check_promptset.py`, `python -m unittest discover -s test -v` and the affected [delivery checks](VERIFICATION.md) on the final revision. Verify job URLs, archive identifiers and remote SHA. Distinguish independently audited observations from available evidence still awaiting 30A. Record unsupported physical claims as open rows. [Coverage](COVERAGE.md)

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Give parallel reviewers the walkthroughs, the software-to-evidence mapping, and the physical/provenance claims. Have the lead reproduce conflicting results from files and traces. Resolve confirmed gaps within the two original prompts and repeat affected software checks after any fix. Use the [audit procedure](AUDIT.md) to record the challenge and reruns.

## Handoff

Finish with a concise report of the audit findings, fixes made, and final verification results. Record `delivery-audit` in [PROGRESS](PROGRESS.md) with the audited SHA, public walkthrough and evidence paths, findings resolved, exact passing/failing/blocked checks and any remaining per-SKU physical obligations.
