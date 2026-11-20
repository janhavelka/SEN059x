# SEN0591 and SEN0592 engineering reference

| Look up | Use |
| --- | --- |
| Model selection, power, four-wire pinout, beam coverage, dimensions and panel cutouts | [Hardware and installation](hardware.md) |
| Measurement modes, conversion times, settings and their interactions | [Measurement and configuration](measurement-and-configuration.md) |
| Exact register address, access, unit, value and per-model limits | [Register map, 12 addresses](protocol/registers.csv) |
| Serial format, FC03 and FC06 transactions, exceptions, CRC and bus timing | [Modbus RTU wire guide](protocol/Modbus_RTU_notes.md) |
| Request and reply bytes with decoded values | [Manufacturer frame examples](protocol/modbus_vectors.csv) |
| Original documents, drawings, example sketches and licensed source archives | [Source inventory](source_links/ORIGINAL_SOURCES.md) |
| Distributor discrepancies, package searches and third-party sensor implementations | [Registry survey](source_links/registry-survey.md) |

For the Czech introduction, see [Český průvodce](SEN059x_reference.md).
For application ownership and the proposed library interface, see the
[FieldCore-node integration notes](../ARCHITECTURE.md).

## Manufacturer cross-check

Checked on 20 November 2026. Both DFRobot English protocol pages, both Chinese
manuals, both English Arduino sketches and all nine drawings/wiring images
matched the archived files byte for byte. Both overview pages had the same
specification and pinout table rows as their snapshots, and both storefronts
had the same visible text. The 22 archived originals also include three
Modbus Organization PDFs. [Source inventory](source_links/ORIGINAL_SOURCES.md) · [URLs and SHA-256 digests](source_links/sources.json)

| Question raised by the sources | Finding and where to find the evidence |
| --- | --- |
| Which baud codes and SEN0591 switching threshold were lost in the English tables? | The Chinese register rows give the complete values. The packet-duration table numbers its rows independently of the baud codes. [Register map](protocol/registers.csv) · [Configuration notes](measurement-and-configuration.md#configuration-on-the-rs485-models) |
| Which function and how many bytes change the address? | The write examples establish FC06. The Chinese example supplies the byte omitted by the English edition and its CRC checks out. [Wire guide](protocol/Modbus_RTU_notes.md#published-examples-and-corrections) · [Frame vectors](protocol/modbus_vectors.csv) |
| Which addressing and no-parity conventions apply? | DFRobot and the serial-line standard specify different broadcast and framing conventions. The wire guide gives the sensor settings and the interoperable unicast range. [Device and standard comparison](protocol/Modbus_RTU_notes.md#serial-settings-and-addressing) |
| Does the highest SEN0591 range setting extend the rated range? | The manuals give an approximate configuration distance above the storefront rating. The product rating remains the figure for specifying an installation. [Range settings](measurement-and-configuration.md) |
| Which SEN0592 response times and beam angles apply? | The timing tables agree on the upper bounds and disagree on some lower bounds. The beam specifications also differ by source and by axis. The model guide preserves the published values together. [Measurement and configuration](measurement-and-configuration.md) |
| Which outline, cutout and environmental limits should be used? | The hardware guide distinguishes the two SEN0592 drawing options and quotes operating and storage conditions separately. It identifies the information needed to match the drawings to a unit. [Hardware](hardware.md) · [Distributor comparison](source_links/registry-survey.md) |

The reviewed documents leave the no-echo return value, subzero temperature
encoding, write persistence and baud-change acknowledgement speed open. A
capture for the specific unit or a further manufacturer specification is needed
before treating any of these as a device contract. [Measurement and configuration](measurement-and-configuration.md)

## Offline use

From the repository root, run `python scripts/fetch_originals.py --verify` to
check all bundled snapshots. Run `python scripts/fetch_originals.py` to fetch
the eight additional software and related-device references into the local
cache. The [supplied four-page summary PDF](SEN059x_reference.pdf) records the
project’s initial research and predates this cross-check.
