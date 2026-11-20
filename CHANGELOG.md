# Changelog

Release notes follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- A 32-stage implementation promptset with source-pinned API, health, ownership and CLI conventions, both FieldCore-node consumer contracts, and independently resumable software, single-device and multidrop acceptance checks. Each stage identifies its entry check, owned work, independent review and evidence handoff.
- Eleven standalone audit checkpoints are interleaved with those stages. Each names the original range, calls for fresh parallel review of the completed code and diff, and records scoped corrections and rerun results.
- Executable prompt checks tie all 20 addressed operations to the public API and both full examples. The stages cover isolated vendor addressing and bounded multi-word diagnostics, specify both peer Arduino toolchains and native ESP-IDF, and require delegation, self-audit and regression evidence.
- An engineering reference for both models covering electrical limits, panel cutouts, measurement timing, configuration and RTU framing. It identifies where the SEN0592 timing, beam and mounting documents still admit more than one interpretation.
- A register map for all 12 documented addresses and six manufacturer request/reply pairs. The Chinese editions supply the complete baud-code table, the SEN0591 switch-threshold limits and the byte missing from the English address-write example.
- A USB–RS485 probe and an ESP32-S3 Arduino example for read-only bench use. Offline tests check the published Modbus frames, CRCs and request addresses.
- A source index with 30 bundled originals, including both DFRobot China specifications and the matching DYP family mechanical drawings. The fetcher also retrieves 12 supporting references.
- Offline comparisons against the four manufacturer editions cover the register inventory, settings, measurement times, all eight baud-rate timing rows and the six published request/reply pairs.
- PlatformIO and ESP-IDF package metadata, a generated version header, and local checks for the development scaffold.
- Contributor, security and agent guidance for this repository and a description of FieldCore-node’s RS485 bus ownership.
- A shared PlatformIO entry point for the ESP32-S3 sketch, a Doxygen code reference and CI checks for the reference data, package and example build.

### Fixed

- The reference now distinguishes SEN0591 general and per-level response times, calculates serialization times from frame lengths, and identifies the delays added by DFRobot's Arduino sketch.
- The bench probes check reply length and trailing bytes, validate Modbus exceptions and account for transmission time in their deadlines. The Python probe requires the actual model for a live read.

[Unreleased]: https://github.com/janhavelka/SEN059x/commits/main/
