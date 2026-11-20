# 20 — Verify the Arduino example under both peer toolchains

## Start

Apply [COMMON](COMMON.md) and reproduce the primary build recorded in [PROGRESS](PROGRESS.md). Find its real `setup()`/`loop()` composition, port hooks and shared dispatcher. Own the second Arduino environment, parity tests, build instructions and operator walkthrough for this example. Stop at two linked firmware images plus reproducible host parity evidence.

## Inputs

Consume `arduino-primary`. Produce `arduino-example`. Use [toolchain pins](VERSIONS.md), [profile-specific build commands](VERIFICATION.md#arduino-esp32-s3) and the [operation matrix](OPERATIONS.csv).

## Work

1. Add `sen059x-cli-fieldcore-esp32s3` alongside the official profile. Build the identical app, port, owner and public library under isolated Core 6.1.19 with pioarduino 55.03.311 and Arduino-ESP32 3.3.11. The primary build uses Core 6.2.0 and Arduino 3.3.12. Capture each resolver output, SDK version and ELF. On Windows use the managed wrapper only when its Core matches the selected lane.
2. Run the full command matrix through the Arduino application composition with scripted UART, console and clock. Confirm each board entry point actually links that composition. Exercise both renderers on both SKUs, twelve reads, eight writes, eight send-only cases, standard and extended addresses, all sensor baud rates, and raw block success/exception with exact RX bounds.
3. Write the USB/monitor and wiring instructions. Give runnable commands for every operation, heading users to the shared command reference. Include model selection, switch/UART settings, previews, verified addressed readback, uncertain result, `readregs`, extended-address restoration and send-only verification. Document `pio device monitor --baud 115200` with a port chosen for the board.

## Acceptance

Run `pio run --project-dir examples/01_basic_bringup_cli -e sen059x-cli-fieldcore-esp32s3` in its Core 6.1.19 environment, and the primary environment command under Core 6.2.0.

Run both named environment builds in separate tool environments. Replay the matrix from the documented entry point. Check reported identity against the selected lane, zero hardware traffic for cached output, per-command raw bytes and preservation of the previous valid sample on failure. Distinguish a host-injected result from a firmware build or board observation.

## Delegation and self-audit

Follow the [stage audit](AUDIT.md). Delegate profile resolution and ask an independent reviewer to swap the two Core environments and inject a rejected baud change. Detect the wrong build identity and preserve the signed backend error. Fix findings and repeat.

## Handoff

Record `arduino-example` in [PROGRESS](PROGRESS.md) with both package lists, artifact paths, exact commands, matrix counts by SKU and separately recorded hardware gaps.
