# Start or resume one implementation stage

Choose a file from the [index](README.md). Each file is a complete assignment for one execution. Its **Start** names the prerequisite check, paths and stop point. **Inputs** names the handoffs to verify. Keep the shared contracts open at the sections the file cites. The [ledger](PROGRESS.md) records actual work and the [sequence](SEQUENCE.json) gives dependency order.

## First execution

```text
Work on main in the SEN059x repository. Execute docs/prompts/01-baseline-and-coverage.md only.
Read AGENTS.md and the stage in full. Follow its Start section and COMMON.md. Check the current
checkout and ledger. Delegate its source check or disjoint work to subagents. Have an independent
reviewer challenge the integrated result, fix findings and rerun the affected checks.
Record the named output, tested revision, commands, evidence and open items in PROGRESS.md.
Commit and sync main. Finish at this prompt's handoff.
```

## Resume or execute a later stage

For a specific stage, replace the filename above with its exact path from the index. Use its named Inputs to find the predecessor entry in the ledger and rerun the smallest check that establishes the needed behavior. A status word or existing file alone cannot establish a dependency. If using a former filename, follow the [old-to-new topic map](README.md#finding-an-earlier-prompt).

```text
Resume the SEN059x promptset on main. Read the ledger and sequence. Run the earliest ready
unfinished stage. If it is Partial or In progress, verify the existing artifacts and resume
at its first missing acceptance item. Name the selected stage before editing. Use its
Start, Inputs, Work, Acceptance, independent audit and Handoff. Record and verify the complete
stage evidence, commit and sync, then stop at that handoff.
```

## Execute a range or the remaining set

```text
Execute the remaining SEN059x implementation prompts on main. Read COMMON.md, PROGRESS.md
and SEQUENCE.json. Select ready stages in numeric order, verifying their actual prerequisites.
Use only the relevant sections of the linked contracts. Have each execution own one named output,
delegate scoped tasks, audit the integrated change and record its evidence before moving on.
If a stage is blocked, record the exact missing capability or fixture and continue with independent
ready stages. Revisit partial rows when their prerequisites become available. Keep package, host,
firmware-build and physical evidence distinct. Commit and sync each completed prompt on main.
Finish with the delivery review, reporting real outcomes and any remaining obligations.
```

For a bounded range, specify its first and last filenames. Check every stage in the range against the dependency graph. If a required handoff outside the range is missing, identify the producing prompt and continue any independent eligible work inside the range. The final review can close software with per-SKU hardware rows still open.

## Audit the prompts themselves

```text
Review the SEN059x promptset as preparation work. Cross-check scope, dependency outputs,
actual ownership, versions, source limits and operation coverage. Trace the single-stage
and resume instructions. Have a reviewer check the revised contracts and run
python scripts/check_promptset.py plus python -m unittest discover -s test.
Keep all implementation progress tied to actual implementation evidence.
```
