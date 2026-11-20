# 03 — Build and validate bounded Modbus RTU frames

## Inputs

Consume `baseline` and `value-status`. Apply [COMMON](COMMON.md). Read [wire layout and exception rules](../reference/protocol/Modbus_RTU_notes.md), [six manufacturer examples](../reference/protocol/modbus_vectors.csv), [register map](../reference/protocol/registers.csv) and [CONVENTIONS](CONVENTIONS.md). Produce `rtu-codec`.

## Work

1. Implement `calcCrc16`, `isValidAddress`, `buildReadRegisters`, `validateReadResponseExpected` and `parseWriteSingleRegister` through the public include. Provide the bounded FC06 encoding primitive for the checked builder completed in phase 05. Validate every argument before changing the output, including destination, capacity and register/count arithmetic. Transmit register words high byte first, CRC low byte first. Return 8 on successful FC03/FC06 request construction, 0 on failure. Require target address 1..247. Device-facing reads support one published word per request. If a general FC03 shape helper accepts counts 1..125, document it as wire validation independent of device support.
2. For an expected FC03 reply accept only the addressed function, exact byte count `2 * count`, exact total `5 + 2 * count` and correct CRC. For FC06 accept only the addressed eight-byte echo of the exact register **and** value. For either function recognize only its five-byte addressed exception (0x83/0x86) with a valid CRC. Preserve all eight exception bits in `Status.detail`, including unknown codes. Never read CRC, function, count or data outside the supplied length.
3. Return the broad `Status` and all nine `ReadResponseError` distinctions from the shared contract. Define a deterministic precedence for multiply bad frames and test it. Independently test a CRC-correct frame for each wrong destination/function/count/length. Return a validated word exclusively through the retained request context. FC03 has no wire field that identifies the source register. Response timestamps, assembly, late-reply handling and local-echo exclusion stay with the owner.
4. Restrict implementation to pure bounded memory operations. Keep decoding independent of byte alignment and host endianness. Set every promised out-reason even on an early return. Reject truncated and overlong frames, including an extra byte after an otherwise valid reply.

## Acceptance

- Use the actual C++ API against the checked-in vectors. Verify all four read request/reply pairs and both FC06 pairs byte for byte, including the corrected address write at its original address. Test the internal FC06 encoder here and the public guarded write route again in 05. Keep the Python vectors as independent evidence.
- Cover null and short buffers with output canaries, empty input, one-byte-short/long replies, wrong address/function, wrong byte count, corrupt and byte-swapped CRC, unknown exception code, padded exception, and mismatched FC06 register/value with a valid CRC. Verify rejected data cannot replace a prior value. Test the supported FC03 count boundaries at the actual API layers.
- Use examples of two one-word requests to the same address with identical reply bytes. Show why only the retained, live owner context can select the engineering unit. Execute native tests and the offline protocol regression checks.

## Handoff

Record `rtu-codec` in [PROGRESS](PROGRESS.md) with the exported signatures, builder bounds, validator precedence and positive/negative test results.
