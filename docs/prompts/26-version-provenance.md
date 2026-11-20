# 26 — Verify library and application version provenance

## Start

Use [COMMON](COMMON.md) and [PROGRESS](PROGRESS.md). Run `python scripts/generate_version.py check` and inspect the exported `Version.h`. Own the generator, its regression tests, and the library identity supplied to the examples where needed. Stop when identity is correctly attributed in all fixture layouts.

## Inputs

Consume `package-exports` and `shared-cli`. Produce `version-provenance`. Read the [version identifiers and override semantics](VERSIONS.md#public-package-version-and-build-identity).

Verify the independent audit handoff `cli-audit` before using the covered work.

## Work

1. Preserve `library.json` as the SemVer source, all published identifiers, the prerelease text, numeric aliases and existing build overrides. Compare emitted header, both archives, component version and Doxygen project number. Keep `check` read-only and `sync` idempotent.
2. Attribute library and application revision and dirty state separately. Exercise a clean checkout, tracked local modification, detached archive, archive installed within an unrelated Git repository and failed Git status. Unavailable provenance reports `unknown`. A component must never borrow the parent application's revision.
3. Use both human `version`/`ver` and the correlated handshake to expose the actual framework, target, application build, library version and protocol schema. Use injected identities on a desktop so tests can differentiate two commits with the same SemVer.

## Acceptance

Run the version-generator tests and a release-mode consumer checking both string and numeric fields. Show that repeated checks leave fixture files byte-identical. Replay the two public version surfaces and compare their identities with fixture inputs. The SDK-specific version observations can be collected in the build matrix.

## Delegation and self-audit

Re-read original prompt 26 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Follow [AUDIT](AUDIT.md). Delegate expected identity cases to a reviewer familiar with the pinned peer. Have an independent reviewer install the archive inside an unrelated repository, force Git status failure and validate the result. Fix and rerun the affected cases.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `version-provenance` in [PROGRESS](PROGRESS.md) with the public identifiers, source and installed versions, observed clean/dirty/unknown cases and rerun command. Keep the current project version until a release is requested.

Finish with a concise report of the audit findings, fixes made, and final verification results.
