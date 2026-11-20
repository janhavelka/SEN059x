# 12 — Execute isolated commissioning through the bus owner

## Start

Apply [COMMON](COMMON.md), find the settlement and write-trust evidence in [PROGRESS](PROGRESS.md), and rerun the existing lost-ACK and partial-TX fixtures. Own the application-side commissioning admission and orchestration in `examples/common/` with native tests. Stop when one confirmed operation can be settled, observed and inspected.

## Inputs

Consume `configuration` and `owner-scheduler` and produce `owner-contract`. Read [operator commissioning rules](CLI_CONTRACT.md), [all eight write cases](OPERATIONS.csv) and the [register evidence](../reference/measurement-and-configuration.md).

## Work

1. Connect the portable write-verification transitions to the real owner steps. A preview causes zero TX. Admit one confirmed operation with a one-use isolated maintenance grant. Default to `StandardUnicast`. Check enabled SKU, known endpoint, host capability, address collisions and explicit policy before any wire effect. Reject unsupported new baud rates before TX.
2. For all eight ordinary writes, retain original/intended endpoint, primary and cleanup status, TX/ACK evidence and same-register FC03 verification. Address changes verify at the proposed address. Baud changes verify at the new rate. Preserve the uncertain ACK rate, unknown persistence and old value as historical when TX may have changed the unit. Bounded explicit readback can establish a later observation after settlement. No automatic scan, rewrite or rollback.
3. Under the explicit DFRobot isolated profile support extended 248..254 endpoints and restoration to a standard address. A distinct confirmed vendor FC06 to 0xFF terminates on local send settlement. Keep possible effects unverified, settle unsolicited bytes, preserve link/setting uncertainty and leave any later addressed readback to an explicit operation. Vendor TX creates zero device communication successes by itself.

## Acceptance

Drive all eight settings on each SKU through addressed and isolated send-only paths. Test zero-TX refusal, 247→248 and 254→1, lost ACK plus valid readback, verified mismatch, wrong echo, silence at new baud, short TX, failed cleanup, cancellation and old-generation bytes. Verify immutable original result, no retry of FC06, fresh observation versus historical state, unchanged measurement units at both UART-unit values and unaffected peer bindings. Run native owner and commissioning suites.

## Delegation and self-audit

Follow [AUDIT](AUDIT.md). Delegate transaction/word expectations separately. Have a reviewer remove a genuine ACK, inject local echo after a vendor send and inspect a passive status query. Fix trust or duplicate-TX defects and rerun both paths.

## Handoff

Record `owner-contract` in [PROGRESS](PROGRESS.md) with admission/confirmation call sequence, supported policies, exact operation counts, causal wire trace, uncertain outcomes and checks usable by both full examples.
