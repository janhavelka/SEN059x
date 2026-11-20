# SEN059x implementation promptset

Each of the 32 prompts delivers one reviewable result. Run one file for a single session. Its **Start** section names the first check, what to edit and where to stop. The prerequisites below are named outputs that can be verified in the checkout. [Copy-ready dispatch and resume instructions](START_HERE.md) · [Progress and evidence](PROGRESS.md)

| Stage | Prompt | Requires | Handoff | Optional evidence |
| ---: | --- | --- | --- | --- |
| 01 | [Establish the two-model baseline](01-baseline-and-coverage.md) | Initial scaffold | `baseline` | — |
| 02 | [Establish the portable value and status API](02-value-status-contract.md) | 01 | `value-status` | — |
| 03 | [Build and validate addressed FC03 frames](03-read-rtu-codec.md) | 02 | `read-codec` | — |
| 04 | [Encode FC06 requests and validate addressed replies](04-write-rtu-codec.md) | 03 | `rtu-codec` | — |
| 05 | [Decode four measurements and model timing](05-model-measurements.md) | 03 | `measurement-values` | — |
| 06 | [Keep raw and usable samples fresh per binding](06-sample-freshness.md) | 05 | `measurements` | — |
| 07 | [Expose typed access to every configuration word](07-configuration-registers.md) | 04 | `setting-access` | — |
| 08 | [Model commissioning readback and uncertain effects](08-write-verification.md) | 07 | `configuration` | — |
| 09 | [Track passive device health and operation statistics](09-device-health.md) | 06, 08 | `device-health` | — |
| 10 | [Drive a bounded exchange through a scripted UART](10-shared-bus-owner.md) | 04, 06, 09 | `owner-exchange` | — |
| 11 | [Schedule shared-bus work and settle late traffic](11-bus-settlement.md) | 10 | `owner-scheduler` | — |
| 12 | [Execute isolated commissioning through the bus owner](12-commissioning-owner.md) | 08, 11 | `owner-contract` | — |
| 13 | [Exercise the FieldCore main consumer lifecycle](13-fieldcore-consumer.md) | 11 | `consumer-main` | — |
| 14 | [Compare the two FieldCore owner contracts](14-fieldcore-revisions.md) | 12, 13 | `consumer-fixture` | — |
| 15 | [Build the human console and passive commands](15-cli-console.md) | 09, 11 | `cli-console` | — |
| 16 | [Run live reads, diagnostics and jobs from the terminal](16-cli-live-commands.md) | 15 | `cli-live` | — |
| 17 | [Expose all commissioning operations in the human console](17-cli-commissioning.md) | 12, 16 | `cli-commissioning` | — |
| 18 | [Add machine sessions and prove dispatcher parity](18-cli-machine-protocol.md) | 17 | `shared-cli` | — |
| 19 | [Bring up the Arduino console on the primary ESP32-S3 profile](19-arduino-port.md) | 18 | `arduino-primary` | — |
| 20 | [Verify the Arduino example under both peer toolchains](20-arduino-peer-parity.md) | 19 | `arduino-example` | — |
| 21 | [Build the native ESP-IDF console and RS485 port](21-esp-idf-port.md) | 18 | `idf-port` | — |
| 22 | [Prove the full native ESP-IDF example and operator path](22-esp-idf-example.md) | 21 | `idf-example` | — |
| 23 | [Implement the Python client for the example console](23-runner-client.md) | 18 | `runner-client` | — |
| 24 | [Exercise both example applications across three profiles](24-runner-faults.md) | 20, 22, 23 | `runner-evidence` | — |
| 25 | [Install the exported package in independent consumers](25-package-exports.md) | 06, 08, 09 | `package-exports` | — |
| 26 | [Verify library and application version provenance](26-version-provenance.md) | 18, 25 | `version-provenance` | — |
| 27 | [Run the release build matrix on the exact commit](27-build-ci.md) | 14, 24, 26 | `verified-package` | — |
| 28 | [Measure each available SKU on a read-only bench](28-bench-measurements.md) | 24 | `bench-measurements` | — |
| 29 | [Verify settings and restoration on an isolated unit](29-bench-commissioning.md) | 24 | `bench-commissioning` | — |
| 30 | [Observe multidrop recovery and acoustic interference](30-bench-shared-bus.md) | 11, 24 | `bench-shared-bus` | — |
| 31 | [Publish the tested library, console and integration walkthroughs](31-operator-documentation.md) | 14, 24, 26 | `operator-docs` | — |
| 32 | [Audit the software delivery and record physical evidence](32-delivery-review.md) | 27, 31 | `delivery-review` | 28, 29, 30 |

The Arduino port, native IDF port and Python client can start independently once the shared machine console works. The release build matrix and operator walkthroughs can also proceed independently. The three bench campaigns each take the verified runner as input. Software delivery records any missing physical results per SKU and fixture.

## Finding an earlier prompt

The previous revision used 15 numbers. Match earlier editor tabs and notes by subject. A completed result from an older checkout needs its actual artifacts and tests checked against each new stage before its ledger status changes. [Previous index](https://github.com/janhavelka/SEN059x/blob/db42fe727661d314a19dc4af4351a93454b993d2/docs/prompts/README.md)

| Former topic | Current stage(s) |
| --- | --- |
| Baseline (01), public values (02) | 01, 02 |
| RTU codec (03), measurements (04) | 03–04 and 05–06 respectively |
| Configuration (05), passive health (06) | 07–08 and 09 |
| Shared-bus owner (07), FieldCore fixture (08) | 10–12 and 13–14 |
| Shared CLI (09) | 15–18 |
| Arduino example (10), native IDF example (11) | 19–20 and 21–22 |
| Host runner (12) | 23–24 |
| Packaging, CI and documentation (13) | 25–27 and 31 |
| Hardware (14), final review (15) | 28–30 and 32 |

## Shared contracts

- Use [COMMON](COMMON.md) for ownership and [AUDIT](AUDIT.md) for delegation and rerunning fixes. The [source map](SOURCE_MAP.md) records the pinned implementation and consumer precedents.
- Use [public conventions](CONVENTIONS.md) for literal names, [CLI contract](CLI_CONTRACT.md) for commands and sessions, and [version targets](VERSIONS.md) for toolchain and package identity.
- Consult [coverage](COVERAGE.md) and the [operation cases](OPERATIONS.csv) for the four measurement reads, eight setting reads and eight setting writes per SKU. Follow [verification](VERIFICATION.md) for the actual host, archive and firmware commands.
- The [machine-readable sequence](SEQUENCE.json) records direct dependencies and optional hardware evidence. Run `python scripts/check_promptset.py` after changing a stage, handoff, link or operation.
