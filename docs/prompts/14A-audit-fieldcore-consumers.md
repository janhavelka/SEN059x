# 14A — Audit both FieldCore consumer contracts

## Start

Re-read original prompts 13–14 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md). Use [PROGRESS](PROGRESS.md) to locate each base and tested revision. Locate both native consumer fixtures and their source revisions. Compare each fixture's call signatures, result mapping and actual backend capabilities with its specific pinned FieldCore branch in the [source map](SOURCE_MAP.md).

## Inputs

Original prompts 13–14: [13](13-fieldcore-consumer.md), [14](14-fieldcore-revisions.md).

Consume `consumer-main`, `consumer-fixture`. Produce `consumer-audit`.

## Work

Assign separate reviewers the default and rodtester contracts. Have a third trace both results through the fixture’s installed-style public SEN059x headers, product status fields and parser-error translations. Check request identity, fixed-length/suffix versus qualified RTU evidence, cache age, duplicate completion and the five-rate product limit. Challenge any FC06 result that claims its own echo proved execution. Keep common fixtures small while preserving the behavior unique to each revision.

## Acceptance

Run both offline consumer modes from the [native suite](VERIFICATION.md#native-c-and-consumer-fixtures). For each revision show a successful measure, valid exception and cancellation. Verify retained results are consumed once and the asserted physical evidence can actually be represented by that revision. Record the fixture commit and pinned consumer SHAs.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary. Follow the [audit procedure](AUDIT.md). Reproduce findings at the public entry point and re-review the corrected diff.

## Handoff

Record `consumer-audit` in [PROGRESS](PROGRESS.md) with the reviewed revisions, evidence for each original, and the final checked revision. Finish with a concise report of the audit findings, fixes made, and final verification results.
