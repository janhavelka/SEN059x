# 04 — Encode FC06 requests and validate addressed replies

## Start

Apply [COMMON](COMMON.md), open `read-codec` in [PROGRESS](PROGRESS.md) and rerun its CRC/vector tests. Own portable FC06 encoding, addressed parsing and native wire tests alongside the FC03 codec. Stop at verified frame shape and address policy. The typed settings layer will supply model/value validation.

## Inputs

Consume `read-codec` and produce `rtu-codec`. Use the [published FC06 pairs](../reference/protocol/modbus_vectors.csv), [register access table](../reference/protocol/registers.csv) and [public names](CONVENTIONS.md).

## Work

1. Provide a bounded encoding primitive for the checked setting builders and `parseWriteSingleRegister` through the public umbrella. Enforce caller capacity, destination policy and writable register access. Preserve big-endian register/value, low-byte-first CRC and eight-byte success/zero failure. Keep the byte-level primitive from bypassing the future public model/value guards.
2. For an addressed FC06 response validate exact length, CRC, expected address, function, register **and** value. Recognize a five-byte 0x86 exception independently, validate address and CRC, and preserve its raw exception detail. Initialize failure reasons and never replace a prior good output on rejection.
3. Provide separate addressed and fixed-destination 0xFF encoding routes for later typed builders. Carry policy through reply checks for extended unicast. Never pass a 0xFF send to an addressed reply parser. Document what eight matching bytes prove about shape when adapter echo and reply are identical.

## Acceptance

Run native checks, Python vectors and the FC03 regressions. Reproduce both published FC06 examples, including the corrected old-address reply. Test all eight writable and four read-only register addresses, unknown register, null/short buffers, short/overlong replies, wrong register/value with valid CRC, foreign address, malformed/padded exception, and unknown exception code. Cover both address policies and produce a CRC-correct isolated 0xFF frame without asserting transmission or effect.

## Delegation and self-audit

Follow [AUDIT](AUDIT.md). Delegate independent wire-vector preparation. Have a separate reviewer substitute a wrong register and then wrong value while repairing each CRC, and try feeding TX echo as evidence of verification. Fix any acceptance and rerun all write parser cases.

## Handoff

Record `rtu-codec` in [PROGRESS](PROGRESS.md) with encoder visibility, addressed parser signature, exception handling, exported files and results for both published FC06 pairs.
