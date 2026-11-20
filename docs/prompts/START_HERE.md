# Start or resume the SEN059x implementation

Use this set to build a portable C++17 library for **DFRobot SEN0591 and SEN0592**. The checkout already contains the device references, independent read-only probes and a version-only package. The [sequence](README.md) defines the work still to do.

## Execute one prompt

```text
Work in the SEN059x repository on main. Execute docs/prompts/01-baseline-and-coverage.md only.
Read root AGENTS.md, docs/prompts/COMMON.md, AUDIT.md, VERSIONS.md and the stage.
Check its prerequisites in the working tree. Use the sensor references and pinned sibling
implementations as evidence. Spawn subagents for source checks or disjoint implementation,
then have an independent reviewer audit the integrated result. Perform the stage self-audit,
fix confirmed findings, rerun the checks and record evidence and open rows in PROGRESS.md.
Commit and sync main. Stop after the stage handoff.
```

For the next prompt replace the filename with the exact file in the [index](README.md). To dispatch a range, name its first and last stage and verify the dependencies of the first.

## Execute the set

```text
Execute the SEN059x promptset in docs/prompts/ on main. Read COMMON.md, CONVENTIONS.md,
CLI_CONTRACT.md, SOURCE_MAP.md, VERSIONS.md, OPERATIONS.csv, AUDIT.md and SEQUENCE.json.
Verify what is already implemented,
then resume from PROGRESS.md. Work through ready stages in numeric order. Preserve the
application-owned RS485 bus. Use the same portable core, shared CLI and owner operations
in the Arduino and native ESP-IDF examples. Test against both actual FieldCore consumer
contracts using a fixture in this repository. Sibling repositories are read-only references.
Delegate scoped work and independent review to subagents. For each stage perform its
self-audit, fix confirmed findings and rerun checks. Prove all 20 addressed operations on
each model through the public API and both full example consoles as those stages close.
Test the isolated vendor operations separately. Verify the two named Arduino profiles and
native IDF pin. After each stage record tests, findings and remaining evidence, commit and sync.
If a physical fixture is unavailable, complete the independent software and package work
and report exactly which SKU and behavior still need observation.
```

## Review the promptset itself

```text
Audit docs/prompts/ against the current sibling headers, example call sites, both FieldCore
owner revisions and DFRobot originals. Correct naming, ownership, stage dependencies and
verification gaps. Spawn a reviewer to test the revised contracts against real sources.
Reconcile all operation rows, framework pins and self-audit requirements. Run
python scripts/check_promptset.py. This is preparation work only.
```
