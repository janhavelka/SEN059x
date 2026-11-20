# 13 — Exercise the FieldCore main consumer lifecycle

## Start

Apply [COMMON](COMMON.md), locate `owner-scheduler` in [PROGRESS](PROGRESS.md), and inspect the pinned default-branch `ShzkDeviceModule` call site. Own a host fixture under `test/native/` and its FieldCore evidence notes. Stop with the default-branch standard-unicast lifecycle and passive product status.

## Inputs

Consume `owner-scheduler` and produce `consumer-main`. Read the pinned binding, owner, actual SHZK/VibWire callers and product health contracts in [SOURCE_MAP](SOURCE_MAP.md). Use the literal result, health and status names in [CONVENTIONS](CONVENTIONS.md).

## Work

1. Use only installed-style SEN059x public headers and a bounded host fixture mirroring the consumer contract. It must build offline without private headers. Follow `bind`, `applyRuntimeConfig`, `start`, `poll`, `takeStep(step)`, `acceptObservation` and `takeResult`. Include `cancel`, `setEnabled`, `onBusInvalidated`, `snapshot`, `readingCatalog` and `copyStatus`. Preserve command identity, deadline and exactly one retained terminal result. Handle `Rs485ModuleAction::{None, RequestTransaction, WaitUntil}`.
2. Exercise `Probe` with a non-ranging configuration register, `Measure` with one selected register and `ReadLast` passively. Keep SKU explicit. Track per-kind raw value, units, timestamp, reading count and valid/stale/error masks. Supply the profile's pins/DE, standard address and explicit serial format. The owner's default 9600 differs from the factory 115200. Admit only the five product 8N1 baud rates and reject unsupported rates and vendor addressing before TX.
3. Mirror `DeviceModuleStartStatus`, `DeviceModulePollState`, `DeviceResultStatus`, `HealthState` and `PresenceState`. Track attempt/success/error times, count misses separately from corrupt/foreign frames, and keep product health, library communication state and measurement freshness distinct. Project `FRAME_TOO_SHORT` to `TruncatedFrame`, `CRC_MISMATCH` to `BadCrc`, wrong address/function to `UnexpectedAddress`/`UnexpectedFunction`, count/length errors to `ValueCountMismatch`, and an exact exception to `ExceptionResponse` with its raw detail. Retain `None`, `UnsupportedFrame` and `OutOfRangeValue` where applicable.

## Acceptance

In the default-branch fixture use the actual `FixedLength` and `Suffix` frame modes. Test all four identity/deadline fields of `DeviceCommand` and the three `DeviceModuleStatus` uptime fields defined in [public conventions](CONVENTIONS.md).

Run the native fixture through the actual public API for two bindings. Assert no unsolicited writes, independent status/masks, no transfers on cached reads, recovery from misses and valid exception without sample publication. Test unsupported, expired, disabled, busy, cancelled and duplicate completions. Model 8-byte TX/192-byte RX and fixed-length/suffix completion: a valid prefix or echo-stripped observation cannot supply missing frame-closure evidence.

## Delegation and self-audit

Follow [AUDIT](AUDIT.md). Delegate comparison with the pinned product declarations and lower-case display names. Have an independent reviewer attempt to consume a result twice and force an apparent success from a valid prefix. Fix and rerun those consumer paths.

## Handoff

Record `consumer-main` in [PROGRESS](PROGRESS.md) with the pinned default-branch revision, fixture path, commands, exact public call/result mapping and its evidence limits.
