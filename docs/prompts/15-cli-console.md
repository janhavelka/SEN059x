# 15 — Build the human console and passive commands

## Start

Follow [COMMON](COMMON.md) and inspect the predecessor entries in [PROGRESS](PROGRESS.md). Locate the native target, device health observer and scheduler snapshot. Run their recorded tests before adding console code. Own the processor, registry, human renderer and native console tests under the example/test trees. Finish with a usable local terminal. Live operations start in the next prompt.

## Inputs

Consume `device-health` and `owner-scheduler`. Produce `cli-console`. Use the command spellings, presentation and limits in [Human console](CLI_CONTRACT.md#human-console) and [Parser and cooperative service](CLI_CONTRACT.md#parser-and-cooperative-service).

## Work

1. Build the shared processor under `examples/common/` with injected console sink, clock, identity and owner hooks. One registry drives dispatch, grouped help, detailed usage and aliases. Advertise implemented commands. Give later stages an entry point to extend the same registry.
2. Implement the console, local binding and cached groups, plus the offline `selftest`. Cover `?`, `ver`, `sample`, color and verbose. Render status, exact health fields, per-kind freshness, configuration trust, diagnostic detail and units. Model selection is explicit. A local baud change uses the backend's admitted rates and reports failures. Reject rebinding while work or recovery is active.
3. Service the line editor, dispatch and output cooperatively with the documented caps. Retain incomplete output under backpressure. Never dispatch the surviving prefix of a poisoned line.

## Acceptance

Run `ctest --test-dir .pio/sen059x-native -C Release --output-on-failure` after rebuilding the native target. [Native build recipe](VERIFICATION.md)

Compile the actual processor in the native target. Feed byte streams through its input with a fixed clock and a backend that fails unexpected sensor transfers. Verify zero TX at startup, attach, help, target selection, cache queries and self-test. Compare grouped help with dispatch. Exercise maximum lengths, invalid arity/numbers, CR/LF/CRLF, backspace/DEL, overflow, invalid bytes, stalled output and recovery on the next line.

## Delegation and self-audit

Apply the [stage audit](AUDIT.md). Delegate registry tests to one agent and ask an independent reviewer to append a valid command to an overlong prefix and issue a cached query while busy. Fix findings and rerun both streams.

## Handoff

Record `cli-console` in [PROGRESS](PROGRESS.md) with processor/registry paths, native invocation, captured human help and zero-TX evidence.
