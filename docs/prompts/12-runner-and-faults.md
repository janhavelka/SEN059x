# 12 — Exercise the host runner and fault recovery

## Inputs

Apply [COMMON](COMMON.md). Consume `shared-cli`, `arduino-example` and `idf-example`. Use the exact [CLI contract](CLI_CONTRACT.md) and stage 08 result dispositions. This stage produces `runner-evidence`.

## Work

1. Create a small Python 3 host client for the examples' **console** port. Provide a quick-start, bounded deadline, explicit port and selectable suite. Support stdlib offline fake streams/transcripts and optional pySerial for a live run. Retain the existing independent read-only sensor probe on the direct RTU adapter. Document both connections and keep exactly one master on the sensor bus.
2. Attach with `@1 session <nonce>`, parse the returned build/board/schema and boot identity, then dispatch the same commands as a human. Match anchored `SEN059X ` JSON records by boot, session, request, phase and operation. Stream partial input within the line limits. Accept one terminal result per operation. Report terminal code, detail, disposition, result evidence, freshness and elapsed time. An accepted `cancel` command can return `OK`. The original cancelled operation ends with `ABORTED` and `data.disposition` of `Cancelled`. The runner must check both status and disposition before passing a measurement assertion.
3. Expose read-only suites for help/version, cached status, probe via a configuration read, each measurement, watch with finite bounds, operation cancellation and recovery. Keep commissioning in a separately selected single-device maintenance suite. Capture TX/RX or instrumented owner counts so each passive command and preview demonstrably emits zero sensor requests. Give missing hardware a distinct skip reason and return failure for an expected check that does run and fails.
4. Simulate timeouts at admission, after acceptance, after TX and during readback. Inject data in chunks, unsolicited human text, stale or malformed records, duplicate IDs, client disconnect, reboot, late responses and output backpressure. Query retained results after reconnection where available. Permit explicit read-only reconciliation with the current binding. Never repeat a trigger or FC06 automatically to reconstruct a lost terminal result.
5. Record machine-consumable results plus a short human summary for CI or the bench. Include commit, dirty flag or unknown, framework, board, SKU, address, link settings, suite, fixture type and what was actually observed. Bind a recording to its device/build identity. Keep transcripts small and free of machine-specific secrets.

## Acceptance

Run the host client in CI against traffic emitted by the real shared dispatcher plus fault-injected fake streams. Test valid immediate and accepted/done sequences, stale boot/session, wrong command or operation, truncated JSON, duplicate terminal, oversized line, wrong schema, idle timeout and successful next-line recovery. Verify timeout/cancel preserves the last-good sample, an unsupported command causes no TX, and one lost write ACK results in at most one FC06. Use the same expectations for Arduino and IDF. Name any live targets still pending.

## Handoff

Record `runner-evidence` in [PROGRESS](PROGRESS.md) with the runner path, smoke commands for both ports, replay commands, result schema, pass/fail counts and outstanding HIL rows.
