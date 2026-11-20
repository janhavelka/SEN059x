# 28 — Measure each available SKU on a read-only bench

## Start

Apply [COMMON](COMMON.md) and inspect [PROGRESS](PROGRESS.md). Identify one physically connected SKU, its supply and current endpoint before starting. Own read-only capture and measurement reports. One run covers one named SKU, fixture and build. Resume open rows on another setup without repeating completed observations.

## Inputs

Consume `runner-evidence`. Produce `bench-measurements`. Read the [electrical ratings](../reference/hardware.md), [register meanings](../reference/measurement-and-configuration.md) and [timing reference](../reference/protocol/Modbus_RTU_notes.md).

Verify the independent audit handoff `runner-audit` before using the covered work.

## Work

1. Record device marking, board/framework/commit, supply, pinout, common reference, topology, termination, transceiver/echo, target, clock and precision. SEN0591 allows 3.3–12 V, SEN0592 allows 3.3–24 V. Allow up to 1000 ms after power-on. Use a single master and a known address.
2. Read all four measurements and all eight configuration words by address. Capture raw request/reply/CRC and decoded units. Try a contiguous configuration-only `readregs` span and record success or exception. Compare the actual Arduino and native IDF apps and independent Python probe in separate runs where available.
3. Measure near/far targets within the rated range, geometry and no-return conditions. Compare temperature with a reference. Resolve subzero encoding only with a controlled cold observation. Record TX start, actual drain, DE/RX handover, first/last byte, frame end and owner deadline where observable. Preserve differing SEN0592 minimums, available SEN0591 range-level observations, and its 3 m rating versus the nominal 3.5 m setting.

## Acceptance

Attach raw captures, measured distances and uncertainty. Report distributions by register, SKU and existing setting. A USB timestamp can show latency without proving exact wire closure. Leave unobserved range levels for the isolated configuration campaign. Without hardware, record the missing setup and keep the corresponding rows open.

## Delegation and self-audit

Re-read original prompt 28 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Use [AUDIT](AUDIT.md). Assign a subagent to match captured words and timing to sourced limits. Have a reviewer check one claimed distance and one uncertain condition against the actual reference or trace. Correct the claim and recheck the observation.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `bench-measurements` or its partial/blocked status in [PROGRESS](PROGRESS.md), with fixture identity, rows exercised, trace locations and the setup needed to resume.

Finish with a concise report of the audit findings, fixes made, and final verification results.
