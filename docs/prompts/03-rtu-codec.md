# 03 — Build and validate bounded Modbus RTU frames

## Inputs

Consume `baseline` and `value-status`. Apply [COMMON](COMMON.md). Read [wire layout and exception rules](../reference/protocol/Modbus_RTU_notes.md), [six manufacturer examples](../reference/protocol/modbus_vectors.csv), [register map](../reference/protocol/registers.csv) and [CONVENTIONS](CONVENTIONS.md). Produce `rtu-codec`.

## Work

1. Implement `calcCrc16`, `isValidAddress`, `buildReadRegisters`, `validateReadResponseExpected` and `parseWriteSingleRegister` through the public include. Provide the bounded FC06 encoder used by the two checked write builders in stage 05. Validate arguments before changing output, including destination, capacity and register/count arithmetic. Transmit register words high byte first and CRC low byte first. Return 8 for a built FC03/FC06 request and 0 on failure. Device-facing reads cover every published word individually, including the three mode-specific settings. The public diagnostic accepts the documented contiguous spans within the standard 1..125-word FC03 envelope. The manufacturer demonstrates single-word operations. [Operation matrix](OPERATIONS.csv)
2. For an expected FC03 reply accept only the addressed function, exact byte count `2 * count`, exact total `5 + 2 * count` and correct CRC. For FC06 accept only the addressed eight-byte echo of the exact register **and** value. For either function recognize only its five-byte addressed exception (0x83/0x86) with a valid CRC. Preserve all eight exception bits in `Status.detail`, including unknown codes. Never read CRC, function, count or data outside the supplied length.
3. Return the broad `Status` and all nine `ReadResponseError` distinctions from the shared contract. Define a deterministic precedence for multiply bad frames and test it. Independently test a CRC-correct frame for each wrong destination/function/count/length. Return a validated word exclusively through the retained request context. FC03 has no wire field that identifies the source register. Response timestamps, assembly, late-reply handling and local-echo exclusion stay with the owner.
4. Restrict implementation to pure bounded memory operations. Keep decoding independent of byte alignment and host endianness. Set every promised out-reason even on an early return. Reject truncated and overlong frames, including an extra byte after an otherwise valid reply.
5. Carry the address policy in the retained context from request construction through reply validation. The default `AddressPolicy::StandardUnicast` admits 1..247. The explicitly selected `AddressPolicy::DFRobotIsolated` admits 1..254, including matching responses from 248..254. Both reject 0 and 255 as assigned/current addresses. Only the separate `buildVendorBroadcastWrite` route in stage 05 can emit destination 0xFF with FC06. Keep FC03 and the addressed FC06 parser unavailable for broadcast. Standard Modbus address 0 is a separate convention with no SKU-specific evidence of support.

## Acceptance

- Expose a bounded, public, addressed FC03 diagnostic via `buildReadRegisters` and the expected-count parser. Accept a contiguous span of documented words, including `0x0100` count 3 and `0x0200` count 2. Require count in 1..125, checked end-address arithmetic, caller capacity and a known address for every word. The longest contiguous span in this map is three words. Reject gaps, wrap and broadcasts before TX. Decode the ordered raw words without updating individual measurement caches. Preserve a device exception if a unit declines the request. DFRobot defines the quantity and variable-size data fields and works single-word examples. Qualify multi-word behavior per SKU. [SEN0591 protocol format](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0592 protocol format](https://wiki.dfrobot.com/sen0592/docs/19640)

- Use the actual C++ API against the checked-in vectors. Verify all four read request/reply pairs and both FC06 pairs byte for byte, including the corrected address write at its original address. Test the internal FC06 encoder here and the public guarded write route again in 05. Keep the Python vectors as independent evidence.
- Cover null and short buffers with output canaries, empty input, one-byte-short/long replies, wrong address/function, wrong byte count, corrupt and byte-swapped CRC, unknown exception code, padded exception, and mismatched FC06 register/value with a valid CRC. Verify rejected data cannot replace a prior value. Test the supported FC03 count boundaries at the actual API layers.
- Use examples of two one-word requests to the same address with identical reply bytes. Show why only the retained, live owner context can select the engineering unit. Execute native tests and the offline protocol regression checks.
- Cover FC03 at every published address and FC06 encoding at each of the eight writable addresses on both profiles. Check current addresses 0, 1, 247, 248, 254 and 255 under both policies, including a correctly addressed extended-ID exception and a foreign extended-ID frame. An explicitly constructed vendor broadcast frame uses 0xFF, valid CRC and the chosen word. No received frame for that transmission enters an addressed reply parser.

## Delegation and self-audit

Apply the [stage audit](AUDIT.md). Give the reviewer the six manufacturer exchanges and independently computed bad frames. Test reversed CRC order, a trailing byte and a wrong FC06 value through the exported parser. Check output canaries on rejection.

## Handoff

Record `rtu-codec` in [PROGRESS](PROGRESS.md) with the exported signatures, builder bounds, validator precedence and positive/negative test results.
