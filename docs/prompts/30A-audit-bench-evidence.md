# 30A — Audit available per-SKU hardware evidence

## Start

Re-read original prompts 28–30 in full and perform a fresh, independent audit of the completed work. Apply [COMMON](COMMON.md) and use [PROGRESS](PROGRESS.md) to locate the original work. Start from the tested runner revision. Locate original bench report commits and the raw captures for each setup actually exercised. Read the full scope even where a unit, fixture or authorization is missing.

## Inputs

Original prompts 28–30: [28](28-bench-measurements.md), [29](29-bench-commissioning.md), [30](30-bench-shared-bus.md).

Consume `runner-audit`. Review `bench-measurements`, `bench-commissioning` and `bench-shared-bus` when available. Produce `bench-audit`.

## Work

Trace each reported claim back to its identifying SKU, supply, binding, board, firmware, timings and reference. For writes, inspect original word, authorized preview/confirmation, FC06, same-register addressed FC03, restoration and any separate power-cycle evidence. For fairness and cross-device cache or health isolation, require evidence from two simultaneously retained bindings. Sequential address selection can establish individual transfers or acoustic observations when the fixture and firing sequence support those claims. Distinguish raw USB timestamps, on-wire timing and acoustic measurements.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompts.

## Acceptance

Rerun the documented read-only replay against available captures and `python examples/python/sen059x_probe.py --self-test` for the reference bytes. Reproduce physical observations only with the identified available fixture and existing authorization. Preserve missing, partial and failed hardware rows per SKU and operation. If no bench evidence exists, report that limit and leave the physical audit open. An offline replay establishes no hardware pass. [Hardware ratings](../reference/hardware.md) · [Operation cases](OPERATIONS.csv)

## Delegation and self-audit

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Assign separate reviewers to measurement/timing evidence, commissioning/restoration, and multidrop attribution. The lead independently reads the raw traces, revises unsupported claims and reruns only the relevant authorized checks. An uncertain endpoint or inaccessible switch output stays explicitly unresolved. Use the [audit procedure](AUDIT.md) to record the challenge and reruns.

## Handoff

Finish with a concise report of the audit findings, fixes made, and final verification results. Record `bench-audit` in [PROGRESS](PROGRESS.md) with the available commits and traces, reproduced checks, corrected claims, authorized observations, and partial/blocked rows with the fixture or permission needed.
