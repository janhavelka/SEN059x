# 27A — Audit exported packages, version provenance and CI

## Start

Re-read original prompts 25–27 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md) and use [PROGRESS](PROGRESS.md) to locate the original work. Obtain the implementation commits, resolved toolchain records, both archives and their CI artifacts. Compare actual archive contents, manifests, version generator, fixtures and workflow jobs against the relevant diffs and current files.

## Inputs

Original prompts 25–27: [25](25-package-exports.md), [26](26-version-provenance.md), [27](27-build-ci.md).

Consume `package-exports`, `version-provenance` and `verified-package`. Produce `package-audit`.

## Work

Trace one known frame and a passive health query through desktop C++17 consumers built from each archive. Check native consumers link those same exported bytes. Match package digests across jobs. Examine clean, dirty, missing and unrelated enclosing Git states and compare human and machine identities. Inspect the logs for both Arduino profiles, the native IDF app, FieldCore fixtures and the exact CI SHA.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Acceptance

Run `python scripts/generate_version.py check`, `python scripts/check_package.py` and the native pack/install commands in [verification](VERIFICATION.md#native-esp-idf-esp32-s3). Build in fresh fixture directories, link and run both desktop consumers, and verify the SDK links. Confirm the export checker detects a missing file in a temporary archive. Inspect the final commit's CI after any repair and repeat affected jobs. [Peer versions](VERSIONS.md)

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Split review between package/install integrity, version attribution and workflow/test coverage. Have a fresh reviewer trace a claimed green build back to its actual archive and commit. Correct coverage or code wherever an assertion can pass on source-tree files alone. Use the [audit procedure](AUDIT.md) to record the challenge and reruns.

## Handoff

Finish with a concise report of the audit findings, fixes made, and final verification results. Record `package-audit` in [PROGRESS](PROGRESS.md) with the two archive digests, consumer results, framework versions, exact commit and CI run, findings and corrected verification.
