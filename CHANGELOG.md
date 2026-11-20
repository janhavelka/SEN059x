# Changelog

Release notes follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- SEN0591 and SEN0592 reference tables for all 12 documented registers, including the eight baud-rate codes and model-specific range and angle settings. The reference records timing differences between the English and Chinese SEN0592 manuals.
- A USB–RS485 probe and an ESP32-S3 Arduino example for read-only bench use. Offline tests check the published Modbus frames, CRCs and request addresses.
- A source index with 22 checksummed documents and drawings, plus a fetcher for the eight supporting references.
- PlatformIO and ESP-IDF package metadata, a generated version header, and local checks for the development scaffold.
- Contributor, security and agent guidance for this repository and a description of FieldCore-node’s RS485 bus ownership.
- A shared PlatformIO entry point for the ESP32-S3 sketch, a Doxygen code reference and CI checks for the reference data, package and example build.

[Unreleased]: https://github.com/janhavelka/SEN059x/commits/main/
