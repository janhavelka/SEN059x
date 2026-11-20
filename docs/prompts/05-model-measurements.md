# 05 — Decode four measurements and model timing

## Start

Apply [COMMON](COMMON.md), locate `read-codec` in [PROGRESS](PROGRESS.md), and replay the published processed-distance pair through its expected-response entry point. Own portable measurement declarations, decode/timing tables and their native tests. Stop with a typed result for one attributable live request.

## Inputs

Consume `read-codec` and produce `measurement-values`. Use [measurement tables](../reference/measurement-and-configuration.md), [register map](../reference/protocol/registers.csv), [electrical and distance ratings](../reference/hardware.md) and [operation cases](OPERATIONS.csv).

## Work

1. Implement the four public `MeasurementKind` requests and decode their exact FC03 responses with retained context on each model. Return model, destination, kind/register, raw word, units, quality/interpretation availability and owner-supplied completion timestamp with separate validity. Distance and echo reads trigger independent acquisitions. Temperature sampling instant is unspecified.
2. For distance, apply the inclusive 20..3000 mm or 30..5000 mm rating by SKU. Retain out-of-profile words including zero and 0xFFFF. Missing-echo encoding is unknown. Use tenths of °C for the documented positive temperature example and report 0.5 °C resolution. Preserve unexpected quantization and ambiguous high words without assigning an undocumented sign. Keep echo time in µs and expose the explicitly approximate division by 5.75 using fractional arithmetic.
3. Make timing available by model/register. Preserve both SEN0592 EN/CN minima for processed distance (190/300 ms) and temperature/echo (5/40 ms). Retain the broader 750 ms processed, 140 ms real-time/temperature/echo upper estimates, the real-time 15 ms minimum, SEN0591's published minima and its four narrower per-level timing pairs. Frame time, turnaround and margin belong to the owner.

## Acceptance

Run native public-API tests for all eight model/kind combinations. Assert 754 mm, 751 mm and 30.0 °C against the published pairs. Label the echo sample synthetic. Test inclusive distance bounds, adjacent values, a high temperature word, known zero time, unknown time and exact units. Compare every timing entry to its source and keep FC03 units independent of UART output mode.

## Delegation and self-audit

Follow [AUDIT](AUDIT.md). Delegate the bilingual timing and conversion oracle. Have an independent reviewer decode the same raw word through two different request contexts and challenge an out-of-range value. Fix any bad units, timing or quality and rerun the matrix.

## Handoff

Record `measurement-values` in [PROGRESS](PROGRESS.md) with typed entry points, timing lookup, fixture sources, raw/quality results by SKU and unresolved physical encodings.
