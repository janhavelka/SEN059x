# SEN059x repository conventions

These instructions cover the repository. Claude Code imports this file through
[CLAUDE.md](CLAUDE.md).

## Project and evidence

- Follow the current request in this staged project. The package foundation,
  device references and stand-alone probes are usable now. The C++ sensor
  codec is planned. Check the [README](README.md) for the current entry points.
  For implementation prompts use the [stage index](docs/prompts/README.md)
  and its [progress ledger](docs/prompts/PROGRESS.md). When executing a
  numbered stage, spawn subagents for scoped work and independent review,
  follow its [self-audit](docs/prompts/AUDIT.md) and rerun checks after fixing findings.
- Select SEN0591 (3 m) or SEN0592 (5 m) explicitly. Both devices respond to
  the same measurement register addresses, so a successful read cannot
  identify the SKU.
- For changes to units, defaults, timing or frame bytes, identify the SKU,
  register and source table. Keep conflicting manufacturer values and their
  language or revision visible. Preserve a missing-echo code, subzero
  temperature encoding or configuration persistence as an open question
  until a source or a physical observation establishes it. See the
  [register map](docs/reference/protocol/registers.csv) and
  [manufacturer source index](docs/reference/source_links/ORIGINAL_SOURCES.md).
- Keep original documents byte-for-byte and record provenance in the
  [source manifest](docs/reference/source_links/sources.json). Put corrections
  in the technical reference and CSV tables. Keep reference archives, board
  settings and bench transcripts outside the published library.

## C++ and RS485

- Put self-contained public headers in `include/SEN059x/` and portable
  implementation in `src/`. Document public arguments, units, buffer
  capacities and error results with Doxygen. Keep dependencies on Arduino
  and ESP-IDF in example or application code.
- Use bounded operations on caller-supplied buffers. Prefer `constexpr`
  constants and simple functions. Keep heap allocation, logging, sleeps and
  background work out of the codec.
- For new public C++ declarations, use `PascalCase` types, `camelCase`
  functions and parameters, and `CAPS_CASE` protocol constants. Use the
  checked-in [clang-format configuration](.clang-format).
- Follow the FieldCore-node integration model. The application owns UART
  configuration, DE/RE, elapsed time, response deadlines and retries. It
  schedules one outstanding request per bus and retains the expected address
  and register until the response has been handled. See the
  [RS485 ownership notes](docs/ARCHITECTURE.md).
- For FC03 and FC06, check the expected address, function, exact frame length
  and CRC. Check FC03 byte counts, FC06 echoed register/value, and exception
  frames before publishing a reading or confirming a write. Register words
  use high byte first and the Modbus RTU CRC uses low byte first. Use the
  [reference frames](docs/reference/protocol/modbus_vectors.csv).
- Keep address and baud changes explicit. The supplied probes use unicast
  reads. Reading the distance registers starts a measurement, so account for
  the selected measurement mode when setting the application deadline.

## Build, package and verification

- Use `library.json` as the version source. Use the repository version
  generator to synchronize the component manifest, Doxygen project number
  and generated `include/SEN059x/Version.h`. Keep unreleased changes in the
  [changelog](CHANGELOG.md).
- On Windows, `scripts\pio.cmd` selects the existing VS Code-managed
  PlatformIO installation. For native ESP-IDF work, use the project's SDK
  version in a side-by-side installation through `C:\esp\idf.cmd`.
  Keep IDF environment changes local to the build shell.
- Read the relevant workflow and use its commands and pinned tool versions.
  Run the [local checks](README.md) appropriate to the change. For reference
  or frame changes, run the offline Python tests, probe self-test and
  snapshot verification. Build the ESP32-S3 example after edits to its
  sketch, pins or PlatformIO configuration.
- Use focused local checks for small edits. When remote checks are in scope,
  verify substantial API, build, dependency or integration changes in CI on
  the exact final commit. Report any failed or unrun checks. Distinguish
  published examples, offline tests, compile checks and physical observations.

## Git workflow

- Inspect the working tree, branch and upstream before editing. Check for
  incoming changes and fast-forward when safe. Keep a sequence of prompts
  on the existing branch unless the user requests a different workflow.
- Preserve unrelated and in-progress work. Limit commits to the files or
  hunks for the completed change. When agents share a checkout, the lead
  integrates their work and owns Git operations.
- Follow the user's Git scope for each task. Local-only work ends with local
  changes and checks. When upstream delivery is authorized, use Conventional
  Commits, push each verified chunk, and confirm the upstream commit. Report
  conflicts or synchronization failures.
- Use sibling repositories as read-only sources for conventions and consumer
  contracts. Keep integration work in the repository the user requested.
