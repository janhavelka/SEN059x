# 14 — Qualify the sensor and bus on a named bench

## Inputs

Apply [COMMON](COMMON.md). Consume `runner-evidence` and `verified-package`. Read the [hardware ratings](../reference/hardware.md), [measurement tables](../reference/measurement-and-configuration.md), [wire timing](../reference/protocol/Modbus_RTU_notes.md) and [coverage index](COVERAGE.md). Produce `bench-evidence` for the actual setup and model tested.

## Work

1. Identify each unit by marking or installation record. Record SKU, cable/pin mapping, reference ground, termination, topology, transceiver, direction mode and echo behavior, actual supply, adapter, board, firmware/SDK, console path, target geometry and clock resolution. SEN0591 permits 3.3–12 V and SEN0592 permits 3.3–24 V. Allow the published power-up delay of up to 1000 ms before the first command. Use one owner, a known unicast endpoint and one unit at address 1 when starting with factory settings.
2. On each available SKU read all four measurement registers individually at multiple measured target distances, including the near limit and a representative far target within its rating. Record raw requests/replies including CRC and the reference distance and uncertainty. Vary target/beam geometry for no-return observations. Record independent temperature alongside the positive example. Only interpret sign with controlled subzero evidence and a separate reference. Mark unexercised conditions explicitly.
3. Time TX start, end of shift-out, DE/RX handover, first/last response byte and observed frame boundary at the available timestamp precision. Record owner timeout separately. Compare measurement windows, all four SEN0591 per-level estimates when available, serial time at the selected rate and the SEN0592 language-edition differences. Evaluate the 3 m rating and the approximately 3.5 m SEN0591 level-4 label separately. Record actual samples and distribution before revising a limit.
4. Observe the eight configuration values where the unit supports an addressed read. Record rejected/unsupported, raw, applicable mode and observed value. On an explicitly authorized isolated bench, preserve the original settings, validate owner capabilities, preview the change, perform a single confirmed write, verify by independent same-register FC03 at the expected endpoint and restore/verify the original. Address and baud changes each need old/new endpoint evidence. Document ambiguous echo, lost ACK, readback mismatch or unsuccessful restoration. Never infer persistence from readback. Test persistence with a separate authorized power cycle and read where practicable.
5. Verify recovery after a missed reply, wrong/late reply or disconnect and reconnection. Keep deliberate malformed traffic in the offline suite unless the specific bench setup can inject it separately. With multiple uniquely addressed units, run bounded interleaved operations and verify no cross-assignment or starvation. When a four-head fixture is available, vary emitter placement/sequence and record cross-talk, intervals, target and geometry. Keep scheduling results separate from acoustic behavior.
6. Compare hardware behavior through the actual Arduino and IDF applications and the independent direct probe in separate runs. Record which combination of adapter, board, framework and SKU was actually connected. A USB polling timestamp may support end-to-end latency and still leave RTU wire closure unmeasured.

## Acceptance

For each SKU and examined feature, publish one of observed pass, observed fail or untested/blocked, with the test conditions, evidence path and unresolved question. Use a raw capture, logic trace or other independent reference for conclusions about ACK baud, timing, sign, sentinel or persistence. Record whether each commissioning operation restored the original endpoint. Missing hardware leaves specific rows open. Stage 15 may still review the tested software.

## Handoff

Record `bench-evidence` or the partial/blocked fixture status in [PROGRESS](PROGRESS.md). Provide a reproducible bench report by SKU plus a bounded list of open observations and any justified corrections to the audited reference.
