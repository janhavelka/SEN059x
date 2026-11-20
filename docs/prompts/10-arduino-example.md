# 10 — Run the common CLI in the ESP32-S3 Arduino example

## Inputs

Apply [COMMON](COMMON.md). Consume `owner-contract` and `shared-cli`. This stage produces `arduino-example`. Inspect the [existing ESP32-S3 probe](../../examples/platformio-esp32/src/main.cpp), its [board configuration](../../examples/platformio-esp32/platformio.ini), the [CLI contract](CLI_CONTRACT.md) and the pinned [SHZK-PT Arduino example](https://github.com/janhavelka/SHZK-PT/tree/59d76c502a90bef8038ced2b72a2d50df98184e9/examples/01_basic_bringup_cli).

## Work

1. Create one independently buildable PlatformIO project at `examples/01_basic_bringup_cli/` with two environments targeting the ESP32-S3 DevKitC-1. `sen059x-cli-esp32s3` uses SHZK's PlatformIO Core 6.2.0, official Espressif32 8.0.0 and Arduino-ESP32 3.3.12. `sen059x-cli-fieldcore-esp32s3` uses PlatformIO Core 6.1.19, pioarduino 55.03.311 and Arduino-ESP32 3.3.11. Use the exact framework/tool package sources in the [version contract](VERSIONS.md). Build the same application, shared processor and owner with each environment and consume the root library through its public headers. Preserve the existing standalone read-only probe and its root and nested build commands.
2. Use `Serial` for the human console at 115200 and an independent hardware UART for the RS485 transceiver, following the existing probe's `Serial2`. Provide the existing board overrides `SEN059X_RX_GPIO`, `SEN059X_TX_GPIO`, `SEN059X_DE_GPIO`. Example values are GPIO16 RX, GPIO17 TX, GPIO4 DE with DE and /RE tied, active high for transmit. Document the 3.3 V MCU/transceiver connection, sensor supply per SKU, common reference and A/B wiring from the [hardware guide](../reference/hardware.md). Validate pin conflicts, including console pins, before enabling TX. Label board pins, transceiver-enable settle time and echo mode as fixture choices.
3. Start in receive, set host defaults to slave 1, 115200 8N1, and explicitly select the SKU in board config or via `model` before any sensor operation. Service the console during the sensor's documented power-up interval, up to 1000 ms. Document the power-on time assumption for independently powered sensors. Do not transmit at boot, on terminal attach, on cache queries, or while waiting for target selection. Keep a single active request per UART.
4. In `setup()` configure board resources and initialize the processor. In `loop()` pump bounded input, owner service and output. Make console disconnect/backpressure and a silent bus leave the loop responsive. Implement the stage 07 port hooks for actual TX byte count, last-stop-bit drain, DE release, monotonic timing, frame closure and overflow. Establish finite limits for each SDK call, including TX/drain. A write accepted by the UART FIFO alone cannot justify DE release. When this backend cannot observe a timing boundary or distinguish an adapter echo, expose that limitation through the owner outcome. Preserve queued RX during TX drain.
5. Compose the full stage 09 bench registry in both images. Implement all eight sensor baud rates in each standalone UART adapter, including 4800, 14400 and 76800. Surface a hardware/API error with its original detail if applying a rate fails. FieldCore's five permitted rates are a product policy. Supply an operator guide with both USB/monitor configurations, pin overrides, named and numeric reads, all eight typed writes including switch/UART settings, commissioning admission, extended-address selection/restoration and the vendor send-only form. For each operation list a runnable command and how to interpret verified, failed and uncertain results. Label synthetic transcripts, compile results and actual attached-hardware observations separately.

## Acceptance

Run these builds from the repository root. Run the official profile and original probes in the managed Core 6.2.0 environment. On Windows use `.\scripts\pio.cmd` for that environment when its reported version matches. Run the consumer profile in a separate Core 6.1.19 environment. Save the resolved packages and the build/firmware path for each CLI image. [Toolchain setup and exact pins](VERSIONS.md)

```sh
pio run --project-dir examples/01_basic_bringup_cli -e sen059x-cli-esp32s3
pio run -e esp32-s3-devkitc-1
pio run --project-dir examples/platformio-esp32 -e esp32-s3-devkitc-1
```

Use the consumer's pinned tool environment for the second build.

```sh
pio run --project-dir examples/01_basic_bringup_cli -e sen059x-cli-fieldcore-esp32s3
```

Exercise the stage 09 matrix on the actual application composition with scripted UART, console and clock hooks. Confirm the board entry point uses the same complete command registry and port hooks in both links. Check all twelve single-word reads, `readregs` on a documented contiguous span, all eight writable settings, both address policies, eight baud rates, isolated send-only FC06, and the matching UI and runner output. Include raw block success and device-exception cases with bounded RX and an unchanged typed cache. Record host evidence and each firmware build separately. A sensor or board run in stage 14 supplies additional physical evidence.

Verify an alternate legal board pin configuration reaches the compiled port, and an invalid/conflicting configuration fails cleanly at build or pre-init. Confirm shared help, version and schema come from the same sources used by the host tests. Document `pio device monitor --baud 115200` with a port chosen by the operator. Record uploads and bench readings independently in stage 14. Stage 13 retains both the CLI and existing probe build in CI.

## Delegation and self-audit

Apply the [stage audit](AUDIT.md). Have the reviewer build the same Arduino application on both pinned profiles. Exercise its application composition through the full operation matrix with an injected port and verify the resolved build identity and board pin selections.

## Handoff

Record `arduino-example` in [PROGRESS](PROGRESS.md) with the project path, exact commands, framework and board, included portable sources and limits requiring physical verification. Identify the specific transceiver and wiring used only if an actual fixture was observed.
