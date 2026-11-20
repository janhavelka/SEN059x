# 31 — Publish the tested library, console and integration walkthroughs

## Start

Apply [COMMON](COMMON.md) and read the runner and package results in [PROGRESS](PROGRESS.md). Open the root README as a first-time user. Own the README, example guides, IDF usage notes, API documentation and unreleased changelog. Finish with walkthroughs that work using the public installation.

## Inputs

Consume `consumer-fixture`, `runner-evidence` and `version-provenance`. Produce `operator-docs`. Use [CLI_CONTRACT](CLI_CONTRACT.md), the [version profiles](VERSIONS.md) and [coverage](COVERAGE.md).

Verify the independent audit handoffs `consumer-audit` and `runner-audit` before using the covered work.

## Work

1. Show a minimal framework-free C++17 caller-owned TX/RX example, explicit SKU/binding, error detail and a passive cached reading. Explain sample validity, age and the division of ownership between sensor library, line owner and bigger firmware. Link source-pinned FieldCore fixture limitations.
2. Give the Arduino and IDF build/monitor commands, pin overrides, separate bus/console connections, model/supply selection and first read. Link the complete grouped command reference. Include one real dispatcher transcript, a conditional setting preview, isolated readback/recovery and the host runner invocation. Distinguish console port versus direct USB–RS485 probe.
3. Document installed headers, both package checks, versions, mode-specific features and known physical uncertainties. Cross-check package ownership, maintainer and license. Render Doxygen and document the revision and evidence for any claimed physical result.

## Acceptance

Compile the README snippet from an installed archive. Run copied commands using the host fixture or corresponding SDK build. Compare the help transcript and API declarations with the operator docs. Verify the three readers can locate a first read, an uncertain write result and an integration example.

## Delegation and self-audit

Re-read original prompt 31 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Follow [AUDIT](AUDIT.md). Delegate a cold read to a package consumer and a console operator. Have a separate reviewer challenge the documented failure/recovery sequence on a scripted lost ACK. Repair documentation or forward and recheck any underlying behavioral defect.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `operator-docs` in [PROGRESS](PROGRESS.md) with the entry-point paths, commands and captured transcripts, generated reference result, and exact commit reviewed.

Finish with a concise report of the audit findings, fixes made, and final verification results.
