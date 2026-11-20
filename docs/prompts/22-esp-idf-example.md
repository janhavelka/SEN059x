# 22 — Prove the full native ESP-IDF example and operator path

## Start

Apply [COMMON](COMMON.md) and reproduce the native build from [PROGRESS](PROGRESS.md). Trace ordinary monitor input from `app_main` to the shared dispatcher. Own the native example's composition tests, override build and operator README. Stop when an IDF operator can reach every documented operation and another developer can reproduce the results.

## Inputs

Consume `idf-port`. Produce `idf-example`. Use the [operation matrix](OPERATIONS.csv), [human command table](CLI_CONTRACT.md#human-console), [machine schema](CLI_CONTRACT.md#machine-channel-for-the-host-runner) and [IDF build commands](VERIFICATION.md#native-esp-idf-esp32-s3).

## Work

1. Run the actual app's platform-independent composition against scripted clock, UART and console hooks. Follow both renderers through twenty addressed operations per SKU, all eight isolated send-only forms, both address policies and every supported device baud. Include port failure, valid device exception and readback after a conditional switch/UART write.
2. Exercise `readregs 0x0100 3` and a five-byte exception to a two-word request. Check complete raw output or an explicit failure, bounded RX/deadline, no fallback requests and zero publication to typed caches.
3. Build from a checkout with a different directory name and verify discovery as `sen059x`. Build a legal pin override and check the resulting configuration. Document the correct console connector, boot logs, serial settings, pin selection, monitor command and walkthrough. Include each register/key, preview/confirmation, model-specific limits, extended-address restoration and independent verification of a vendor send.

## Acceptance

Build `examples/espidf_basic` for `esp32s3` using IDF 5.5.5. Inspect the ELF/link map for the application and exactly one root codec. Retain CLI and matrix results by SKU and fixture. Demonstrate an ordinary monitor instruction, e.g. `idf.py -C examples/espidf_basic -B build-cli -p <PORT> monitor`. Record the independent packaged consumer as a separate build path in the docs. Its archive qualification belongs to the packaging stages.

## Delegation and self-audit

Apply the [stage audit](AUDIT.md). Delegate the renamed-checkout test and have an independent reviewer run the switch/UART-unit cases with a rejecting fake device. Fix findings and rerun both the resolver and operator checks.

## Handoff

Record `idf-example` in [PROGRESS](PROGRESS.md) with build commands, SDK, ELF, alternate pin evidence, transcripts, matrix totals and physical limits.
