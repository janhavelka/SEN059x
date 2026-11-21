# SEN059x

SEN059x is a C++ library in development for DFRobot’s SEN0591 and SEN0592
RS485 ultrasonic distance sensors. This checkout provides two read-only bench
examples, Modbus RTU test vectors and the original device documentation.
Version `0.0.0-dev.0` identifies the development package. Include
`<SEN059x/SEN059x.h>` for model and register selectors, the addressed FC03 codec,
addressed FC06 reply validation and diagnostics, or `<SEN059x/Version.h>` for
version information.
Use the repository checkout for the examples, tests and device references.
[Source repository](https://github.com/janhavelka/SEN059x) · [Changelog](CHANGELOG.md)

## Devices and protocol

| DFRobot model | Rated measurement range | Supply | Factory serial settings |
| --- | --- | --- | --- |
| SEN0591 | 20–3000 mm | 3.3–12 V | Address 1, 115200 baud, 8N1 |
| SEN0592 | 30–5000 mm | 3.3–24 V | Address 1, 115200 baud, 8N1 |

Reading one holding register at `0x0101` with Modbus function 03 starts a
real-time distance measurement and returns millimetres. Function 06 writes
configuration registers. Range levels, beam angles and switch thresholds
depend on the model. [Register map](docs/reference/protocol/registers.csv) · [Measurement and configuration](docs/reference/measurement-and-configuration.md) · [Example frames](docs/reference/protocol/modbus_vectors.csv)

## Read a sensor

Power one sensor within its rated supply range. Red is VCC, black is GND,
white is RS485 A and yellow is RS485 B. Connect A and B to a USB–RS485
adapter and allow one second after power-up. From the repository root, use
the sensor’s actual model, address, baud rate and adapter port. [Hardware and installation](docs/reference/hardware.md)

```sh
python -m pip install pyserial
python examples/python/sen059x_probe.py --port COM16 --model SEN0592 --address 1 --baud 115200 --mode realtime
```

The probe prints the register value and elapsed time. `--mode all` reads
processed distance, real-time distance, temperature and echo time in sequence.
`--model` labels the output. The model comes from the part marking or the
installation record. [Python probe](examples/python/README.md)

The ESP32-S3 Arduino example polls `0x0101` through a half-duplex RS485
transceiver. Set the RX, TX and DE/RE pins for the board before running it on
hardware. [ESP32-S3 wiring and build](examples/platformio-esp32/README.md)

## Shared RS485 bus

Use a distinct unicast address per sensor and one outstanding request per
bus. A one-word FC03 response carries the address, function and data, but
omits the register number. The bus owner associates each reply with its
request and handles late replies after a timeout. Allow for the selected
measurement’s response time as well as transmission. [RTU timing](docs/reference/protocol/Modbus_RTU_notes.md#wire-time-and-measurement-time)

The portable core builds FC03 requests and validates replies using caller-owned
buffers. It returns raw register words and preserves validated Modbus exception
codes. An addressed FC06 reply must echo the requested register and value. Local
transmit echo can pass the same byte checks. The bus owner qualifies the origin
of the reply, and a separate addressed readback can establish the setting.
FieldCore-node owns the UART, DE/RE timing, scheduling and retries in the intended
integration. [FieldCore-node architecture notes](docs/ARCHITECTURE.md)

The implementation promptset takes the library from the current scaffold
through the portable codec, shared-bus consumer, Arduino and native ESP-IDF
consoles, package checks and qualification of each SKU. Both full examples
must expose all four measurements and read/write access to all eight settings.
The sequence interleaves 32 original stages with 11 standalone audit
checkpoints. Each checkpoint names the original prompts to reread and the
implementation evidence to verify. Its handoff records confirmed findings,
fixes and final verification results.
[Start the first prompt](docs/prompts/START_HERE.md) · [Operation cases](docs/prompts/OPERATIONS.csv) · [Build profiles and versions](docs/prompts/VERSIONS.md) · [Implementation progress](docs/prompts/PROGRESS.md)

## Build and verify

Run the offline reference checks from the repository root with Python.

```sh
python -m unittest discover -s test -v
python examples/python/sen059x_probe.py --self-test
python scripts/fetch_originals.py --verify
python scripts/generate_version.py check
```

To build the ESP32-S3 example with the managed PlatformIO installation on
Windows, run `.\scripts\pio.cmd run`. With PlatformIO on your path, run
`pio run -e esp32-s3-devkitc-1`. The package check uses the same PlatformIO
installation and PyYAML. On Windows, install `PyYAML==6.0.3` in the Python
environment that runs the check. For a fresh Linux CI environment, install
the [pinned tools](requirements-ci.txt) with `python -m pip install -r requirements-ci.txt`.

```sh
python scripts/check_package.py
```

The ESP-IDF package check builds an independent ESP32-S3 application against
the installed portable API. See the [ESP-IDF build instructions](docs/IDF_PORT.md).
The [desktop build commands](docs/prompts/VERIFICATION.md#native-c-and-consumer-fixtures)
run the C++17 tests with CMake and CTest.

To render the C++ code reference locally, run `doxygen Doxyfile` and
open `docs/doxygen/html/index.html`.

## References and contributing

The reference guide indexes the original DFRobot pages, mounting drawings,
Modbus specifications and unresolved hardware questions. Each bundled original
has a SHA-256 checksum. The source survey also covers
distributor listings and community code. [Engineering reference](docs/reference/README.md) · [Český průvodce](docs/reference/SEN059x_reference.md) · [Source inventory](docs/reference/source_links/ORIGINAL_SOURCES.md) · [Registry survey](docs/reference/source_links/registry-survey.md)

See [Open questions](docs/open-questions.md) for bench scope, commissioning,
measurement scheduling and device behavior still waiting for evidence.

Copyright © 2026 THYMOS Solution s.r.o., Jan Havelka. Repository-authored
material uses the MIT license. Vendor documents and
third-party libraries retain their respective copyrights and terms.
[License](LICENSE) · [Contributing](CONTRIBUTING.md) · [Agent instructions](AGENTS.md) · [Security reports](SECURITY.md)
