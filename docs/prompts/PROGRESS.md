# Implementation progress

The starting point is the audited scaffold at [be99469](https://github.com/janhavelka/SEN059x/commit/be99469b0d5b701c7649a47083c92942c2fcca3d). It exports `SEN059x::VERSION` and has independent Python and Arduino probes. Its Arduino build, ESP-IDF package consumer and reference tests have run. There is no device-HIL result in that baseline.

| Stage | Output | Status |
| ---: | --- | --- |
| 01 | Device and feature baseline | Planned |
| 02 | Public value/status contract | Planned |
| 03 | FC03/FC06 codec | Planned |
| 04 | Typed measurements and freshness | Planned |
| 05 | Typed configuration and commissioning | Planned |
| 06 | Passive per-device state and health | Planned |
| 07 | Shared-bus owner contract | Planned |
| 08 | FieldCore-shaped consumer fixture | Planned |
| 09 | Shared human and machine CLI | Planned |
| 10 | Arduino ESP32-S3 CLI application | Planned |
| 11 | Native ESP-IDF CLI application | Planned |
| 12 | Host console runner and fault campaigns | Planned |
| 13 | Installable package, docs and build verification | Planned |
| 14 | Physical qualification, by SKU and fixture | Planned |
| 15 | Coverage and delivery handoff | Planned |

For each completed stage add its evidence commit or a link to the checked commit, files, exact checks and outstanding obligations. Use **Partial** for separately qualified features and **Blocked** for a specific unavailable prerequisite. A missing SEN0591 physical unit can leave its hardware rows open while SEN0592 measurements and independent software work proceed.
