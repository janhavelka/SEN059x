# 21 — Build the native ESP-IDF console and RS485 port

## Start

Follow [COMMON](COMMON.md). Check the shared-console evidence in [PROGRESS](PROGRESS.md), then confirm SDK 5.5.5 and the existing root component registration. Own the new native app, its CMake files, SDK defaults, adapter and focused port tests. Stop with the native application linked, without embedding example dependencies in the published component.

## Inputs

Consume `shared-cli`. Produce `idf-port`. Read the [SDK contract](VERSIONS.md), [IDF integration notes](../IDF_PORT.md) and [native build instructions](VERIFICATION.md#native-esp-idf-esp32-s3).

Verify the independent audit handoff `cli-audit` before using the covered work.

## Work

1. Create `examples/espidf_basic/` with top-level and `main/` CMake files, `sdkconfig.defaults` and `main/main.cpp` defining `extern "C" void app_main()`. Register the root under stable component name `sen059x`. Declare `REQUIRES sen059x` and used SDK components such as `esp_driver_uart`, `esp_driver_gpio`, `esp_timer` and `freertos` in the example's main component. Link the complete shared CLI and owner once.
2. Specify one bounded console path such as the DevKitC-1 UART0 bridge at 115200, and an independent RS485 UART such as `UART_NUM_2`. Support the existing RX/TX/DE override names and validate pin conflicts before TX. The illustrative RX16/TX17/DE4 wiring follows the Arduino example.
3. Keep the owner in receive through boot and SKU selection, including the sensor startup delay. Service console, owner deadlines and output cooperatively. Use monotonic microseconds, bounded SDK calls and deliberate RTOS tick conversion. Propagate native errors, observed TX/drain, buffer overflow, parity/framing, echo ambiguity and recovery. Support all eight sensor rates. Supply fresh boot identity, actual SDK/target, firmware revision and library version.

## Acceptance

In the activated 5.5.5 shell run `idf.py -C examples/espidf_basic -B build-cli set-target esp32s3 build`. The linked build instructions give the Windows side-by-side equivalent.

Build the real app for ESP32-S3 with SDK 5.5.5 in its own build directory. Locate `app_main`, the shared dispatcher and the native adapter in the link. Check bounded progress with a silent UART, stalled console and failed driver call. Record which timing/closure facts an IDF buffered read can actually establish.

## Delegation and self-audit

Re-read original prompt 21 in full and perform a fresh, independent audit of the completed work.

Use subagents in parallel to inspect requirements coverage, implementation correctness, edge cases, tests, scope, and code simplicity. Verify their findings against the actual code and diff, not the previous summary.

Apply the [stage audit](AUDIT.md). Delegate component/build inspection and ask an independent reviewer to inject a short TX and timeout during physical drain. Check DE and terminal evidence before accepting the next exchange. Fix findings and repeat.

Fix every confirmed gap or bug within the original scope. Prefer simple, readable corrections and remove unnecessary complexity. Repeat the required verification and continue until the implementation fully satisfies the original prompt.

## Handoff

Record `idf-port` in [PROGRESS](PROGRESS.md) with SDK version, component/console selection, command, ELF, pins, error cases and outstanding hardware observations.

Finish with a concise report of the audit findings, fixes made, and final verification results.
