# 13 — Verify the public package through real consumers

## Inputs

Apply [COMMON](COMMON.md). Consume `consumer-fixture`, `arduino-example`, `idf-example` and `runner-evidence`. Follow [VERSIONS](VERSIONS.md), [VERIFICATION](VERIFICATION.md) and the [current IDF packaging notes](../IDF_PORT.md). Produce `verified-package`.

## Work

1. Extend the existing PlatformIO/Arduino and ESP-IDF metadata to install every intended header and portable source. Open the actual archive and compare paths with the export allow-list in the package checker. Register each compiled unit in the ESP-IDF component. Keep example owners, framework console adapters, bench runners, private source checkouts and reference originals outside the distributed core. Preserve the exported version header and current prerelease version unless a release has been requested.
2. Add a native C++17 compile/link consumer of the public headers in a clean build, then exercise the same public interface from the ESP-IDF fixture after installation from the actual archive. Include the umbrella and version, build a request, parse a known reply and call the passive health API. Make private include-directory or transitive SDK coupling visible by compiling each public header independently. Verify symbol definitions actually link.
3. Implement the framework matrix in VERSIONS and preserve the original entry points. Run the original probe and standalone Arduino console under SHZK's primary profile. In a separate isolated Core 6.1.19 job build the same S3 console with the consumer's exact pioarduino platform. Keep the primary Core 6.2.0 requirement and compatibility job's Core pin explicit. Run the native ESP-IDF application and independent packaged consumer with SDK 5.5.5. Exercise both the PlatformIO archive and the native component packer's output. Inspect the native payload after excluding packer-owned metadata and compile its installed public headers and sources in an isolated IDF fixture. Run the native fixture, console replay, reference/vector suites, promptset checker, version checks and Doxygen. Keep CI output specific to the failing target.
4. Bring README, Doxygen, examples, package usage, IDF port notes and changelog up to date with tested include paths, methods, pins and commands. Provide a minimal caller-owned TX/RX example, one human session, the runner invocation, meanings of raw/valid/fresh and the distinction between core, board adapter and product policy. Link the per-SKU source and evidence record.
5. Compare the archive's version fields with the source and both example build reports. Compile the public version contract, verify the prerelease string and numeric aliases and exercise known, dirty and unavailable Git provenance. An installed component may be inside the consumer's Git checkout. Verify library metadata and application identity are attributed to their own sources. Report detected framework/SDK and CLI schema separately from SEN059x SemVer. Repair any false `clean` or borrowed Git revision revealed by these tests.

## Acceptance

Preserve the package authors THYMOS Solution s.r.o. and Jan Havelka, the Jan Havelka maintainer entry, repository URL and MIT notice. Keep vendor originals under their recorded terms. Read the version from `library.json` and use the existing generator to synchronize the public header, Doxygen and IDF manifest. [Package metadata](../../library.json) · [License](../../LICENSE) · [Version generator](../../scripts/generate_version.py)

From a fresh build directory perform each applicable command in [VERIFICATION](VERIFICATION.md). Inspect the serialized archive, use the installed fixture outside the source include path, and check the native link on a desktop without either embedded framework. Verify generated API documentation resolves exported types. Record exact commit, SDK/tool versions, environment names, commands and job outcomes. A build-only result has no sensor observation.

## Delegation and self-audit

Apply the [stage audit](AUDIT.md). Give the reviewer the exported archive and public build instructions. Rebuild the independent consumer and compare resolved framework/package identities from both Arduino builds and native IDF with the version contract.

## Handoff

Record `verified-package` in [PROGRESS](PROGRESS.md) with archive inventories, public version values, both resolved PlatformIO dependency trees, native SDK version, compiler standard, framework/board identity and job links. The shipping app, FieldCore-compatible build, native SDK application and each archive consumer need distinct outcomes. Leave publication, tags and sibling profile updates as separate user decisions.
