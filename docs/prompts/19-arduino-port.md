# 19 — Bring up the Arduino console on the primary ESP32-S3 profile

## Start

Use [COMMON](COMMON.md) and replay the shared-console session from [PROGRESS](PROGRESS.md). Check `pio --version`, the existing read-only probe and the ESP32-S3 pin overrides before changing code. Own `examples/01_basic_bringup_cli/`, primary profile pins used by the probes, the Arduino adapter and its focused tests. Stop with the primary firmware linked and its port behavior verified to the extent observable in software.

## Inputs

Consume `shared-cli`. Produce `arduino-primary`. Use [version pins](VERSIONS.md), [Arduino build commands](VERIFICATION.md#arduino-esp32-s3), the [existing probe](../../examples/platformio-esp32/src/main.cpp) and [wiring guide](../reference/hardware.md).

## Work

1. Create `sen059x-cli-esp32s3` under PlatformIO Core 6.2.0, official Espressif32 8.0.0 and Arduino-ESP32 3.3.12 with the exact S3 package pins. Use public sensor headers and the complete shared console, including both renderers. Keep both original read-only probe entry points working with the primary pins.
2. Use `Serial` for the 115200 console and the probe's `Serial2` for the independent RS485 UART. Preserve `SEN059X_RX_GPIO`, `SEN059X_TX_GPIO` and `SEN059X_DE_GPIO` and reject console pin conflicts before enabling TX. Illustrative pins are RX16/TX17/DE4 with tied DE and /RE. Document transceiver polarity, 3.3 V interface, common ground, A/B and SKU supply requirements.
3. Start in receive, address 1 at 115200 8N1, with explicit SKU selection. Service the console throughout the up-to-1000 ms sensor startup interval. In `setup()` and `loop()` drive the shared owner and bounded console. Implement all eight baud rates, monotonic timing, RX overflow, short TX, last-stop-bit drain, DE release and recovery. Preserve RX during drain and expose limits of buffered timing/echo evidence.

## Acceptance

Set finite bounds on each SDK TX and drain call. Stall the UART while pumping the console and show that passive queries and deadline handling remain responsive. For an independently powered sensor, document how the owner establishes startup readiness when it has no observed power-on timestamp.

Use `pio run --project-dir examples/01_basic_bringup_cli -e sen059x-cli-esp32s3` under Core 6.2.0. Run the two existing probe commands in the build recipe.

Build the primary CLI plus root and nested probes. Save resolved packages and artifact paths. Exercise startup/attach with zero sensor transfers, one real handler through the injectable application composition and a conflicting pin selection. Check a legal override reaches the compiled adapter.

## Delegation and self-audit

Apply the [stage audit](AUDIT.md). Delegate pin/build checks and have an independent reviewer inject a TX accepted into the FIFO followed by failed drain. Fix and repeat the check for premature DE release or false success.

## Handoff

Record `arduino-primary` in [PROGRESS](PROGRESS.md) with entry point, build commands, versions, artifacts, port tests and physical timing limits.
