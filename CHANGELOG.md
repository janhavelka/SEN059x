# Changelog

Release notes follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- An engineering reference for both models covering electrical limits, panel cutouts, measurement timing, configuration and RTU framing. It identifies where the SEN0592 timing, beam and mounting documents still admit more than one interpretation.
- A register map for all 12 documented addresses and six manufacturer request/reply pairs. The Chinese editions supply the complete baud-code table, the SEN0591 switch-threshold limits and the byte missing from the English address-write example.
- A USB–RS485 probe and an ESP32-S3 Arduino example for read-only bench use. Offline tests check the published Modbus frames, CRCs and request addresses.
- A source index with 22 checksummed documents and drawings, plus a fetcher for the eight supporting references.
- PlatformIO and ESP-IDF package metadata, a generated version header, and local checks for the development scaffold.
- Contributor, security and agent guidance for this repository and a description of FieldCore-node’s RS485 bus ownership.
- A shared PlatformIO entry point for the ESP32-S3 sketch, a Doxygen code reference and CI checks for the reference data, package and example build.

[Unreleased]: https://github.com/janhavelka/SEN059x/commits/main/
