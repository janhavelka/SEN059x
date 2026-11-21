# Native ESP-IDF integration

The root component exports the portable C++17 API under
`<SEN059x/SEN059x.h>` and `SEN059x::VERSION` under `<SEN059x/Version.h>`.
It links the addressed FC03 codec, request context and diagnostics. FieldCore-node owns
UART, GPIO and timing on a shared RS485 bus.
[Component registration](../CMakeLists.txt) · [RS485 architecture](ARCHITECTURE.md)

## Build the package consumer

From the repository root, prepare a new or empty directory with
`python scripts/check_package.py --prepare-idf .pio/idf-consumer`.
The script checks the package export lists, creates an actual PlatformIO
archive, verifies its contents, and installs those files under
`components/sen059x/` in the fixture. Its `main` component declares
`REQUIRES sen059x` and includes both public entry headers. It builds the
published real-time distance request, validates its reply and checks the raw
word and status. [Fixture](../scripts/fixtures/idf_package_consumer/)

Build for ESP32-S3 with the side-by-side Windows ESP-IDF 5.5.5 installation.

```powershell
C:\esp\idf.cmd 5.5.5 -C .pio/idf-consumer -B C:\esp\build\sen059x-consumer -D IDF_TARGET=esp32s3 build
```

In an activated ESP-IDF 5.5.5 shell, the equivalent command is
`idf.py -C .pio/idf-consumer set-target esp32s3 build`. For a repeat build,
run the SDK build command against the prepared project. When the package has
changed, prepare a fresh fixture directory and build directory.

This checks component discovery, the installed include path and the firmware
link. The fixture performs no sensor I/O. Use the [standalone read examples](../examples/)
for bench work.
