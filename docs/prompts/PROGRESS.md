# Implementation progress

The audited scaffold exports `SEN059x::VERSION` and contains independent read-only probes. It has reference tests and Arduino and IDF package builds. All 32 original stages and 11 independent checkpoints below are planned. The promptset and its checks are preparation evidence. [Audited scaffold](https://github.com/janhavelka/SEN059x/commit/be99469b0d5b701c7649a47083c92942c2fcca3d) · [Stage index](README.md)

| Stage | Output | Status |
| ---: | --- | --- |
| 01 | `baseline` | In progress |
| 02 | `value-status` | Planned |
| 03 | `read-codec` | Planned |
| 04 | `rtu-codec` | Planned |
| 04A | `codec-audit` | Planned |
| 05 | `measurement-values` | Planned |
| 06 | `measurements` | Planned |
| 07 | `setting-access` | Planned |
| 08 | `configuration` | Planned |
| 09 | `device-health` | Planned |
| 09A | `device-audit` | Planned |
| 10 | `owner-exchange` | Planned |
| 11 | `owner-scheduler` | Planned |
| 12 | `owner-contract` | Planned |
| 12A | `owner-audit` | Planned |
| 13 | `consumer-main` | Planned |
| 14 | `consumer-fixture` | Planned |
| 14A | `consumer-audit` | Planned |
| 15 | `cli-console` | Planned |
| 16 | `cli-live` | Planned |
| 17 | `cli-commissioning` | Planned |
| 18 | `shared-cli` | Planned |
| 18A | `cli-audit` | Planned |
| 19 | `arduino-primary` | Planned |
| 20 | `arduino-example` | Planned |
| 20A | `arduino-audit` | Planned |
| 21 | `idf-port` | Planned |
| 22 | `idf-example` | Planned |
| 22A | `idf-audit` | Planned |
| 23 | `runner-client` | Planned |
| 24 | `runner-evidence` | Planned |
| 24A | `runner-audit` | Planned |
| 25 | `package-exports` | Planned |
| 26 | `version-provenance` | Planned |
| 27 | `verified-package` | Planned |
| 27A | `package-audit` | Planned |
| 28 | `bench-measurements` | Planned |
| 29 | `bench-commissioning` | Planned |
| 30 | `bench-shared-bus` | Planned |
| 30A | `bench-audit` | Planned |
| 31 | `operator-docs` | Planned |
| 32 | `delivery-review` | Planned |
| 32A | `delivery-audit` | Planned |

## Stage evidence

When starting or resuming, open the producing artifacts at their recorded revisions and rerun the smallest check that proves the prerequisite. For each original stage capture the initial base SHA and pre-existing edits, then the completed revision or relevant commits and paths. At a standalone checkpoint use that history to reconstruct the range and inspect it again on the current tree. If evidence is partial, identify precisely which required behavior can be exercised.

Use **Planned**, **In progress**, **Partial**, **Blocked** or **Complete** for each execution. Record grouped results by SKU and surface. Physical campaigns identify the fixture and any missing observations. The bench-evidence checkpoint can finish reviewing the available evidence while the affected physical rows stay open. When a later correction changes a previously audited claim, identify the previous checkpoint result, reopen its affected checks and record the new tested revision. [Fresh range audit](AUDIT.md#fresh-range-audit) · [Stage handoff](AUDIT.md#record-the-handoff)

Use this short entry for an original stage or checkpoint. Record commands and measured outcomes in the same evidence entry or linked immutable run. A checkpoint with no confirmed findings still records the original range and verification performed.

```text
Stage or checkpoint, output, status:
Original prompt(s) reread and predecessor artifacts checked:
Base SHA, covered commit(s), current/tested SHA and local changes examined:
Entry points, files and tests inspected:
Parallel assignments and lead-confirmed findings:
Scoped fixes or no confirmed findings:
Final verification commands, versions, results and evidence type:
Prior result superseded / affected handoffs rechecked:
Open rows by SKU / fixture / surface:
Next ready handoff or exact resume action:
```

### 01 — Two-model baseline, phase a

Base and tested source revision `10cc4107e217d6fbe14feb1dc5834d04c8353f72`. `main` matched the local `origin/main` and the wrapper's dispatch record. The index and worktree reported no edits. This was the first live run, so there was no earlier wrapper commit receipt. Stage 01 has no predecessor. Its output is the reproducible baseline and any verified reference or coverage corrections. [Original prompt](01-baseline-and-coverage.md) · [Scaffold](../../README.md)

**Package and probes.** `library.json` declares `0.0.0-dev.0`. The PlatformIO and IDF allowlists name `library.json`, `idf_component.yml`, `CMakeLists.txt`, `include/SEN059x/Version.h`, `LICENSE`, `README.md` and `CHANGELOG.md`. The generated header exports `SEN059x::VERSION` and the other version fields. The IDF registration exports `include/` and `src/` contains only its placeholder. The Python probe requires an explicit SKU for live runs and reads any of the four measurement words. The Arduino/PlatformIO probe reads `0x0101` at address 1 on a dedicated link. Both supply their own framing. The version-header consumer for stage 02 is `scripts/fixtures/idf_package_consumer/main/main.cpp`, which compiled as a C++17 translation unit with MinGW GCC 15.1.0. [Package manifest](../../library.json) · [IDF consumer](../../scripts/fixtures/idf_package_consumer/main/main.cpp) · [Python probe](../../examples/python/sen059x_probe.py) · [Arduino probe](../../examples/platformio-esp32/src/main.cpp)

**Register and frame evidence.** A direct `Counter` comparison of `(register, function)` pairs from `OPERATIONS.csv` and the visible HTML rows read by `test/test_register_reference.py::snapshot_rows` found 12 FC03 reads and 8 FC06 writes for SEN0591 EN, SEN0591 CN, SEN0592 EN and SEN0592 CN separately. The six matrix case IDs match the vendor CSV's function, register and returned (FC03) or written (FC06) word. `test_all_six_pairs_match_each_manual` checked all request/reply bytes in each edition, restoring the omitted `00` in EN example 4 as documented. The FC06 examples, including the write layouts mislabeled `03`, are reference evidence only. Both SKUs can produce the same valid address-1 FC03 response, whose fields carry neither the SKU nor the originating register. Use the part marking or installation record. [Operation matrix](OPERATIONS.csv) · [Register map](../reference/protocol/registers.csv) · [Manufacturer exchanges](../reference/protocol/modbus_vectors.csv) · [Frame corrections](../reference/protocol/Modbus_RTU_notes.md#published-examples-and-corrections)

**Implementation obligations.** Both full examples and the portable API cover four measurement reads and eight setting reads/writes per SKU, including polarity, threshold and UART units. Keep fixed Modbus units regardless of `0x0209`. Standard unicast uses 1–247 and the isolated profile admits 1–254. The separately checked vendor FC06 to `0xFF` requires explicit isolation and any setting verification comes from a subsequent addressed read. The contiguous FC03 diagnostic uses a separate bounded raw result, rejects holes and preserves typed caches. The longest documented contiguous span is three words from `0x0100`. The manuals illustrate single-word reads. [Public conventions](CONVENTIONS.md) · [Coverage](COVERAGE.md) · [Human and machine dispatch](CLI_CONTRACT.md)

**Device limits and open observations.** Both SKUs specify startup up to 1000 ms, operating current up to 10 mA and factory address 1 at 115200 baud, 8N1. SEN0591 is rated 3.3–12 V and 20–3000 mm. Its nominal range level 4 is about 3500 mm. SEN0592 is rated 3.3–24 V and 30–5000 mm. Processed, real-time and echo reads start ranging. The temperature sampling instant is unspecified. Preserve the SEN0592 table 1 EN/CN minima (190/300 ms for processed and 5/40 ms for temperature and echo) and the common register-specific maxima. Size owner deadlines with the broader register estimates, actual serialization, turnaround and frame closure. At factory baud the 8-byte request, 7-byte read reply and 5-byte exception serialize in 0.694, 0.608 and 0.434 ms respectively. For each SKU retain raw words for missing echo and subzero temperature. A known-target and known-temperature capture is needed. The four-wire diagrams and tables give no mode-selection path for checking UART or switch effects. Verify their register access separately. The acknowledgement baud and power-cycle persistence need isolated readback and restoration evidence on each SKU. Contiguous-read acceptance and acoustic interaction also remain open on both SKUs. This run had no identified or authorized physical fixture. [Hardware ratings](../reference/hardware.md) · [Response times and conditional settings](../reference/measurement-and-configuration.md) · [Wire timing](../reference/protocol/Modbus_RTU_notes.md#wire-time-and-measurement-time)

**Peer contracts and versions.** Inspected pinned sibling Git objects and callers. SHZK-PT `59d76c5` and product pin `361c47c` declare `buildReadRegisters`, `validateReadResponseExpected`, `buildWriteSingleRegister` and `parseWriteSingleRegister`. The SHZK CLI supplies caller storage and retained write identity. FieldCore's `ShzkDeviceModule.cpp` uses the expected address/count validator on both pins. TMP1x2 `afd5088` exposes `state()`, `isOnline()`, `healthSnapshot()` and `resetStatistics()`. Its CLI displays state and online status and calls the reset. FieldCore `eccdbdc` provides 8-byte TX and 192-byte RX with fixed-length or suffix completion. `c40ea83` provides 21-byte TX and 192-byte RX, adds RTU completion and evidence qualification. Keep deadlines, one outstanding exchange, serial setup and echo qualification with the owner. Both product revisions pin Core 6.1.19 in CI and select pioarduino 55.03.311 and GNU++17. The peer target is Arduino-ESP32 3.3.11 on IDF 5.5.5. SHZK `59d76c5` pins Core 6.2.0, official espressif32 8.0.0, Arduino package `4.30312.261024` (Arduino 3.3.12), S3 libs 3.3.12 and esptool `2.50400.0`. Its v3.0.0 product pin has different build configuration. [Pinned sources and callers](SOURCE_MAP.md) · [Target profiles](VERSIONS.md)

**Initial checks.** On Python 3.12.10, `python scripts/check_promptset.py`, `python scripts/fetch_originals.py --verify` (30/30) and `python examples/python/sen059x_probe.py --self-test` passed in the checkout. `python -m unittest discover -s test -v` and `python scripts/generate_version.py check` hit read access errors on the checkout's `Version.h`. Both passed (40/40 tests, three generated outputs in sync) in a `git archive` extraction of the exact base at `C:/esp/build/sen059x-loop/01-baseline-10cc410/source`. `g++ -std=c++17 -Wall -Wextra -Werror -I include -c scripts/fixtures/idf_package_consumer/main/main.cpp -o ../version-consumer.o` and `doxygen Doxyfile` passed there. Local `python scripts/check_package.py` hit the header access error. `--prepare-idf ../consumer` in the extracted source reached PlatformIO packaging and hit unavailable SPDX validation. Local `scripts\pio.cmd run -e esp32-s3-devkitc-1` hit the read-only `C:/pio/platforms.lock` path. `scripts\pio.cmd --version` gave Core 6.1.19. `pkg list -e esp32-s3-devkitc-1` with `PYTHONIOENCODING=utf-8` listed espressif32 8.0.0 and S3 libs 3.3.12, but did not resolve the main Arduino package among the local installed candidates. The local native `C:\esp\idf.cmd 5.5.5 --version` reports `v5.5.5-dirty`, base `b774170ff46c393eeb5e495ea37936038d3f4f4f`, so use pinned CI for clean native evidence.

**Exact-base CI.** The `verify` job passed the offline checks, created and uploaded the package consumer, ran Doxygen and built the read-only Arduino probe. The build resolved official espressif32 8.0.0 with Core 6.1.19, Arduino package `4.30312.261116` (Arduino 3.3.12), S3 libs 3.3.12 and esptool `2.50400.0`. The S3 libraries report IDF 5.5.5 in their installed `versions.txt`. The successful scaffold build uses the same Arduino release as the SHZK target but a different host Core and Arduino package revision. It also differs from the FieldCore platform and Arduino release. Exact peer build verification belongs to stages 19, 20 and 27. The `idf-consumer` job was still running in the pinned `espressif/idf:v5.5.5` container at the last check. [Workflow](../../.github/workflows/ci.yml) · [Baseline CI run](https://github.com/janhavelka/SEN059x/actions/runs/44780324429)

**Next.** Phase b verifies and finishes the baseline evidence and makes any reproduced corrections within the reference or coverage scope. Recheck the native job receipt at the base revision and reconcile the coverage links with the source tables. Phase c performs six labeled solo review passes (requirements coverage, correctness, edge cases, tests, scope and simplicity) and challenges an omitted conditional write and the apparent compatibility of the resolved Arduino revision. Rerun the promptset, archive hashes, Python suite, probe and version checks after confirmed fixes. Phase d records acceptance and advances the ledger.
