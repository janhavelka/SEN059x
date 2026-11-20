# 14 — Compare the two FieldCore owner contracts

## Start

Apply [COMMON](COMMON.md), locate the passing default-branch fixture and isolated owner records in [PROGRESS](PROGRESS.md), and run the default fixture before adding the rodtester variant. Own the second native fixture mode and `docs/fieldcore_handoff.md`. Stop with both pinned revisions verified in SEN059x.

## Inputs

Consume `owner-contract` and `consumer-main` and produce `consumer-fixture`. Compare both exact revision paths in [SOURCE_MAP](SOURCE_MAP.md), [public conventions](CONVENTIONS.md) and [architecture notes](../ARCHITECTURE.md).

## Work

1. Extend the same fixture using rodtester's `takeStep(step, nowMs)`, 21-byte TX/192-byte RX and `Rtu` evidence. Preserve copied identity/generation, accessible versus total length, hardware errors, physical TX drain, closure bounds and `QualifiedResponse` versus `Ambiguous`. A buffered Arduino backend cannot provide precise wire timestamps. Configure only evidence the tested backend actually possesses.
2. For the same `Probe`, `Measure` and `ReadLast` calls, compare product and library states and masks. Exercise representative typed configuration operations through admitted standard bindings. Show how fixed-length/suffix reception and echo stripping on main differ from qualified closure and source evidence on rodtester. Preserve the unknown outcome of a potentially executed FC06. Extended addresses and send-only vendor writes stay with the standalone owner.
3. Check `Rs485DeviceStatus` counters and product health/presence under configured stale/fault thresholds. Include `lastStatus`, `lastParserError`, `parserErrorCount`, `consecutiveMissCount`, `staleCount`, `validMask` and the attempt/success/error uptime fields. Verify `DeviceModuleStartStatus::{Accepted, Busy, Disabled, Unsupported, Rejected}`, `DeviceModulePollState::{Idle, Running, Complete, NeedsBusRecovery}` and `DeviceResultStatus::{Ok, Disabled, Absent, Timeout, Stale, Cancelled, Failed}`. Compare `completedUptimeMs`, `readingCount`, `validMask`, `staleMask` and `errorMask` for each result. Keep profile dependency pins (SHZK-PT v3.0.0, VibWire-108 v1.2.0), backend limits and product integration status visible in the handoff.

## Acceptance

Build and run both offline fixture modes against the same public API. Inject qualified and ambiguous delivery, identical local echo, trailing bytes, failed drain, cancellation while active, disable/re-enable, bus invalidation and delayed duplicate completion. Verify exactly one terminal per admission, a second `takeResult` fails, link success can coexist with an exception, and recovery preserves historical reading age.

## Delegation and self-audit

Follow [AUDIT](AUDIT.md). Delegate a revision-by-revision API/size comparison. Have a separate reviewer feed main's fixed-length prefix to the qualified rodtester path and compare the outcomes. Correct any fabricated evidence and rerun both modes.

## Handoff

Record `consumer-fixture` in [PROGRESS](PROGRESS.md). Publish `docs/fieldcore_handoff.md` with pinned revisions, public call sequence, tested health/status/error mappings, commands, transport limitations and what an actual product build or bench still needs.
