# 24 — Exercise both example applications across three profiles

## Start

Follow [COMMON](COMMON.md). In [PROGRESS](PROGRESS.md) find the two Arduino builds, IDF build and runner smoke evidence. Verify their resolver/target identities and run one human-equivalent machine request through each application's host-testable composition. Own runner suites, fault inputs and evidence output. Finish with offline parity evidence and separately identified live-bench obligations.

## Inputs

Consume `arduino-example`, `idf-example` and `runner-client`. Produce `runner-evidence`. Use the [operation matrix](OPERATIONS.csv), [write/readback grammar](CLI_CONTRACT.md#configuration-commands-for-both-example-applications) and [version targets](VERSIONS.md).

## Work

1. Run both SKUs through primary Arduino, FieldCore-compatible Arduino and native IDF compositions. Capture actual dispatcher records and owner traffic. Exercise all twenty addressed operations, numeric/alias forms, finite watch, cancellation, recovery and passive commands. Make live commissioning a separately selected single-device suite. Document original-value read, preview, one confirmation, addressed verification and restoration.
2. Add a separately selected vendor send-only suite covering every writable key on the isolated profile. Check 248–254, invalid assignments and broadcast reads, every sensor baud, threshold/range SKU bounds, each valid angle/noise level, both switch polarities and UART units. Confirm vendor destination 255, uncertain effect, no ACK claim, zero automatic readback/replay and a distinct request for subsequent verification.
3. Exercise block reads on documented contiguous spans, including count one, with ordered raw output, exact timeout or exception and no cache publication. Reject holes, wrap, invalid count or broadcast before TX. Live block diagnostics require explicit selection because a span can initiate ranging.
4. Inject admission timeout, lost ACK, readback mismatch, cancellation during drain, stale terminal, reboot and backpressure. Verify historical trust after possible effects, preserved trust before TX, unchanged last-good measurement and continued service.

## Acceptance

Inject expiry after acceptance but before TX, then expiry during FC03 verification after an FC06. Prove zero writes and intact prior trust in the first case, and exactly one write, uncertain current value and no replay in the second. Record the actual backend and commit for each transcript. A shared host replay supplies behavioral evidence once. List each ELF and profile identity separately and mark board execution only where it occurred.

Register the offline campaigns in CI and document one command that reproduces the three-profile result locally. Run `python -m unittest discover -s test -v` for the regression suite.

Emit one row per SKU/profile/case with command, register, policy, wire counts/values, disposition, code, expected/actual outcome and fixture. Distinguish missing hardware from failed tests. Compare both applications and all three builds with independent expected words, including zero-TX previews/refusals and at most one FC06 for each confirmed command.

## Delegation and self-audit

Use the [stage audit](AUDIT.md). Delegate case enumeration. Have an independent reviewer inject a reboot after a write and a delayed reply from another session. Fix findings and rerun the affected profiles.

## Handoff

Record `runner-evidence` in [PROGRESS](PROGRESS.md) with exact replay commands, profile/operation totals, signed outcomes, labeled artifacts and remaining hardware observations.
