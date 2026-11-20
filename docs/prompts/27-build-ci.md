# 27 — Run the release build matrix on the exact commit

## Start

Apply [COMMON](COMMON.md) and check [PROGRESS](PROGRESS.md) for independently working consumer, runner and package commands. Read the existing CI workflow first. Own CI jobs, their pinned environments and artifact transfers. Stop at a verified, reproducible software build.

## Inputs

Consume `consumer-fixture`, `runner-evidence` and `version-provenance`. Produce `verified-package`. Use the exact lanes in [VERSIONS](VERSIONS.md) and [VERIFICATION](VERIFICATION.md).

Verify the independent audit handoffs `consumer-audit` and `runner-audit` before using the covered work.

## Work

1. Run Python discovery, promptset and reference verification, probe self-test, version checks, C++17 tests, both FieldCore modes, shared CLI replay and Doxygen. Verify the actual configuration under test uses release-active assertions.
2. Build the existing root/nested read-only Arduino probe and the full ESP32-S3 console in the primary Core 6.2.0 / Arduino 3.3.12 environment. In an isolated Core 6.1.19 / Arduino 3.3.11 lane build the same console for FieldCore. Record resolved packages, board and compiler settings for each.
3. Build the native IDF 5.5.5 console and both independent ESP32-S3 consumers installed from the exact PlatformIO and native component archives. Carry artifact hashes across jobs. Keep jobs individually named so a green app build cannot conceal a missing package consumer.

## Acceptance

Inspect the workflow run on the final pushed SHA, its native/Arduino/IDF logs, artifact identities and generated public documentation. Reproduce and correct failures, then verify the corrected commit. Distinguish a compiled firmware image, an executed host fixture and a physical sample in the evidence.

## Delegation and self-audit

Re-read original prompt 27 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Use [AUDIT](AUDIT.md). Delegate a source-to-workflow lane inventory. Have an independent reviewer pick an archive by digest, trace it to the consuming job and check that the two Arduino lanes used different pinned cores. Fix drift and rerun.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `verified-package` in [PROGRESS](PROGRESS.md) with tested SHA, job links, actual compiler/Core/SDK versions, both inventories and all distinct consumer results.

Finish with a concise report of the audit findings, fixes made, and final verification results.
