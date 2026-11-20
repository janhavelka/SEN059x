# 02 — Establish the portable value and status API

## Start

Apply [COMMON](COMMON.md), find `baseline` in [PROGRESS](PROGRESS.md), and run `python scripts/generate_version.py check`. Inspect the installed version header and package fixture first. Own `include/SEN059x/`, required portable sources and export lists, and the native harness under `test/native/`. Stop with compiled types, context validation and diagnostic text.

## Inputs

Consume `baseline` and produce `value-status`. Read the literals in [CONVENTIONS](CONVENTIONS.md), the [typed selectors](OPERATIONS.csv), [version rules](VERSIONS.md) and [build commands](VERIFICATION.md).

## Work

1. Add the package umbrella `<SEN059x/SEN059x.h>` with self-contained C++17 headers. Export the files and linked definitions through the current package/component. Preserve `Version.h`. Document types, units, buffer lifetimes and return contracts.
2. Implement explicit `DeviceModel` selection, all `MeasurementKind` and `Setting` selectors and request context with model, destination, function, starting register and expected count or write word. Give fallible outputs a documented success-only assignment contract. Context checks support `AddressPolicy::StandardUnicast` (1..247) and explicit `DFRobotIsolated` (1..254). Reserve 255 for a separate vendor send operation.
3. Implement the entire `Err`, `Status`, `Ok()`, predicates, exception text and `ReadResponseError` vocabulary. Use stable message storage and preserve signed detail and raw exception codes. Keep scheduling and hardware types outside the public headers.

## Acceptance

Create the desktop CMake/CTest harness and run the native build from [VERIFICATION](VERIFICATION.md). Compile each header first in a translation unit and link a consumer including umbrella and version. Exercise both models, missing/invalid model, 0/247/248/254/255 context boundaries, every status string and fallback, both predicates, explicit bool, −7 and 0xE1 details. Run package/version checks and link the affected installed package consumer.

## Delegation and self-audit

Re-read original prompt 02 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Follow [AUDIT](AUDIT.md). Delegate peer spelling/field checks. Have a separate reviewer try an unselected model and a default-policy binding at 248 and inspect an archive consumer. Fix the public entry point and rerun its failing case and package checks.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `value-status` in [PROGRESS](PROGRESS.md) with exported declarations, one compiled caller, context boundaries, package inventory and runnable native and installed-fixture checks.

Finish with a concise report of the audit findings, fixes made, and final verification results.
