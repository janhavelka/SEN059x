# 25 — Install the exported package in independent consumers

## Start

Follow [COMMON](COMMON.md) and inspect the predecessor evidence in [PROGRESS](PROGRESS.md). Run `python scripts/check_package.py` on the checkout and record the current archive inventory. Own the export manifests, root component CMake, package checker and install fixtures. Finish when each actual archive can supply an independent application.

## Inputs

Consume `measurements`, `configuration` and `device-health`. Produce `package-exports`. Consult the [native package commands](VERIFICATION.md#native-esp-idf-esp32-s3) and [component layout](../IDF_PORT.md).

Verify the independent audit handoff `device-audit` before using the covered work.

## Work

1. Align PlatformIO and ESP-IDF allow-lists with every intended public header and compiled source. Preserve authors, maintainer, repository URL, MIT notice, version header and required package metadata. Exclude bench adapters, private headers and vendor originals.
2. Have the checker accept `--archive <path>` in addition to its existing PlatformIO pack operation. Produce a native archive with the pinned SDK's `compote component pack`. Inspect both archives, account for packer-owned metadata and compare shipped file contents with the source inventory.
3. In separate empty destinations install each archive from its own bytes. Compile every public header on a clean C++17 desktop. In an ESP-IDF 5.5.5 fixture using `REQUIRES sen059x`, include the umbrella and version, build and parse a known frame, and link a passive health call. No source-tree include path may conceal a missing export.

## Acceptance

Run both pack/install paths and builds from [verification](VERIFICATION.md). For each archive compile, **link and run** a desktop C++17 consumer from the installed files. It builds a request, parses a known frame and reads passive health. Separately link the ESP-IDF consumer. Record each archive digest and its actual consumers. Remove one required header from a temporary archive and demonstrate that the checker or independent build detects the omission.

## Delegation and self-audit

Re-read original prompt 25 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Apply [AUDIT](AUDIT.md). Delegate archive inventories to a source reviewer. Have an independent consumer reviewer reproduce an install using only the archive and instructions. Fix packaging leaks and rerun both consumers.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `package-exports` in [PROGRESS](PROGRESS.md) with exact pack/install commands, inventory comparison, digests and linked targets. Publication is a separate release operation.

Finish with a concise report of the audit findings, fixes made, and final verification results.
