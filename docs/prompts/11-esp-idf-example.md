# 11 — Build the native ESP-IDF bench application

## Inputs

Apply [COMMON](COMMON.md). Consume `owner-contract` and `shared-cli`. This stage produces `idf-example`. Read the [native package workflow](../IDF_PORT.md), [CLI contract](CLI_CONTRACT.md), [SHZK-PT native app](https://github.com/janhavelka/SHZK-PT/tree/59d76c502a90bef8038ced2b72a2d50df98184e9/examples/espidf_basic) and [TMP1x2 native CLI](https://github.com/janhavelka/TMP1x2/blob/afd50889312f6e218a6a34eca687e3e8547d8b40/examples/esp_idf/basic/main/main.cpp).

## Work

1. Add a native project at `examples/espidf_basic/` with top-level and `main/` CMake files, `sdkconfig.defaults`, `main/main.cpp` defining `extern "C" void app_main()`, and a project README. Register the root library with the stable component name `sen059x`, even if the checkout is renamed. Follow the sibling example's small CMake wrapper or an equivalent named component registration. Link the public component and the shared example sources exactly once. Keep Arduino includes, CLI, console, UART, GPIO, timers and FreeRTOS dependencies out of the root library.
2. The example `main` component declares `REQUIRES sen059x` and its actual ESP-IDF 5.5.5 driver dependencies such as `esp_driver_uart`, `esp_driver_gpio`, `esp_timer` and `freertos`. Use a bounded nonblocking console backend, e.g. the default UART0 console at 115200 on a DevKitC-1 USB/UART bridge. Keep one selected backend responsible for input and output. Document the USB connector, selected console, any boot logs and console pins. Use a dedicated RS485 UART such as `UART_NUM_2` at 115200 8N1, with RX16/TX17/DE4 as illustrative defaults and board overrides `SEN059X_RX_GPIO`, `SEN059X_TX_GPIO`, `SEN059X_DE_GPIO`. Validate all resources and pin conflicts before enabling transmit.
3. Initialize RX direction safely. Configure the port, start the shared owner/CLI and service it cooperatively. Keep console admission, cancel, cached observations and deadlines responsive while UART/terminal is silent or slow. Use monotonic microseconds for RTU timing, bounded work per wake and explicit care with RTOS tick conversion. An IDF buffered read timestamp only establishes when software obtained bytes. Qualify frame boundaries using the evidence the backend actually has. Expose overflow, parity/frame errors, short TX, timed-out physical drain, possible local echo, recovery and GPIO/driver failure with original signed detail. Release DE only after established TX completion or the documented failure/recovery path.
4. Use the entire stage 09 CLI registry, both renderers and the public typed library operations. Apply all eight sensor baud rates in the standalone IDF UART adapter, including 4800, 14400 and 76800. Return the native detail when a configuration call fails. Enforce explicit SKU selection and a usable console throughout startup. Source a fresh boot ID from the platform and report the actual ESP-IDF 5.5.5 version, application revision, library version, target and parameters in the handshake. Build with the [version contract](VERSIONS.md).
5. Keep two distinct native builds. This app proves the real `app_main`, console and RS485 driver link. The existing independent `--prepare-idf` fixture proves consumers can install the archive. Name both in verification and the CI stage. Keep generated SDK config, build outputs, port-specific settings and hardware transcripts outside published package files.
6. Make all four measurement kinds and eight writable settings discoverable from an ordinary IDF monitor session. Link the shared command reference from the app's README and supply a runnable IDF walkthrough for conditional register writes, addressed readback, extended-address restoration, and explicit vendor send-only FC06. Show output units, conditional mode labels, legal SKU limits, independent verification steps, and where to inspect an exception or uncertain link. Use the same command forms as stage 10.
7. Expose the shared `readregs` diagnostic at the same IDF monitor entry point. Exercise a contiguous three-word FC03 and a five-byte exception for a two-word request through the scripted IDF composition. Verify exact-length RX for the accepted span, finite timeout, raw words or an explicit error and zero typed-cache publication. Include one runnable diagnostic command in the IDF walkthrough and identify the SKU's undocumented block behavior.

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

Build one pin-override variant and verify component resolution after renaming the checkout. Exercise the stage 09 matrix through the IDF application's platform-independent composition with scripted port/clock hooks. Confirm `app_main` uses this registry and the IDF backend in the linked ELF. Compare human help, all twelve read commands, eight confirmed addressed writes, eight isolated broadcast forms, both address policies and eight baud rates with the Arduino expectations. Exercise failure propagation for one unsupported backend configuration and a valid device exception. Keep SDK console identifiers and version identity specific to the image. Check example-only dependencies stay out of the package consumer. Record the monitor command using a user-supplied port, e.g. `idf.py -C examples/espidf_basic -B build-cli -p <PORT> monitor`. Label physical RS485 observations by fixture.

## Delegation and self-audit

Apply the [stage audit](AUDIT.md). Ask the reviewer to locate the CLI application's real app_main and hardware adapter and the separate installed-package consumer. Check both builds and exercise the host-testable IDF application composition through the operation matrix.

## Handoff

Record `idf-example` in [PROGRESS](PROGRESS.md) with SDK, target, exact build commands, artifact paths, console/RS485 assignments, alternate pin build and any unresolved physical limits. Stage 12 uses the same external runner for both adapters. Stage 13 builds the native example in the pinned SDK job in addition to the package fixture.
