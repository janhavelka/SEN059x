# Start or resume the SEN059x implementation

Use this set to build a portable C++17 library for **DFRobot SEN0591 and SEN0592**. The checkout already contains the device references, independent read-only probes and a version-only package. The [sequence](README.md) defines the work still to do.

## Execute one prompt

```text
Work in the SEN059x repository on main. Execute docs/prompts/01-baseline-and-coverage.md only.
Read root AGENTS.md, docs/prompts/COMMON.md, the stage and its linked contracts.
Check its prerequisites in the working tree. Use the sensor references and pinned sibling
implementations as evidence. Implement its scoped outputs with tests, update PROGRESS.md,
record remaining evidence, commit and sync main. Stop after the stage handoff.
```

For the next prompt replace the filename with the exact file in the [index](README.md). To dispatch a range, name its first and last stage and verify the dependencies of the first.

## Execute the set

```text
Execute the SEN059x promptset in docs/prompts/ on main. Read COMMON.md, CONVENTIONS.md,
CLI_CONTRACT.md, SOURCE_MAP.md and SEQUENCE.json. Verify what is already implemented,
then resume from PROGRESS.md. Work through ready stages in numeric order. Preserve the
application-owned RS485 bus. Use the same portable core, shared CLI and owner operations
in the Arduino and native ESP-IDF examples. Test against both actual FieldCore consumer
contracts using a fixture in this repository. Sibling repositories are read-only references.
After each stage, record the commands, evidence and open obligations, commit and sync.
If a physical fixture is unavailable, complete the independent software and package work
and report exactly which SKU and behavior still need observation.
```

## Review the promptset itself

```text
Audit docs/prompts/ against the current sibling headers, example call sites, both FieldCore
owner revisions and DFRobot originals. Correct naming, ownership, stage dependencies and
verification gaps. Run python scripts/check_promptset.py. This is preparation work only.
```
