# Implementation progress

The audited scaffold exports `SEN059x::VERSION` and contains independent read-only probes. It has reference tests and Arduino and IDF package builds. All 32 original stages and 11 independent checkpoints below are planned. The promptset and its checks are preparation evidence. [Audited scaffold](https://github.com/janhavelka/SEN059x/commit/be99469b0d5b701c7649a47083c92942c2fcca3d) · [Stage index](README.md)

| Stage | Output | Status |
| ---: | --- | --- |
| 01 | `baseline` | Planned |
| 02 | `value-status` | Planned |
| 03 | `read-codec` | Planned |
| 04 | `rtu-codec` | Planned |
| 04A | `codec-audit` | Planned |
| 05 | `measurement-values` | Planned |
| 06 | `measurements` | Planned |
| 07 | `setting-access` | Planned |
| 08 | `configuration` | Planned |
| 09 | `device-health` | Planned |
| 09A | `device-audit` | Planned |
| 10 | `owner-exchange` | Planned |
| 11 | `owner-scheduler` | Planned |
| 12 | `owner-contract` | Planned |
| 12A | `owner-audit` | Planned |
| 13 | `consumer-main` | Planned |
| 14 | `consumer-fixture` | Planned |
| 14A | `consumer-audit` | Planned |
| 15 | `cli-console` | Planned |
| 16 | `cli-live` | Planned |
| 17 | `cli-commissioning` | Planned |
| 18 | `shared-cli` | Planned |
| 18A | `cli-audit` | Planned |
| 19 | `arduino-primary` | Planned |
| 20 | `arduino-example` | Planned |
| 20A | `arduino-audit` | Planned |
| 21 | `idf-port` | Planned |
| 22 | `idf-example` | Planned |
| 22A | `idf-audit` | Planned |
| 23 | `runner-client` | Planned |
| 24 | `runner-evidence` | Planned |
| 24A | `runner-audit` | Planned |
| 25 | `package-exports` | Planned |
| 26 | `version-provenance` | Planned |
| 27 | `verified-package` | Planned |
| 27A | `package-audit` | Planned |
| 28 | `bench-measurements` | Planned |
| 29 | `bench-commissioning` | Planned |
| 30 | `bench-shared-bus` | Planned |
| 30A | `bench-audit` | Planned |
| 31 | `operator-docs` | Planned |
| 32 | `delivery-review` | Planned |
| 32A | `delivery-audit` | Planned |

## Stage evidence

When starting or resuming, open the producing artifacts at their recorded revisions and rerun the smallest check that proves the prerequisite. For each original stage capture the initial base SHA and pre-existing edits, then the completed revision or relevant commits and paths. At a standalone checkpoint use that history to reconstruct the range and inspect it again on the current tree. If evidence is partial, identify precisely which required behavior can be exercised.

Use **Planned**, **In progress**, **Partial**, **Blocked** or **Complete** for each execution. Record grouped results by SKU and surface. Physical campaigns identify the fixture and any missing observations. The bench-evidence checkpoint can finish reviewing the available evidence while the affected physical rows stay open. When a later correction changes a previously audited claim, identify the previous checkpoint result, reopen its affected checks and record the new tested revision. [Fresh range audit](AUDIT.md#fresh-range-audit) · [Stage handoff](AUDIT.md#record-the-handoff)

Use this short entry for an original stage or checkpoint. Record commands and measured outcomes in the same evidence entry or linked immutable run. A checkpoint with no confirmed findings still records the original range and verification performed.

```text
Stage or checkpoint, output, status:
Original prompt(s) reread and predecessor artifacts checked:
Base SHA, covered commit(s), current/tested SHA and local changes examined:
Entry points, files and tests inspected:
Parallel assignments and lead-confirmed findings:
Scoped fixes or no confirmed findings:
Final verification commands, versions, results and evidence type:
Prior result superseded / affected handoffs rechecked:
Open rows by SKU / fixture / surface:
Next ready handoff or exact resume action:
```
