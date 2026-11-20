# Contributing to SEN059x

SEN059x is in development for the DFRobot SEN0591 and SEN0592. Contributions
can already improve the protocol references, captured frames, packaging and
read-only bench examples. The [README](README.md) describes the current scope.

## Protocol corrections

Identify the model, register or function and the value being corrected.
For a documentation finding, link the exact manufacturer table and identify
the language or revision. For a bench observation, include the model, serial
format, slave address, request and reply bytes, and relevant timing. A tested
CRC establishes frame consistency. Physical observations need the setup and
conditions that produced the reading.

Update the [register map](docs/reference/protocol/registers.csv) or
[reference frames](docs/reference/protocol/modbus_vectors.csv) together with
the affected example or test. Keep manufacturer originals intact. Record
disagreements in the [manufacturer cross-check](docs/reference/README.md#manufacturer-cross-check) and link to the relevant source editions.
Third-party documents and source archives retain their own copyright and
license terms. The [MIT license](LICENSE) covers repository-authored software
and documentation.

## Development checks

Run the following commands from the repository root when changing the
references, frame handling or probe. The offline checks use the Python
standard library.

```sh
python -m unittest discover -s test -v
python examples/python/sen059x_probe.py --self-test
python scripts/fetch_originals.py --verify
git diff --check
```

For ESP32-S3 example changes, build with `pio run`. On Windows use the
managed installation through `.\scripts\pio.cmd run`. For packaging and
documentation changes, follow the checks in the [README](README.md).

For a version change, use `python scripts/generate_version.py set <version>`
or edit `library.json` and run `python scripts/generate_version.py sync`.
The generator updates `include/SEN059x/Version.h`, `idf_component.yml` and
`Doxyfile`. Its `check` command reports drift without writing files.
Keep the two manifests' export lists in step when adding a public header.
Run the [package consumer build](docs/IDF_PORT.md) after changing component
registration or exported headers.

Keep public C++ interfaces in `include/SEN059x/`, with units, buffer lengths
and result semantics documented at the declaration. Place serial I/O, GPIOs
and retry policy in examples or in the consuming firmware. See the
[architecture notes](docs/ARCHITECTURE.md).

## Pull requests

1. Make a focused change on a branch in your fork.
2. Update affected documentation and add user-visible changes under
   `[Unreleased]` in the [changelog](CHANGELOG.md).
3. Use a Conventional Commit subject such as `docs: clarify SEN0592 response timing`
   or `test: cover a truncated FC03 reply`.
4. Open a pull request summarizing the change, the sources or captures it
   uses, and the checks you ran. Include the hardware setup if you tested a
   physical sensor.

For private security reports, follow the [security policy](SECURITY.md).
