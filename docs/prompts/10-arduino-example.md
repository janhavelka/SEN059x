# 10 — Run the common CLI in the ESP32-S3 Arduino example

## Inputs

Apply [COMMON](COMMON.md). Consume `owner-contract` and `shared-cli`. This stage produces `arduino-example`. Inspect the [existing ESP32-S3 probe](../../examples/platformio-esp32/src/main.cpp), its [board configuration](../../examples/platformio-esp32/platformio.ini), the [CLI contract](CLI_CONTRACT.md) and the pinned [SHZK-PT Arduino example](https://github.com/janhavelka/SHZK-PT/tree/59d76c502a90bef8038ced2b72a2d50df98184e9/examples/01_basic_bringup_cli).

## Work

1. Create the independently buildable PlatformIO project `examples/01_basic_bringup_cli/` with environment `sen059x-cli-esp32s3`. Compile the shared processor and owner sources once and consume the root SEN059x library through its public headers. Keep the existing standalone read-only probe and its root and nested build commands usable. Retain the audited ESP32-S3 DevKitC-1 target, Arduino framework and pinned `platformio/espressif32@8.0.0` unless a later explicit upgrade changes the repository pin.
2. Use `Serial` for the human console at 115200 and an independent hardware UART for the RS485 transceiver, following the existing probe's `Serial2`. Provide the existing board overrides `SEN059X_RX_GPIO`, `SEN059X_TX_GPIO`, `SEN059X_DE_GPIO`. Example values are GPIO16 RX, GPIO17 TX, GPIO4 DE with DE and /RE tied, active high for transmit. Document the 3.3 V MCU/transceiver connection, sensor supply per SKU, common reference and A/B wiring from the [hardware guide](../reference/hardware.md). Validate pin conflicts, including console pins, before enabling TX. Label board pins, transceiver-enable settle time and echo mode as fixture choices.
3. Start in receive, set host defaults to slave 1, 115200 8N1, and explicitly select the SKU in board config or via `model` before any sensor operation. Service the console during the sensor's documented power-up interval, up to 1000 ms. Document the power-on time assumption for independently powered sensors. Do not transmit at boot, on terminal attach, on cache queries, or while waiting for target selection. Keep a single active request per UART.
4. In `setup()` configure board resources and initialize the processor. In `loop()` pump bounded input, owner service and output. Make console disconnect/backpressure and a silent bus leave the loop responsive. Implement the stage 07 port hooks for actual TX byte count, last-stop-bit drain, DE release, monotonic timing, frame closure and overflow. Establish finite limits for each SDK call, including TX/drain. A write accepted by the UART FIFO alone cannot justify DE release. When this backend cannot observe a timing boundary or distinguish an adapter echo, expose that limitation through the owner outcome. Preserve queued RX during TX drain.
5. Document the exact board, console and bus UART, 8N1, baud list, build-time overrides and USB/monitor selection. Include a short command walkthrough (`help`, `model SEN0591`, `read realtime`, `latest`, `health`, `job status`, `result`, `setaddr 2` preview). Reuse the shared command reference. Label synthetic transcripts, compile results and actual attached-hardware observations separately.

## Acceptance

Run these builds from the repository root. On Windows substitute `.\scripts\pio.cmd` for `pio`. Also rerun the native CLI/owner tests. Retain the platform/package versions and output firmware locations in the stage evidence.

```sh
pio run --project-dir examples/01_basic_bringup_cli -e sen059x-cli-esp32s3
pio run -e esp32-s3-devkitc-1
pio run --project-dir examples/platformio-esp32 -e esp32-s3-devkitc-1
```

Verify an alternate legal board pin configuration reaches the compiled port, and an invalid/conflicting configuration fails cleanly at build or pre-init. Confirm shared help, version and schema come from the same sources used by the host tests. Document `pio device monitor --baud 115200` with a port chosen by the operator. Record uploads and bench readings independently in stage 14. Stage 13 retains both the CLI and existing probe build in CI.

## Handoff

Record `arduino-example` in [PROGRESS](PROGRESS.md) with the project path, exact commands, framework and board, included portable sources and limits requiring physical verification. Identify the specific transceiver and wiring used only if an actual fixture was observed.
