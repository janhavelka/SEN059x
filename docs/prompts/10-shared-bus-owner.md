# 10 — Drive a bounded exchange through a scripted UART

## Start

Apply [COMMON](COMMON.md), open the preceding native results in [PROGRESS](PROGRESS.md), and run a single register decode through the public header. Own the application-side port/clock/owner seam under `examples/common/` and its `test/native/` fake. Stop with one bounded physical exchange and an attributable completion.

## Inputs

Consume `rtu-codec`, `measurements` and `device-health` and produce `owner-exchange`. Use [RTU timing and frame layout](../reference/protocol/Modbus_RTU_notes.md), [model timing](../reference/measurement-and-configuration.md) and the [ownership notes](../ARCHITECTURE.md).

Verify the independent audit handoff `device-audit` before using the covered work.

## Work

1. Inject UART, DE, clock and bounded caller/owner buffers. Return from each service step with progress or a next wake. Assert DE, write, wait for physical TX drain, switch to receive and collect one frame. Expose accepted TX length, drain success, accessible/total RX length, overflow, source/echo evidence and framing errors. Never release DE on a FIFO enqueue alone.
2. Copy operation identity, generation, explicit SKU, endpoint, register/count or write value and deadline. Keep TX storage alive until copied and RX through parsing. Call the real codec with accessible bytes only. Check eight-byte requests, seven-byte one-word replies, eight-byte ACKs, five-byte exceptions and eleven-byte replies for three contiguous diagnostic words. Block diagnostics return ordered raw data only.
3. Account for request/response serialization at 8N1, direction turnaround, register-specific sensor estimate, margin and RTU closure. Apply low-speed character timing and high-speed fixed recommendations at the documented threshold. A foreground read timestamp can support bounded assembly but cannot prove a precise wire event. Preserve evidence limits in the result.

## Acceptance

Drive the actual step entry with scripted bytes and a clock at 115200 and a lower supported rate. Exercise late polling and exact deadline, fragmented reply, short TX, missed drain, qualified reply, local echo, five-byte exception, trailing byte, concatenation, overrun and CRC-valid prefix. Allow up to the documented 750/140 ms measurement estimates plus overhead. Use an explicit diagnostic budget for a three-word read. Publish only a clean attributable result and feed the passive health observer once.

## Delegation and self-audit

Re-read original prompt 10 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Follow [AUDIT](AUDIT.md). Delegate an independent timeline with timestamps/expected directions. Have a separate reviewer provide a prefix plus a trailing byte and a buffered echo equal to an FC06 request. Repair false closure or acknowledgement and replay both timelines.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `owner-exchange` in [PROGRESS](PROGRESS.md) with owned paths, port lifetime and clock contracts, sample byte timeline, tested budgets and native entry command.

Finish with a concise report of the audit findings, fixes made, and final verification results.
