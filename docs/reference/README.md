# SEN059x device references

Use the DFRobot sources for the specific sensor model and the Modbus
Organization specifications for RTU framing, function codes and addressing.
Both the English and Chinese device manuals are available offline. The
Chinese manuals supply the complete baud-rate mapping for register `0x0201`.

| Question | Source |
| --- | --- |
| What voltage, pinout or measurement range does my sensor use? | [Technical reference in Czech](SEN059x_reference.md) and [DFRobot originals and drawings](source_links/ORIGINAL_SOURCES.md) |
| What does a register contain on each model? | [Register map](protocol/registers.csv) with addresses, units and per-model settings |
| What bytes should appear on the wire? | [Request and response vectors](protocol/modbus_vectors.csv) and [RTU timing and CRC notes](protocol/Modbus_RTU_notes.md) |
| Where did each file come from? | [Download manifest](source_links/sources.json) and [manufacturer and protocol source index](source_links/ORIGINAL_SOURCES.md) |
| Which other implementations mention these sensors? | [Registry survey and pinned community examples](source_links/registry-survey.md) |

The [Czech guide](README_CZ.md) accompanies the technical reference. The
[original project summary PDF](SEN059x_reference.pdf) predates the comparison
of the two language editions. For SEN0592 processed distance, the English
manual gives 190–750 ms and the Chinese manual gives 300–750 ms. For
subzero temperature encoding, the no-echo value and persistence after a
configuration write, confirmation still requires a physical unit.

## Verify and fetch

From the repository root, verify the 22 bundled snapshots offline with
`python scripts/fetch_originals.py --verify`. Their URLs, byte counts and
SHA-256 hashes are recorded in the manifest.

Run `python scripts/fetch_originals.py` to retrieve the eight supporting
files, including four Modbus libraries pinned to Git commits. These files are
cached locally under `originals/libraries/` and `originals/related/`.
Downloaded documents and source code retain the publishers’ terms.
