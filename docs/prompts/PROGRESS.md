# Implementation progress

The audited scaffold exports `SEN059x::VERSION` and contains independent read-only probes. It has reference tests and Arduino and IDF package builds. All implementation stages below are planned. The promptset and its checks are preparation evidence. [Audited scaffold](https://github.com/janhavelka/SEN059x/commit/be99469b0d5b701c7649a47083c92942c2fcca3d) · [Stage index](README.md)

| Stage | Output | Status |
| ---: | --- | --- |
| 01 | `baseline` | Planned |
| 02 | `value-status` | Planned |
| 03 | `read-codec` | Planned |
| 04 | `rtu-codec` | Planned |
| 05 | `measurement-values` | Planned |
| 06 | `measurements` | Planned |
| 07 | `setting-access` | Planned |
| 08 | `configuration` | Planned |
| 09 | `device-health` | Planned |
| 10 | `owner-exchange` | Planned |
| 11 | `owner-scheduler` | Planned |
| 12 | `owner-contract` | Planned |
| 13 | `consumer-main` | Planned |
| 14 | `consumer-fixture` | Planned |
| 15 | `cli-console` | Planned |
| 16 | `cli-live` | Planned |
| 17 | `cli-commissioning` | Planned |
| 18 | `shared-cli` | Planned |
| 19 | `arduino-primary` | Planned |
| 20 | `arduino-example` | Planned |
| 21 | `idf-port` | Planned |
| 22 | `idf-example` | Planned |
| 23 | `runner-client` | Planned |
| 24 | `runner-evidence` | Planned |
| 25 | `package-exports` | Planned |
| 26 | `version-provenance` | Planned |
| 27 | `verified-package` | Planned |
| 28 | `bench-measurements` | Planned |
| 29 | `bench-commissioning` | Planned |
| 30 | `bench-shared-bus` | Planned |
| 31 | `operator-docs` | Planned |
| 32 | `delivery-review` | Planned |

## Stage evidence

When starting or resuming, find the exact handoff names from the stage's Inputs in this ledger. Open their output paths at the recorded revisions and rerun the smallest relevant check. If a previous result is partial, identify precisely which prerequisite behavior exists. A later stage can consume evidence only for the behavior its own acceptance requires. Update the existing entry when resuming the same work.

Use **Planned**, **In progress**, **Partial**, **Blocked** or **Complete**. Record grouped results by SKU and surface for measurement/configuration, CLI, example and runner stages. Physical campaigns also identify the specific fixture. The final review carries open physical rows forward. [Stage audit and handoff](AUDIT.md#record-the-handoff)

Copy this template under the table the first time a stage runs.

```text
Stage and output:
Status and tested revision:
Public entry point / artifact paths:
Predecessor evidence checked:
Checks run, versions, outcome and evidence type:
Delegated work, independent challenge, findings, fixes and rerun:
Open rows by SKU / fixture / surface:
Next ready handoff or exact resume action:
```
