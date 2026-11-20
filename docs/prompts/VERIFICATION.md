# Verification commands and evidence

Run the checks for the current stage in a clean build directory. Record commit, target, tool version and result. The **baseline** commands work in the scaffold. Commands marked **implementation** become acceptance checks in the named stages. Use the [sibling version and framework matrix](VERSIONS.md) for all implementation build claims. [Current workflow](../../.github/workflows/ci.yml) · [ESP-IDF package fixture](../IDF_PORT.md)

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

## Native C++ and consumer fixtures

Establish a desktop harness under `test/native/` in the [public value and status stage](02-value-status-contract.md), extend the same target through each portable stage and register the tests with CTest. The root CMake file registers the ESP-IDF component. Build the portable core, owner fakes, both FieldCore calling conventions and shared console without Arduino, ESP-IDF or private checkouts.

```sh
cmake -S test/native -B .pio/sen059x-native -DCMAKE_BUILD_TYPE=Release
cmake --build .pio/sen059x-native --config Release
ctest --test-dir .pio/sen059x-native -C Release --output-on-failure
```

Use failure-reporting tests that stay active in Release. Cover the six manufacturer exchanges, invalid frames, raw words, every config boundary by SKU, exception/health counters, interleaved devices, late arrivals, wraparound, lost FC06 acknowledgement and console framing. The [host client](23-runner-client.md) and [fault campaign](24-runner-faults.md) add stdlib runner replay to existing Python discovery and records its standalone replay invocation.

## Arduino ESP32-S3

For the [Arduino port](19-arduino-port.md) and [CI matrix](27-build-ci.md), run the following in the primary SHZK-compatible environment with Core 6.2.0, official platform 8.0.0 and Arduino 3.3.12. During a scaffold-only stage the first two builds use the current repository requirements. Record the resolved versions for that stage.

```sh
pio --version
pio run -e esp32-s3-devkitc-1
pio run --project-dir examples/platformio-esp32 -e esp32-s3-devkitc-1
pio run --project-dir examples/01_basic_bringup_cli -e sen059x-cli-esp32s3
pio pkg list --project-dir examples/01_basic_bringup_cli -e sen059x-cli-esp32s3
```

The first two firmware builds exercise the existing read-only example through its root and nested configurations. The third builds the primary full console against the public library. Build the same console again in an independent FieldCore-compatible environment with Core 6.1.19, platform 55.03.311 and Arduino 3.3.11.

```sh
pio --version
pio run --project-dir examples/01_basic_bringup_cli -e sen059x-cli-fieldcore-esp32s3
pio pkg list --project-dir examples/01_basic_bringup_cli -e sen059x-cli-fieldcore-esp32s3
```

Use an isolated PlatformIO Core/package directory per profile to prevent resolver or compiler caches leaking between jobs. On Windows use `.\scripts\pio.cmd` when it supplies the required Core version. Otherwise use the executable from a project-local virtual environment with the required Core installed. Each board adapter owns the configured UART, line direction and console. Record target, framework, example and pin overrides.

## Native ESP-IDF ESP32-S3

Use SDK 5.5.5 at the baseline pin. First prepare an independent project from an actual PlatformIO package, then build it. The second project is the [native full console](22-esp-idf-example.md) and RS485 application. Run both.

```sh
idf.py --version
python scripts/check_package.py --prepare-idf .pio/idf-consumer
idf.py -C .pio/idf-consumer -B build-package set-target esp32s3 build
idf.py -C examples/espidf_basic -B build-cli set-target esp32s3 build
```

In a shell without an activated IDF on the project Windows machine, run the following SDK builds.

```powershell
C:\esp\idf.cmd 5.5.5 -C .pio/idf-consumer -B C:\esp\build\sen059x-package -D IDF_TARGET=esp32s3 build
C:\esp\idf.cmd 5.5.5 -C examples/espidf_basic -B C:\esp\build\sen059x-cli -D IDF_TARGET=esp32s3 build
```

Check the archive allow-list against every intended public header and source, and match the component's compiled sources to those actually exported. The [archive install stage](25-package-exports.md) must link the installed codec and health API from an isolated consuming project. Passing the full console's SDK build alone cannot establish that the packaged component exports all necessary files.

In [archive packaging](25-package-exports.md), add `--archive <path>` to the existing package checker so it verifies the supplied archive and prepares the fixture from those exact bytes. Keep the existing PlatformIO packing invocation usable. In an activated IDF 5.5.5 shell, use a fresh destination for each native pack and fixture.

```sh
compote version
compote component pack --name sen059x --project-dir . --dest-dir .pio/idf-pack
python scripts/check_package.py --archive <emitted-archive-path> --prepare-idf .pio/idf-native-package
idf.py -C .pio/idf-native-package -B build-native-package set-target esp32s3 build
```

Substitute the archive path printed by the packer. Check any metadata added by the packer explicitly. Compare every shipped source, public header, version and license with the expected bytes before using that installation. [SHZK package and installed-consumer jobs](https://github.com/janhavelka/SHZK-PT/blob/59d76c502a90bef8038ced2b72a2d50df98184e9/.github/workflows/ci.yml)

## Recording outcomes

| Check | Evidence it supplies |
| --- | --- |
| Desktop fixture / generated CLI transcript | Repeatable inputs, output bytes, loss or cancellation behavior, ownership and passive observation. Record the fake backend and clock. |
| Archive / firmware build | Package contents and links for the actual installed component, framework, revision and target. Record exact commit and CI jobs. |
| Console handshake on a board | Identifies a running example and its transport context. Record firmware build, board and session. |
| Sensor read or configuration test | Identify SKU, device, serial settings, transceiver/echo, wire evidence, reference target and result. A power-cycle observation is a separate row. |
| Product integration | The [FieldCore fixtures](14-fieldcore-revisions.md) prove a source-pinned host fixture for each FieldCore revision. A real linked product profile and deployed hardware require distinct evidence. |

In [delivery review](32-delivery-review.md), report each missing physical row, then list independently completed native, package and SDK checks.
