# SEN059x implementation promptset

Use these 15 prompts to take the audited SEN0591/SEN0592 reference and version-only C++ package through a portable Modbus codec, shared-bus integration, two example consoles and a per-model qualification record. Start with [the copy-ready first prompt](START_HERE.md). The [progress ledger](PROGRESS.md) records which outputs actually exist.

| Stage | Work | Requires stages | Handoff |
| ---: | --- | --- | --- |
| 01 | [Establish the two-model baseline](01-baseline-and-coverage.md) | Initial scaffold | `baseline` |
| 02 | [Define public values and literal status names](02-value-status-contract.md) | 01 | `value-status` |
| 03 | [Build and validate FC03/FC06 frames](03-rtu-codec.md) | 02 | `rtu-codec` |
| 04 | [Decode measurements by SKU and track freshness](04-model-measurements.md) | 03 | `measurements` |
| 05 | [Validate settings and verify commissioning writes](05-configuration-writes.md) | 03, 04 | `configuration` |
| 06 | [Track passive per-device health and lifecycle](06-device-health.md) | 04, 05 | `device-health` |
| 07 | [Schedule bounded transactions on one RS485 bus](07-shared-bus-owner.md) | 03, 04, 05, 06 | `owner-contract` |
| 08 | [Prove the two FieldCore-node consumer contracts](08-fieldcore-consumer.md) | 07 | `consumer-fixture` |
| 09 | [Implement the shared human and machine console](09-shared-cli.md) | 06, 07, 08 | `shared-cli` |
| 10 | [Build the Arduino ESP32-S3 CLI example](10-arduino-example.md) | 09 | `arduino-example` |
| 11 | [Build the native ESP-IDF CLI example](11-esp-idf-example.md) | 09 | `idf-example` |
| 12 | [Exercise the console client and fault recovery](12-runner-and-faults.md) | 09, 10, 11 | `runner-evidence` |
| 13 | [Verify the exported package, examples and CI](13-package-and-ci.md) | 08, 10, 11, 12 | `verified-package` |
| 14 | [Qualify each model and bus on hardware](14-hardware-qualification.md) | 12, 13 | `bench-evidence` |
| 15 | [Audit coverage and hand off verified results](15-delivery-review.md) | 13 | `delivery-review` |

Stage 15 can close the software review with stage 14 pending. It must report every missing physical result by SKU and fixture. A subsequent hardware result reopens the affected delivery claims. Stages 10 and 11 can proceed independently after 09.

## Shared contracts

- [Execution and ownership](COMMON.md) governs every stage. [Peer source map](SOURCE_MAP.md) pins the library code, product callers and prompt archives used to choose the contracts.
- [Delegation and self-audit](AUDIT.md) describes how to assign a stage, review its integrated result, fix findings and record evidence. [Framework and version targets](VERSIONS.md) identifies the build environments and package version contract to verify.
- [Public API conventions](CONVENTIONS.md) fixes spelling, meaning, ownership and how results cross layers. [Console contract](CLI_CONTRACT.md) fixes human commands, asynchronous behavior and machine records.
- [Coverage index](COVERAGE.md) maps the twelve registers, timing constraints and cross-cutting behaviors to work and evidence. [Verification](VERIFICATION.md) separates host, package, framework and physical checks.
- [Operation cases](OPERATIONS.csv) specifies the twenty addressed operations by typed selector, human command and expected word. Both full example consoles and the public library must exercise every row on each SKU. The isolated vendor-addressing cases are additional checks.
- [Machine-readable sequence](SEQUENCE.json) declares the required handoffs and optional hardware evidence. Run `python scripts/check_promptset.py` from the repository root after editing the set. [Checker](../../scripts/check_promptset.py)

Device facts come from the [audited reference](../reference/README.md). When a peer implementation and a sensor manual answer different questions, use the peer for interface behavior and the manual for physical and protocol claims.
