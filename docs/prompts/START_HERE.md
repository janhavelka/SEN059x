# Start or resume an implementation stage or audit checkpoint

Choose one file from the [index](README.md). There are 32 original stages and 11 additional audit checkpoints. An ID ending in `A` identifies a separate execution that audits the specified originals. **Start** names the prerequisite check and scope. **Inputs** names the handoffs to verify. The [ledger](PROGRESS.md) records evidence and the [sequence](SEQUENCE.json) gives the execution order and dependencies.

## First execution

```text
Work on main in the SEN059x repository. Execute docs/prompts/01-baseline-and-coverage.md only.
Read AGENTS.md and the stage in full. Follow its Start section and COMMON.md. Check the current
checkout and ledger. Delegate its source check or disjoint work to subagents. Have an independent
reviewer challenge the integrated result, fix findings and rerun the affected checks.
Record the named output, starting and tested revisions, commands, evidence and open items in PROGRESS.md.
Commit and sync main. Finish at this prompt's handoff.
```

## Run a single fresh audit

```text
Work on main. Execute docs/prompts/04A-audit-foundation-and-codec.md only.
Reread originals 01–04 in full. Derive their obligations, then inspect their actual commits,
diffs, current code, tests and any relevant local edits. Use parallel subagents for requirements
coverage, implementation correctness, edge cases, tests, scope and code simplicity.
Reproduce their findings, fix every confirmed in-scope gap with simple readable changes, and
repeat the original verification until the obligations are satisfied. Record actual blockers.
Finish with a concise report of findings, fixes and final verification on the tested revision.
Commit and sync the scoped handoff on main. Stop at this checkpoint.
```

For other checkpoints, substitute the checkpoint filename **and the original range named in it**. The audit starts a fresh review. A previous summary can locate evidence, but the code, diff and reruns determine the outcome. [Checkpoint procedure](AUDIT.md#fresh-range-audit)

## Resume or execute a later stage

### Scheduled Codex runs on the maintainer's Windows machine

The Windows task `AgentLoop-SEN059x` checks account-wide Codex usage hourly at minute 11, starting 21 November 2026 at 06:11 Prague time. The shared pacer can admit a full phase, a smaller slice or skip the hour. It prioritizes a completion after 24 hours, or from 18:00 if none has finished that local day. The weekly and five-hour usage limits take precedence. The task continues through the owner's absence until disabled. It can catch up after the machine is available and the owner is logged on.

Each admitted run starts a fresh session. A prompt goes through assessment, implementation, independent review and final verification across sessions. The unattended instruction calls for solo work. The review session rereads the originals and checks all six review areas against the actual code, diff and tests. Routine assessment and closeout use GPT-6.2 Sol at medium effort. Implementation and review use high effort, with GPT-6.2 Astra assigned to selected reviews of configuration writes, bus ownership, recovery and delivery. Manual dispatch uses the parallel review instructions elsewhere in this guide.

Read `%USERPROFILE%\agent-loop-logs\SEN059x\handoff.md` for the active ID, phase and next action. The [ledger](PROGRESS.md) holds the project evidence. The local wrapper commits checked paths, pushes to `main` and verifies the upstream revision. It credits one daily completion only when a prompt or checkpoint has a newly committed **Complete** row. The run log and transcripts are in the handoff directory. Use `Get-ScheduledTaskInfo -TaskName AgentLoop-SEN059x` to check the next trigger and `Disable-ScheduledTask -TaskName AgentLoop-SEN059x` to stop future runs.

### Manual execution

For a specific stage, replace the filename above with its exact path from the index. Use its named Inputs to find the predecessor entry in the ledger and rerun the smallest check that establishes the needed behavior. A status word or existing file alone cannot establish a dependency. If using a former filename, follow the [old-to-new topic map](README.md#finding-an-earlier-prompt).

```text
Resume the SEN059x promptset on main. Read the ledger and sequence. Run the earliest ready
unfinished execution, including a due audit checkpoint. Check its recorded starting and tested
revisions and any changes since its handoff. Resume a partial execution at its first missing
acceptance item. For a checkpoint, reread its originals in full and verify the integrated work.
Record the selected handoff, repairs, reruns and evidence, commit and sync, then stop.
```

## Execute a range or the remaining set

```text
Execute the remaining SEN059x implementation prompts on main. Read COMMON.md, PROGRESS.md
and SEQUENCE.json. Select ready originals and suffixed checkpoints in manifest order, verifying
their actual prerequisites against the checkout and recorded revisions.
Use only the relevant sections of the linked contracts. Have each execution own one named output,
delegate scoped tasks, audit the integrated change and record its evidence before moving on.
Run each due checkpoint as its own execution. Reread its exact original range, audit in parallel,
fix confirmed findings and repeat the required checks. Reopen affected evidence after later changes.
If a stage is blocked, record the exact missing capability or fixture and continue with independent
ready stages. Revisit partial rows when their prerequisites become available. Keep package, host,
firmware-build and physical evidence distinct. Commit and sync each completed prompt on main.
Finish with the delivery review and its 32A checkpoint, reporting real outcomes and open obligations.
```

For a bounded range, supply its first and last filenames, inclusive. Execute the interleaved checkpoints between them. To include the audit immediately after the last original, name the suffixed checkpoint as the end. A single-file execution stops at that file's handoff. Check prerequisites outside a requested range and complete independently ready work inside it. The [bench audit](30A-audit-bench-evidence.md) reports evidence available for originals 28–30, including missing fixtures. The [final checkpoint](32A-audit-delivery.md) rereads originals 31–32 and can close software with per-SKU physical rows still open.

## Audit the prompts themselves

```text
Review the SEN059x promptset as preparation work. Cross-check scope, dependency outputs,
actual ownership, versions, source limits and operation coverage. Trace the single-stage
and resume instructions. Have a reviewer check the revised contracts and run
python scripts/check_promptset.py plus python -m unittest discover -s test.
Keep all implementation progress tied to actual implementation evidence.
```
