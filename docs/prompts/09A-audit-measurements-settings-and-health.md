# 09A — Audit measurement, setting and health behavior

## Start

Re-read original prompts 05–09 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md). Use [PROGRESS](PROGRESS.md) to locate each base and tested revision. Find the revisions that introduced the model decoders, cache, typed settings, write verification and health observer. Open the public calls and their tests alongside the [operation cases](OPERATIONS.csv).

## Inputs

Original prompts 05–09: [05](05-model-measurements.md), [06](06-sample-freshness.md), [07](07-configuration-registers.md), [08](08-write-verification.md), [09](09-device-health.md).

Consume `measurement-values`, `measurements`, `setting-access`, `configuration`, `device-health`. Produce `device-audit`.

## Work

Have reviewers independently trace the four measurement reads and all eight setting reads and writes for each SKU, exercise independent bindings and challenge the observation counters. Check conditional switch/UART settings and ensure the engineer-facing result preserves raw uncertain data. Inspect pending writes after a possibly delivered FC06. Only attributable addressed FC03 readback establishes the observed value. Look for competing caches or redundant state flags that could disagree after rebind or cancellation.

## Acceptance

Run the five originals’ model, cache, configuration and health tests via the [native harness](VERIFICATION.md#native-c-and-consumer-fixtures). Reproduce a lost write ACK and a valid addressed exception using public calls. Record the historical value, trust flag, raw exception code, health totals and unchanged sample timestamp. Repeat the 20 addressed operation cases on each SKU.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary. Follow the [audit procedure](AUDIT.md). Reproduce findings at the public entry point and re-review the corrected diff.

## Handoff

Record `device-audit` in [PROGRESS](PROGRESS.md) with the reviewed revisions, evidence for each original, and the final checked revision. Finish with a concise report of the audit findings, fixes made, and final verification results.
