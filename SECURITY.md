# Security policy

SEN059x is in pre-release development. Reports can cover the current source
tree, the serial probes, package tooling and reference downloads. Identify
the affected commit so the maintainer can inspect the same revision.

## Private reports

Send a report privately to Jan Havelka at [info@thymos.cz](mailto:info@thymos.cz).
Include the affected file or command, the impact and a minimal example of
the input or conditions that produced it. For an RS485 issue, include the
sensor model, serial settings and sanitized request and reply bytes when
available. Remove credentials and deployment-specific identifiers from logs.

## RS485 and downloaded sources

A valid Modbus CRC detects transmission errors. A device on the same bus
can generate a matching CRC, so deployments must control physical bus
access. The application owns the bus and associates each reply with an
outstanding request. This matters for FC03 reads because replies omit the
requested register address.

The reference archive records source URLs and SHA-256 digests for bundled
originals. To check a local copy, run `python scripts/fetch_originals.py --verify`
and include the source ID in reports about an unexpected result. See the
[reference manifest](docs/reference/source_links/sources.json).
