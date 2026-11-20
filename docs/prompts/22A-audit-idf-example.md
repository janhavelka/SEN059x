# 22A — Audit the native ESP-IDF application and operator path

## Start

Re-read original prompts 21–22 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md) and use [PROGRESS](PROGRESS.md) to locate the original work. Find the commits for the native port and complete IDF example. Read their source changes and inspect `app_main`, component registration, UART ownership and the operator instructions from the current checkout.

## Inputs

Original prompts 21–22: [21](21-esp-idf-port.md), [22](22-esp-idf-example.md).

Consume `idf-port` and `idf-example`. Produce `idf-audit`.

## Work

Follow one documented command into the real dispatcher and native adapter. Inspect every native dependency and the ELF link map for exactly one codec. Check the IDF app builds from a renamed checkout, propagates pin overrides and handles failed SDK calls, a silent UART, deadline expiry and bounded raw block replies. Compare the CLI's observed versus verified fields with the shared contract.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Acceptance

Use ESP-IDF 5.5.5 to run `idf.py -C examples/espidf_basic -B build-cli set-target esp32s3 build` and the pin-override build in a separate directory. Replay the [operation cases](OPERATIONS.csv) through the injected IDF application for both SKUs. Record the app ELF, SDK and actual executed fixture. The standalone archive consumer has a separate verification path. [SDK commands](VERIFICATION.md#native-esp-idf-esp32-s3)

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Assign independent reviewers to the CMake/link path, UART fault handling and parity between operator instructions and the real dispatcher. The lead checks the SDK outputs and reproduces the strongest negative case before and after fixes. Use the [audit procedure](AUDIT.md) to record the challenge and reruns.

## Handoff

Finish with a concise report of the audit findings, fixes made, and final verification results. Record `idf-audit` in [PROGRESS](PROGRESS.md) with the application revision, SDK/target, link-map evidence, positive and failing cases repeated, findings, fixes and any physical limitations.
