# SEN059x

SEN059x is a C++ library in development for DFRobot’s SEN0591 and SEN0592
RS485 ultrasonic distance sensors. This checkout provides two read-only bench
examples, Modbus RTU test vectors and the original device documentation.
Version `0.0.0-dev.0` identifies the development package. Include
`<SEN059x/SEN059x.h>` for typed measurements, settings, response-time estimates,
raw FC03 reads and addressed FC06 reply validation, or `<SEN059x/Version.h>` for
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
codes. For a measurement, retain the `MeasurementRequest` from `buildMeasurementRead`
and pass it to `decodeMeasurementResponse` with the owner's completion time and
validity flag. A validated reading retains its raw word and quality even when
its engineering value is unavailable.

For per-device history, give each binding its own `MeasurementCache`. Call
`bind` with the selected model and address, then retain the `SampleRequest` from
`buildRead` until the owner can call `complete` with the reply or `discard` on
timeout. Use `snapshot(kind, nowMs, maxAgeMs)` to inspect the latest raw word
and last usable value without bus traffic. Each keeps its own completion time.
A known age equal to the limit is fresh. A successful rebind clears all four
register histories and retires the previous request.

Give each binding a `DeviceHealth` and call `beginAttempt` for each addressed
attempt actually sent. Retain the token with the request. Report a validated
device reply with `recordReply`, including an addressed exception or an
out-of-range word. Report one terminal communication failure for the attempt
with `recordFailure`. Foreign bytes leave the attempt open until a qualified
reply or the owner's deadline. With the default `offlineThreshold` of five,
the fifth consecutive failure sets `OFFLINE`. One validated reply restores
`READY`. Rebind the health, measurement cache and configuration records
together when the owner selects a new binding. [Device health API](include/SEN059x/DeviceHealth.h)

Keep a separate `Stats` for the endpoint or bus and interval you want to report.
Call `recordAttemptError` with the underlying error on each failed attempt,
`recordRetry` when an extra attempt is sent and `recordOperation` once with
the final logical result. A timeout followed by a successful retry counts as
one timeout, one retry and one successful operation. An addressed exception
counts as a device reply and an attempt error, and a terminal exception counts
as a failed operation. [Owner statistics](include/SEN059x/Stats.h)

Use `buildSettingRead` and `decodeSettingResponse` to read any configuration word.
The result keeps the original word and reports whether the model documents it.
`settingDescriptor` gives the register's unit, bounds, output mode and factory
word when specified. `buildWriteSingleRegister` checks the supplied unit and
word before encoding. Use `valueToBaud` to convert bits per second to a baud
code. The separate `buildVendorBroadcastWrite` requires the explicit
`DFRobotIsolated` policy and emits a send-only frame to `0xFF`.

For commissioning, keep one `Configuration` per physical binding. Use
`buildRead` and `completeRead` to record an observed word, its endpoint and
the owner's completion time. `buildWrite` prepares a checked FC06. Call
`recordTx` as soon as transmission becomes possible. From then on,
`snapshot(setting)` marks the previous word as historical. An address or
baud write also clears endpoint trust. Record the ACK or its loss, then use
`buildVerification` for an independent FC03 and supply the actual address and
bit rate to `completeRead`. The operation retains the requested word, ACK
evidence, readback and cleanup result. After a vendor send, an explicit
`buildRead` can refresh the observation while preserving the local TX result.
Call `interrupt` on a known power interruption and read again to establish
current values. [Commissioning API](include/SEN059x/Configuration.h)

An addressed FC06 reply must echo the requested register and value. Local
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
run the C++17 tests with CMake and CTest. The same build produces
`human_terminal` (`human_terminal.exe` on Windows). Run it from the native
build output directory and type `?` for help. The desktop terminal simulates
one sensor starting at address 1 and 115200 baud. Its configuration lasts
for the life of the process.
Select `model SEN0591` or `model SEN0592`, then try `read realtime`,
`config read baud` or `readregs 0x0100 3`. A different address lets you
exercise a timeout. During a finite `watch temperature 5 1000`, use
`job status`, `latest temperature` or `stop`. Use `result` to see the
retained outcome. Run `test_human_live --transcript` from the same build
directory to replay both models, including exception and cancellation cases.

For a desktop commissioning example, enter the following commands in order.
Wait for each accepted operation to finish. The preview shows the register
word and endpoints. The first read establishes the addressed endpoint, and
the confirmed write uses one FC06 followed by a same-register FC03.

```text
model SEN0591
config read threshold
config set threshold 750
maintenance on isolated
config set threshold 750 confirm
settings
```

To try the vendor form in the simulator, select `profile dfrobot-isolated`,
then run `config read threshold` to establish the new binding. Run
`maintenance on isolated` and `broadcast set threshold 800 confirm`.
The terminal reports one transfer and `verified=false`. Record its operation
ID, run `config read threshold`, then replay `result <id>` to inspect the
original send. For address or baud changes, the preview shows where to
direct that later read. `help config set <key>` lists units and model limits.
`test_human_commissioning --transcript` replays all eight settings and the
failure cases on both simulated models. Physical output effects, the baud
used for a baud-change ACK and power-cycle persistence require bench evidence.

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
