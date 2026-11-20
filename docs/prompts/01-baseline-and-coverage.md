# 01 — Establish the two-model baseline

## Inputs

Apply [COMMON](COMMON.md). Start from the audited [reference index](../reference/README.md), [register map](../reference/protocol/registers.csv), [wire examples](../reference/protocol/modbus_vectors.csv), [hardware guide](../reference/hardware.md), [coverage index](COVERAGE.md) and [peer source map](SOURCE_MAP.md). This stage produces `baseline`.

## Work

1. Record the current package version, public headers, example entry points, exported file list, CI jobs and SDK pins. Run the existing checks first. Retain the version-header consumer as a compatibility check when adding the public codec.
2. Review the twelve register rows and six worked transactions in both SKU editions. For each device capability record address, access, valid values or units, defaults when documented, measurement and side effects, and where it will be implemented, exposed and tested. Link existing tables for numeric data. Give conditional features their own row and disposition.
3. Assign both explicit models to test profiles. Record startup, SKU-rated supply and range, probe baud/address defaults, wire-time versus device latency, and real-time versus processed response timing. Record the open questions from the reference. Keep differing source values visible and explain which bound the owner will use.
4. Inspect SHZK’s actual build/parse calls and TMP1x2’s status/health observers, plus FieldCore main and rodtester. Identify per-device state and shared-bus state, and where their lifetimes differ. Check public type names against [CONVENTIONS](CONVENTIONS.md) before creating an API.

## Acceptance

Run `python scripts/fetch_originals.py --verify`, `python -m unittest discover -s test -v`, the probe self-test and version check. Confirm the lookup covers four measurements and eight configuration registers on each SKU. Every ambiguity has source links, a conservative software behavior and a physical or documentary fact that could settle it. A valid read at address 1 cannot select the model.

## Handoff

Record `baseline` in [PROGRESS](PROGRESS.md) with the source revision, check results and any corrections to the reference. Stage 02 consumes that baseline.
