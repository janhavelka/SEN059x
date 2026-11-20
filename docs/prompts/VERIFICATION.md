# Verification commands and evidence

Run the checks for the current stage in a clean build directory. Record commit, target, tool version and result. The **baseline** commands work in the scaffold. Commands marked **implementation** become acceptance checks in the named stages. [Current workflow](../../.github/workflows/ci.yml) · [ESP-IDF package fixture](../IDF_PORT.md)

## Baseline and promptset

```sh
python scripts/check_promptset.py
python scripts/fetch_originals.py --verify
python -m unittest discover -s test -v
python examples/python/sen059x_probe.py --self-test
python scripts/generate_version.py check
python scripts/check_package.py
doxygen Doxyfile
```

On clean Linux CI install the repo's pinned Python dependencies first. On Windows use the managed PlatformIO wrapper, including its project arguments for nested examples. The package preparation step below requires a new or empty destination. Reuse a prepared directory for repeat SDK builds and prepare a fresh directory when the package changes. [Tool versions](../../requirements-ci.txt) · [Current build instructions](../../README.md#build-and-verify)

## Native C++ and consumer fixtures — implementation, stages 02–09

Establish a desktop harness under `test/native/` at stage 02, extend the same target through each portable stage and register the tests with CTest. The root CMake file registers the ESP-IDF component. Build the portable core, owner fakes, both FieldCore calling conventions and shared console without Arduino, ESP-IDF or private checkouts.

```sh
cmake -S test/native -B .pio/sen059x-native -DCMAKE_BUILD_TYPE=Release
cmake --build .pio/sen059x-native --config Release
ctest --test-dir .pio/sen059x-native -C Release --output-on-failure
```

Use failure-reporting tests that stay active in Release. Cover the six manufacturer exchanges, invalid frames, raw words, every config boundary by SKU, exception/health counters, interleaved devices, late arrivals, wraparound, lost FC06 acknowledgement and console framing. Stage 12 adds stdlib runner replay to existing Python discovery and records its standalone replay invocation.

## Arduino ESP32-S3

```sh
pio run -e esp32-s3-devkitc-1
pio run --project-dir examples/platformio-esp32 -e esp32-s3-devkitc-1
pio run --project-dir examples/01_basic_bringup_cli -e sen059x-cli-esp32s3
```

The first two build the existing read-only example through its root and nested configurations. The third builds the **stage 10** console against the public library. On Windows substitute `.\scripts\pio.cmd` for `pio`. Each board adapter owns the configured UART, line direction and console. Record target, framework, example and pin overrides.

## Native ESP-IDF ESP32-S3

Use SDK 5.5.5 at the baseline pin. First prepare an independent project from an actual PlatformIO package, then build it. The second project is the **stage 11** full console and RS485 application. Run both.

```sh
python scripts/check_package.py --prepare-idf .pio/idf-consumer
idf.py -C .pio/idf-consumer -B build-package set-target esp32s3 build
idf.py -C examples/espidf_basic -B build-cli set-target esp32s3 build
```

In a shell without an activated IDF on the project Windows machine, run the following SDK builds.

```powershell
C:\esp\idf.cmd 5.5.5 -C .pio/idf-consumer -B C:\esp\build\sen059x-package -D IDF_TARGET=esp32s3 build
C:\esp\idf.cmd 5.5.5 -C examples/espidf_basic -B C:\esp\build\sen059x-cli -D IDF_TARGET=esp32s3 build
```

Check the archive allow-list against every intended public header and source, and match the component's compiled sources to those actually exported. Stage 13 must link the installed codec and health API from an isolated consuming project. Passing the full console's SDK build alone cannot establish that the packaged component exports all necessary files.

## Recording outcomes

| Check | Evidence it supplies |
| --- | --- |
| Desktop fixture / generated CLI transcript | Repeatable inputs, output bytes, loss or cancellation behavior, ownership and passive observation. Record the fake backend and clock. |
| Archive / firmware build | Package contents and links for the actual installed component, framework, revision and target. Record exact commit and CI jobs. |
| Console handshake on a board | Identifies a running example and its transport context. Record firmware build, board and session. |
| Sensor read or configuration test | Identify SKU, device, serial settings, transceiver/echo, wire evidence, reference target and result. A power-cycle observation is a separate row. |
| Product integration | Stage 08 proves a source-pinned host fixture for each FieldCore revision. A real linked product profile and deployed hardware require distinct evidence. |

At stage 15 report each missing physical row, then list independently completed native, package and SDK checks.
