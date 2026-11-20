# 23 — Implement the Python client for the example console

## Start

Apply [COMMON](COMMON.md). Replay the machine handshake and one asynchronous result from [PROGRESS](PROGRESS.md). Confirm the machine prefix, size limit and canonical command names against the dispatcher. Own the Python console client, its stream tests and quick-start. The direct USB–RS485 read-only probe has a different port and wire protocol. Stop with an offline-tested client and a selectable live console connection.

## Inputs

Consume `shared-cli`. Produce `runner-client`. Use the [machine channel](CLI_CONTRACT.md#machine-channel-for-the-host-runner) and the dispatcher transcripts produced by the predecessor.

## Work

1. Implement a Python 3 client with bounded deadlines, explicit console port, optional pySerial and stdlib offline replay. Attach using `@1 session <nonce>`, confirm schema/boot/build/target and issue monotonic request IDs. Accept an anchored `SEN059X ` record only for the matching boot/session/request/command and, when allocated, matching operation generation.
2. Distinguish immediate terminal commands from an accepted command followed by its terminal record. Report code, signed detail, operation disposition, freshness, data and elapsed time. An accepted cancellation control can return `OK` while the original job finishes `ABORTED`/`Cancelled`. Require both terminal status and disposition for a successful measurement.
3. Bound partial lines, tolerate ordinary human output and recover at the next record boundary. After a disconnect, use a fresh session and explicit passive reconciliation. A missing terminal leaves the effect unknown. Avoid automatic replay of triggers and writes.
4. Show smoke commands for the example console and independent direct adapter. Keep one master on the sensor bus. Export machine-readable and human summaries that identify firmware, library, board, session, suite and fixture type.

## Acceptance

Register offline client tests with `python -m unittest discover -s test -v` and publish one working replay command with a checked-in fixture.

Run against real dispatcher transcripts plus fake streams for an immediate result, accepted/done, chunked records, malformed/oversized JSON, wrong schema, duplicate terminal, stale boot/session/op, stalled input and reboot after TX. Check deadlines expire and a later independent query can succeed without repeating an earlier action.

## Delegation and self-audit

Apply the [stage audit](AUDIT.md). Delegate parser cases. Have a separate reviewer splice a prior session's successful write into the current transcript and verify the client refuses credit. Fix and rerun.

## Handoff

Record `runner-client` in [PROGRESS](PROGRESS.md) with client path, smoke/replay commands, dispatch transcript origin, timeout behavior and reproduced negative cases.
