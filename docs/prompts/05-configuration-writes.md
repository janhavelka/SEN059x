# 05 — Validate settings and verify commissioning writes

## Inputs

Consume `rtu-codec` and `measurements`. Apply [COMMON](COMMON.md). Use [table 2 for both models](../reference/protocol/registers.csv), [configuration evidence and conflicts](../reference/measurement-and-configuration.md), [FC06 examples](../reference/protocol/modbus_vectors.csv) and the [public conventions](CONVENTIONS.md). Produce `configuration`.

## Work

1. Implement typed FC03 readback of every documented configuration word, retaining the raw value even for an unrecognized code. Track desired, observed and verified values and their validity independently. Unknown factory settings remain unknown. Normal startup performs zero writes. Cover these controls through the public API.

   | Register | Rules |
   | --- | --- |
   | `0x0200` address | Raw DFRobot range 1..254, vendor broadcast 255. Use standard unicast 1..247 for addressed commands and new addresses. Default 1. |
   | `0x0201` baud | Codes 2..9 in order give 4800, 9600, 14400, 19200, 38400, 57600, 76800 and 115200 baud. Default 9, 8N1. Use the Chinese table and independent published 9600 example. Show the backend's separately supported subset. |
   | `0x0205`, `0x0206` switch | Below threshold, 0 means low, 1 means high (default). Threshold 30..3000 or 30..5000 mm by SKU. Equality behavior and factory threshold unspecified. |
   | `0x0208` angle | Levels 1..4, default 4. Use the model-specific angles and SEN0592 single/horizontal/vertical labels from table 2. Preserve source differences. |
   | `0x0209` UART unit | 0 mm, 1 µs, applies to automatic/controlled UART output. FC03 measurement units remain those in table 1. |
   | `0x021A` noise | Levels 1..5, default 1. Document greater noise suppression and its unspecified effect on beam angle. |
   | `0x021F` maximum range | SEN0591 levels 1..4, default 4. SEN0592 levels 1..5, default 5. Label nominal range separately from rated measuring distance. |

2. Expose a checked `buildWriteSingleRegister` that requires an explicit profile and validates register/access/value before emitting eight bytes. Allow normal commissioning writes for address, baud, angle, noise and range. Keep the switch and UART-mode settings available through explicit readback and documentary metadata. Their physical functionality on the RS485 cable is unestablished. Reject attempted writes to these registers through the supported commissioning route with `UNSUPPORTED`. Reject unknown addresses, measurement registers and unsupported functions without TX. The CLI can preview the plan. Its confirmation and maintenance-window checks run in the owning application.
3. Describe changes as a testable sequence of values or steps that a shared owner can execute. Keep the current endpoint, intended endpoint, TX progress, acknowledgement source/shape, independent FC03 readback and comparison individually observable. For ordinary settings verify the same register on the same binding. For address, verify `0x0200` at the new address and compare the returned word with that address. The published address echo uses the old address. For baud, verify `0x0201` at the selected new rate with the same slave. The write takes effect immediately and the acknowledgement rate is unknown. Preserve the explicitly selected SKU.
4. Treat single-device isolation and absence of address conflicts as owner admission requirements. A valid FC06 echo is byte-identical to the request. Even a genuine acknowledgement leaves verification pending. If an ACK is lost, read back at the expected endpoint after the bus settles. A verified matching setting can be reported alongside the missing ACK. Record mismatches as `CONFIG_MISMATCH` and unresolved link changes as uncertain/`LINK_LOST` with their underlying error. Keep actual UART reconfiguration, draining and timing out of the codec. Exclude blind write replay, scanning, automatic rollback, target migration on send alone, and persistence claims on a live readback.

## Acceptance

Use native deterministic tests to exercise each typed read and every supported write at its minima/maxima and just outside. Check round trips for all eight baud codes and reject unknown codes. Reject addresses 0, 248, 254 and 255. Confirm range level 5 is rejected on SEN0591 and accepted on SEN0592. Validate the two published FC06 examples and old/new endpoint choices. Script wrong echo-register/value, local echo only, missing ACK plus matching verification, verified mismatch, unexpected readback value, power interruption and silence after a baud/address change. Verify no portable call performs I/O, no write updates observed evidence, and no unresolved change silently retargets future reads.

## Handoff

Record `configuration` in [PROGRESS](PROGRESS.md) with register access policy, checked builder and readback signatures, what constitutes verified state, and the owner obligations exercised in stage 07.
