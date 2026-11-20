# 04A — Audit the baseline, public API and RTU codec

## Start

Re-read original prompts 01–04 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md). Use [PROGRESS](PROGRESS.md) to locate each base and tested revision. Inspect the baseline, exported headers, compiled definitions, native tests and both SKU reference tables at the checkout being audited.

## Inputs

Original prompts 01–04: [01](01-baseline-and-coverage.md), [02](02-value-status-contract.md), [03](03-read-rtu-codec.md), [04](04-write-rtu-codec.md).

Consume `baseline`, `value-status`, `read-codec`, `rtu-codec`. Produce `codec-audit`.

## Work

Give one reviewer the four manufacturer editions and the six published exchanges to derive the expected addresses, permissions, lengths and bytes independently. Have another trace request construction and response validation through the installed public API. A third should try a CRC-correct foreign reply, truncated and oversized frames, and an FC06 echo with the wrong register or value. Look for duplicated CRC or length checks that diverge between FC03 and FC06.

## Acceptance

Run `python scripts/fetch_originals.py --verify`, `python examples/python/sen059x_probe.py --self-test`, Python discovery and `python scripts/generate_version.py check`. Rebuild and run the [native consumer and codec tests](VERIFICATION.md#native-c-and-consumer-fixtures). Repeat the original package check and link its installed consumer so a source-tree-only header cannot pass. Prove a rejected frame preserves caller outputs and that an ordinary unicast builder rejects 248 while the isolated path correctly encodes it.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary. Follow the [audit procedure](AUDIT.md). Reproduce findings at the public entry point and re-review the corrected diff.

## Handoff

Record `codec-audit` in [PROGRESS](PROGRESS.md) with the reviewed revisions, evidence for each original, and the final checked revision. Finish with a concise report of the audit findings, fixes made, and final verification results.
