# 01 — Establish the two-model baseline

## Start

Use [COMMON](COMMON.md) and check [PROGRESS](PROGRESS.md). Begin with `git status --short --branch` and `python scripts/check_promptset.py`. Own the baseline evidence entry and any verified corrections in `docs/reference/` or the coverage index. Stop when the starting behavior and implementation obligations can be reproduced.

## Inputs

Produce `baseline` from the existing scaffold, [reference index](../reference/README.md), [coverage](COVERAGE.md), [operation cases](OPERATIONS.csv), [peer revisions](SOURCE_MAP.md) and [toolchain targets](VERSIONS.md).

## Work

1. Inventory the package version, exported paths, header, independent probes, workflow jobs and SDK environments. Run the scaffold checks. Compare actual resolved versions to the SHZK and FieldCore build targets. Identify the library release, PlatformIO Core, platform package, Arduino core and native IDF individually.
2. Cross-check both SKUs' EN/CN tables against the twelve registers and six manufacturer exchanges. For each SKU account for twelve addressed FC03 reads and eight FC06 writes. Trace conditional settings, extended addressing, isolated vendor writes and raw contiguous reads to their planned public and example surfaces.
3. Record startup, electrical and rated range limits, factory endpoint, measurement trigger, sensor time and wire time. For conflicting minima, temperature encoding, missing echo, physical modes and persistence, retain the source, implementation rule and evidence needed.
4. Check SHZK build/parse calls, TMP health observers and the two FieldCore revisions against the planned names and ownership. Reuse existing tables by reference.

## Acceptance

Run `python scripts/fetch_originals.py --verify`, `python -m unittest discover -s test -v`, `python examples/python/sen059x_probe.py --self-test` and `python scripts/generate_version.py check`. Verify the operation count separately on each SKU and explain why a valid response at address 1 cannot identify its model.

## Delegation and self-audit

Follow [AUDIT](AUDIT.md). Delegate an independent register inventory and a peer-version comparison. Have a separate reviewer challenge one missing operation and one apparently compatible but incorrect version pin. Fix any discrepancies and rerun the affected source checks.

## Handoff

Record `baseline` in [PROGRESS](PROGRESS.md) with the source revision, actual commands, results, coverage links and open observations. Identify a working version-header consumer for the next execution.
