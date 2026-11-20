# 07 — Expose typed access to every configuration word

## Start

Apply [COMMON](COMMON.md), check `rtu-codec` in [PROGRESS](PROGRESS.md), and replay its FC06 address and baud vectors. Own the public setting selectors, descriptors, conversions, checked builders and raw readback decoders with native tests. Stop when each operation is callable without transport I/O.

## Inputs

Consume `rtu-codec` and produce `setting-access`. Use [both SKU tables](../reference/protocol/registers.csv), [configuration evidence](../reference/measurement-and-configuration.md), [six wire examples](../reference/protocol/modbus_vectors.csv) and [operation cases](OPERATIONS.csv).

Verify the independent audit handoff `codec-audit` before using the covered work.

## Work

1. Expose `Setting::{Address, Baud, SwitchPolarity, SwitchThreshold, Angle, UartUnit, Noise, Range}`. For each support typed FC03 request and readback decode and checked `buildWriteSingleRegister` on each SKU. Retain unknown readback words and state when interpretation is unavailable. Validate selector, access, model, units, policy and value before encoding. Keep descriptors usable from either example.
2. Implement all documented bounds/defaults and preserve unspecified defaults. Support eight baud codes and bps conversions, model-specific threshold/range limits, polarity below threshold, all noise-supply examples and model-specific beam axes. Label polarity/threshold as switch-mode and UART unit as automatic/controlled output. Physical mode activation and equality behavior are undocumented. Changing this unit cannot reinterpret a Modbus measurement.
3. Add checked `buildVendorBroadcastWrite` for all eight settings with explicit isolated profile and fixed destination 0xFF. Use the same model/value guards. Standard addresses cover 1..247 and explicitly isolated addresses 1..254. Neither profile assigns 0 or 255. The broadcast encoding has no addressed reply identity.

## Acceptance

Compile a public consumer for all sixteen configuration operations on each model. Check all eight settings through both write builders, exact request words/CRC, known/unknown readbacks, RO/undocumented rejection and min/max/outside cases. Include polarity 0/1/2, thresholds 29/30/3000/3001/5000/5001, UART mm/µs and unknown raw 2, eight baud codes, every angle/noise level and range level 5 on both SKUs. Verify 247→248 and 254→1 encodings under the applicable policies.

## Delegation and self-audit

Re-read original prompt 07 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Follow [AUDIT](AUDIT.md). Delegate an independent bilingual settings oracle. Have a separate reviewer attempt each conditional write, a reserved assigned address and a SEN0591 range level 5 through the public path. Fix and rerun the per-SKU boundary checks.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `setting-access` in [PROGRESS](PROGRESS.md) with all sixteen addressed operation cases per SKU, typed signatures, broadcast encoding, independent expected values and unresolved physical effects.

Finish with a concise report of the audit findings, fixes made, and final verification results.
