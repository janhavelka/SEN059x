# 11 — Build the native ESP-IDF bench application

## Inputs

Apply [COMMON](COMMON.md). Consume `owner-contract` and `shared-cli`. This stage produces `idf-example`. Read the [native package workflow](../IDF_PORT.md), [CLI contract](CLI_CONTRACT.md), [SHZK-PT native app](https://github.com/janhavelka/SHZK-PT/tree/59d76c502a90bef8038ced2b72a2d50df98184e9/examples/espidf_basic) and [TMP1x2 native CLI](https://github.com/janhavelka/TMP1x2/blob/afd50889312f6e218a6a34eca687e3e8547d8b40/examples/esp_idf/basic/main/main.cpp).

## Work

1. Add a native project at `examples/espidf_basic/` with top-level and `main/` CMake files, `sdkconfig.defaults`, `main/main.cpp` defining `extern "C" void app_main()`, and a project README. Register the root library with the stable component name `sen059x`, even if the checkout is renamed. Follow the sibling example's small CMake wrapper or an equivalent named component registration. Link the public component and the shared example sources exactly once. Keep Arduino includes, CLI, console, UART, GPIO, timers and FreeRTOS dependencies out of the root library.
2. The example `main` component declares `REQUIRES sen059x` and its actual ESP-IDF 5.5.5 driver dependencies such as `esp_driver_uart`, `esp_driver_gpio`, `esp_timer` and `freertos`. Use a bounded nonblocking console backend, e.g. the default UART0 console at 115200 on a DevKitC-1 USB/UART bridge. Keep one selected backend responsible for input and output. Document the USB connector, selected console, any boot logs and console pins. Use a dedicated RS485 UART such as `UART_NUM_2` at 115200 8N1, with RX16/TX17/DE4 as illustrative defaults and board overrides `SEN059X_RX_GPIO`, `SEN059X_TX_GPIO`, `SEN059X_DE_GPIO`. Validate all resources and pin conflicts before enabling transmit.
3. Initialize RX direction safely. Configure the port, start the shared owner/CLI and service it cooperatively. Keep console admission, cancel, cached observations and deadlines responsive while UART/terminal is silent or slow. Use monotonic microseconds for RTU timing, bounded work per wake and explicit care with RTOS tick conversion. An IDF buffered read timestamp only establishes when software obtained bytes. Qualify frame boundaries using the evidence the backend actually has. Expose overflow, parity/frame errors, short TX, timed-out physical drain, possible local echo, recovery and GPIO/driver failure with original signed detail. Release DE only after established TX completion or the documented failure/recovery path.
4. Use the exact CLI registry, rendered output, commissioning admission and write/readback logic from 09. Require the same explicit SKU selection, power-up readiness and passive startup. Source a fresh boot ID from the platform and report framework/version/build/target in the handshake. Document how to build with default and overridden pins, monitor, choose the SKU, obtain an actual reading and inspect a failed result. Explain how the reference device's baud support can exceed a particular owner/board's admitted set.
5. Keep two distinct native builds. This app proves the real `app_main`, console and RS485 driver link. The existing independent `--prepare-idf` fixture proves consumers can install the archive. Name both in verification and the CI stage. Keep generated SDK config, build outputs, port-specific settings and hardware transcripts outside published package files.

## Acceptance

Build for ESP32-S3 using the repository's ESP-IDF 5.5.5 pin. In an activated IDF shell use the following from the repository root. Specify an empty project fixture destination for a fresh package install.

```sh
idf.py -C examples/espidf_basic -B build-cli set-target esp32s3 build
python scripts/check_package.py --prepare-idf .pio/idf-consumer
idf.py -C .pio/idf-consumer -B build-package set-target esp32s3 build
```

For the Windows side-by-side SDK, use distinct explicit build directories. Prepare the package fixture as above.

```powershell
C:\esp\idf.cmd 5.5.5 -C examples/espidf_basic -B C:\esp\build\sen059x-cli -D IDF_TARGET=esp32s3 build
C:\esp\idf.cmd 5.5.5 -C .pio/idf-consumer -B C:\esp\build\sen059x-consumer -D IDF_TARGET=esp32s3 build
```

Build one pin-override variant and verify the component resolution when the checkout directory has a different name. Re-run host tests using the same CLI/owner sources. Check the produced ELF references the application and the root codec once and that example-only dependencies stay out of the package consumer. When hardware is available, distinguish a console handshake from actual RS485 traffic, verified measurements and commissioning evidence. Record the monitor command using a user-supplied port, e.g. `idf.py -C examples/espidf_basic -B build-cli -p <PORT> monitor`.

## Handoff

Record `idf-example` in [PROGRESS](PROGRESS.md) with SDK, target, exact build commands, artifact paths, console/RS485 assignments, alternate pin build and any unresolved physical limits. Stage 12 uses the same external runner for both adapters. Stage 13 builds the native example in the pinned SDK job in addition to the package fixture.
