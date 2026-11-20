# Execution contract

Read the [repository instructions](../../AGENTS.md), [sequence](SEQUENCE.json), [peer source map](SOURCE_MAP.md), [public conventions](CONVENTIONS.md) and [sensor reference](../reference/README.md) before changing code. Use the [CLI contract](CLI_CONTRACT.md) for any example, console or runner change. The register and timing evidence is already in this repository. Reopen it when behavior depends on a value.

## Entry

1. Check the branch, upstream, current diffs and the last recorded handoff. Verify predecessor **artifacts and tests**, including changes since their recorded commits. The [progress ledger](PROGRESS.md) starts with every implementation stage planned.
2. Name the stage, the outputs it consumes, the paths it owns, and the positive and failure cases that will show it works. The [coverage index](COVERAGE.md) routes each requirement to one or more stages.
3. Inspect the referenced sibling declarations **and a caller** at the pinned revision. Use an exact Git object if the local branch differs. Record any newer revision deliberately adopted. Preserve behavior or document why the transport, device evidence or consumer requires an adaptation. Use the published SKU manuals for device semantics.
4. Work in this repo. With multiple agents, give them disjoint paths or a read-only review. The lead reconciles signatures, examples and test expectations before committing. Follow the user's selected branch and sync policy for the execution session.

## Ownership across stages

| Layer | Responsibility |
| --- | --- |
| Portable library | Model profiles, exact request bytes and response validation, typed values, passive per-device observations. Build on a desktop with standard C++17 and caller-owned storage. |
| Bus owner | UART, direction pins, monotonic clock, request lifetime, one outstanding exchange, deadline, frame boundary, scheduling and retry policy for the entire shared bus. Applications feed complete results to the core. |
| Example application | Compose an owner with one or more explicit device bindings. The human CLI, machine client and periodic example use those public calls. Reporting a cached value requires zero bus traffic. |
| Bigger firmware | Owns profile selection, retries, measurement schedule, product-level health/freshness, persistence, UI, telemetry and recovery policy. The consumer proof lives in SEN059x. |

Keep public headers self-contained. Document units, evidence limits, preconditions, side effects, buffer capacities, time origin and lifetime on the declarations. Keep the core free of Arduino, ESP-IDF, FreeRTOS, pySerial, board pins, logging, polling loops and framework clocks. Carry raw words and Modbus exception details through the API. A valid frame proves a communication event. The publication of a particular kind of measurement also requires the correct request context and usable data.

## Verification and handoff

- Implement one usable slice through its normal entry point. Give each new codec, public result, owner transition or CLI action a positive case and a failure case that leaves state consistent. Preserve existing independent examples and manufacturer golden frames.
- Use the checks in [verification](VERIFICATION.md) that exercise the changed surfaces. Keep a record of the actual target, SDK, commit and outcome. Distinguish simulated traffic, compile/link checks and physical observations.
- Update [PROGRESS.md](PROGRESS.md) with stage, status, output paths, tested commands, evidence commit and the next ready work. Record the sensibly short handoff in the stage ledger or a linked report when substantial. Record any partial or blocked check next to the feature that needs it.
- Commit and sync completed changes on `main` during this session. Preserve local user edits. Use the existing CI policy for the size of the change. Tagging, registry publication and changes to FieldCore-node are separate delivery decisions.
