# 03 — Build and validate addressed FC03 frames

## Start

Apply [COMMON](COMMON.md), inspect `value-status` in [PROGRESS](PROGRESS.md), then compile its public-header smoke test. Own FC03/CRC declarations, portable implementation and native codec tests. Stop at request bytes and decoded raw response words.

## Inputs

Consume `value-status` and produce `read-codec`. Use [wire rules](../reference/protocol/Modbus_RTU_notes.md), [registers](../reference/protocol/registers.csv), [manufacturer frames](../reference/protocol/modbus_vectors.csv) and [public contracts](CONVENTIONS.md).

## Work

1. Implement `calcCrc16`, `isValidAddress`, `buildReadRegisters` and `validateReadResponseExpected`. Retain model, address policy, address, function, register and count in caller context. On valid construction return eight bytes, writing words high-byte-first and CRC low-byte-first. Reject invalid arguments before writing. Builders return zero on failure.
2. Support individual reads at all twelve addresses and explicit contiguous diagnostic reads. Check count 1..125, end-address overflow, every address in the span, TX and caller output capacity. The longest documented contiguous span is three words. Return ordered raw words. Preserve the unit's exception if it rejects a span. Published examples establish single-word operations.
3. Check CRC, expected address/function, exact count byte and exact length (`5 + 2 * count`). Accept an exception only as a five-byte addressed 0x83 with correct CRC and retain its full code. Define deterministic validation precedence and all nine `ReadResponseError` results. Keep promised error outputs initialized on every exit. Reject trailers and preserve value outputs on error. Use bounded alignment-independent memory operations.

## Acceptance

Run native codec and offline protocol checks. Match the four published FC03 pairs. Exercise `0x0100`/3 and `0x0200`/2, holes, count zero/126, wrap, foreign extended addresses, null/short buffers and canaries. Inject empty, short, long, padded exception, wrong address/function/count with recomputed CRC and swapped CRC. Check 0/1/247/248/254/255 under both policies and exclude broadcasts. Show identical single-word reply bytes for two different retained contexts.

## Delegation and self-audit

Follow [AUDIT](AUDIT.md). Delegate independent expected ADUs and CRCs. Have a separate reviewer append a byte after a valid frame and overrun a caller output buffer in a negative fixture. Fix and rerun the relevant parser cases and canaries.

## Handoff

Record `read-codec` in [PROGRESS](PROGRESS.md) with public signatures, precedence, buffer limits, exact vector results, block-read caveats and test commands.
