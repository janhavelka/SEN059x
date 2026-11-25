# Using SEN059x with a FieldCore RS485 owner

The native fixtures exercise SEN0591 (3 m) and SEN0592 (5 m) through the installed-style `<SEN059x/SEN059x.h>` API. Select the SKU when binding the device. Both models answer the same register reads. The tests run offline and need no FieldCore checkout. Both pinned product profiles use SHZK-PT v3.0.0 and VibWire-108 v1.2.0. Integrating a SEN059x module into either firmware profile and building that profile remain open. [Profile pins and caller paths](prompts/SOURCE_MAP.md)

## Owner contracts

| | FieldCore main | FieldCore rodtester |
| --- | --- | --- |
| Pinned revision | [`eccdbdcc0e9ce22beee4f54494c6c179a320d3aa`](https://github.com/janhavelka/FieldCore-node/blob/eccdbdcc0e9ce22beee4f54494c6c179a320d3aa/include/TunnelMonitor/rs485/Rs485OwnerTransaction.h) | [`c40ea8341edf8d665460b902cafefbe857a7f73f`](https://github.com/janhavelka/FieldCore-node/blob/c40ea8341edf8d665460b902cafefbe857a7f73f/include/TunnelMonitor/rs485/Rs485OwnerTransaction.h) |
| Call and storage | `takeStep(step)`, 8-byte TX, 192-byte RX | `takeStep(step, nowMs)`, 21-byte TX, 192-byte RX |
| Receive completion | Fixed length completes at or above the target. Suffix mode checks the retained tail (up to two bytes). The owner strips a matching TX prefix. | RTU steps carry eligibility, deadline, frame gap, echo policy and a copy of the operation, target, generation, address and step identity. |
| Meaning of the bytes | The parser can validate the delivered bytes. A 7-byte FC03 prefix yields 751 mm even if surplus bytes arrive after delivery. Delivering all nine bytes in one batch fails validation. | The fixture checks identity, complete drained TX, equal accessible and total RX length, zero wire errors, ordered closure bounds and confirmed source. The same 7-byte prefix yields 751 mm with qualified evidence. Buffered or ambiguous delivery fails. |

The Arduino backend can expose buffered counts, drain and UART errors. Precise wire closure in these tests comes from the scripted backend. A delayed rodtester callback can succeed when its qualified closure met both the response and command deadlines. Main has no generation field on the observation. Its owner must consume callbacks synchronously and settle a previous exchange before permitting reuse. [Main receive loop](https://github.com/janhavelka/FieldCore-node/blob/eccdbdcc0e9ce22beee4f54494c6c179a320d3aa/src/rs485/Rs485Task.cpp#L1217-L1346) · [Rodtester owner](https://github.com/janhavelka/FieldCore-node/blob/c40ea8341edf8d665460b902cafefbe857a7f73f/src/rs485/Rs485Task.cpp#L1776-L1833) · [Arduino backend](https://github.com/janhavelka/FieldCore-node/blob/c40ea8341edf8d665460b902cafefbe857a7f73f/src/rs485/ArduinoRs485Backend.cpp)

## Binding a measurement

1. Select the SKU, product device ID, measurement kind and 8N1 format. Call `bind(error)` and `applyRuntimeConfig(config, nowMs, error)`. The standard binding accepts addresses 1–247 and baud rates of 9600, 19200, 38400, 57600 and 115200.
2. Supply a `DeviceCommand` with request ID, device ID, kind and active absolute deadline to `start(command, nowMs)`. The binding can return `Accepted`, `Busy`, `Disabled`, `Unsupported` or `Rejected`. The owner retains each admitted command until collection.
3. Poll the binding and take the revision-specific step. `Probe` builds a typed FC03 for the baud register. `Measure` builds an FC03 for the selected quantity. The owner controls UART, DE/RE, serialization, deadlines and retries. Deliver the resulting observation while the borrowed RX bytes are valid. `ReadLast` completes from cache with zero bus transfers.
4. At `Complete`, call `takeResult(result)` once. Check the copied command identity and deadline, completion uptime and masks. A second collection fails. Both owners use `snapshot()` for module health, presence and uptime. Use `copyStatus` for RS485 counters and link health, and `sample(nowMs)` for the age and quality of the last measurement. [Shared lifecycle test](../test/native/test_fieldcore_lifecycle.cpp)

| Terminal status | Reading count and masks for a one-slot Measure or ReadLast |
| --- | --- |
| Ok | Count 1, valid bit 0 set, stale and error masks clear |
| Stale | Count 1, stale bit 0 set, valid and error masks clear |
| Timeout, Cancelled, Failed | Count 1, error bit 0 set, valid and stale masks clear |
| Disabled, Absent | Count 1, all masks clear |

A Probe has count zero and all masks clear. The owner can complete queued work as `Disabled`. An active disable produces `Cancelled` and clears the measurement cache. Re-enabling starts at Unknown presence and Initializing health. The tests exercise Idle, Running and Complete on the sensor bindings. A scripted module signal exercises the owner's `NeedsBusRecovery` branch, which invalidates the active bus operation and collects its terminal result. `Absent` is a product result shape. The pinned SHZK drivers report absence after misses in the *presence* field. [Product result constructor](https://github.com/janhavelka/FieldCore-node/blob/eccdbdcc0e9ce22beee4f54494c6c179a320d3aa/include/TunnelMonitor/contracts/DeviceMeasurement.h#L282-L307) · [Owner polling](https://github.com/janhavelka/FieldCore-node/blob/c40ea8341edf8d665460b902cafefbe857a7f73f/src/rs485/Rs485Task.cpp#L1161-L1178)

## Product health and measurement age

The common scenario uses address 1 at 9600 baud, a 100 ms response timeout, zero retries, stale/fault thresholds of two/three silent misses and a 1000 ms cache age limit. The following times are injected uptimes in milliseconds. [Lifecycle assertions for both SKUs](../test/native/test_fieldcore_lifecycle.cpp)

| Event | Result and retained state |
| --- | --- |
| Probe at 27 ms and real-time distance at 75 ms | Baud code 3 and 751 mm. A cached lookup at age 1000 ms is fresh. At 1001 ms it returns Stale. |
| Addressed FC03 exception 2 | Product result Failed, `lastParserError = ExceptionResponse`, `parserErrorCount = 1`, `consecutiveMissCount = 0`. Presence stays Present and health becomes Degraded. The valid exception counts as the third library link success. Main delivers its five bytes on response timeout at 1211 ms. Rodtester delivers them at 1125 ms with qualified closure bounds. |
| Failed cache lookup at 1230 ms | Both report `Rs485NoCachedSnapshot`. Main increments its error and parser counters to two and stamps its last attempt and error at 1230 ms. Rodtester keeps both counters at one and those times at 1125 ms. |
| Three silent measurements | Both return Timeout, Stale, Timeout. On miss two, main's `lastStatus` is Timeout and rodtester's is Stale. Presence becomes Absent, `staleCount` reaches one and health reaches Fault on miss three. `lastParserError` clears. The status snapshot retains `validMask = 1` for the historical reading. |
| Successful recovery probe at 1925 ms | Present/Ok, library READY, `consecutiveMissCount = 0`. Last attempt and last success are 1925 ms, last error remains 1811 ms. At the next cached read the 75 ms sample is 1851 ms old. |

Main's malformed-frame cases also cover BadCrc, TruncatedFrame, UnexpectedAddress, UnexpectedFunction and ValueCountMismatch. An unusable distance maps to OutOfRangeValue. The fixture retains the product parser code and the raw library evidence separately. [Parser mappings and failure cases](../test/native/test_fieldcore_main.cpp)

## Addressed maintenance

The host maintenance caller uses an enabled, idle binding and waits for bus availability. It rejects addresses 248 and 255 and 4800 baud, and a write preview emits no bytes. Both SKUs use `Setting::SwitchThreshold`, register `0x0206` in millimetres. The 100 mm seed and 750 mm request are synthetic operation cases derived from the register table. [Register inventory and sources](reference/protocol/registers.csv) · [Operation cases](prompts/OPERATIONS.csv)

The caller uses `Configuration::buildRead` and `completeRead` to establish the initial word. On confirmation it sends one addressed FC06, reports the TX evidence, then inspects `operation()` and `snapshot(Setting::SwitchThreshold)`.

| Injected event | Write outcome | Independent addressed FC03 |
| --- | --- | --- |
| Lone FC06-shaped echo | Main strips all eight bytes and reports TIMEOUT. Rodtester retains a valid ACK shape with Unknown origin and the host caller records NO_DATA. Both retain 100 mm as untrusted history. | The scripted device returns 750 mm. The new read is trusted and the original write stays unverified. |
| Backend accepts eight bytes and flush fails | Both record possible TX and IO with the injected backend detail `-17`. Main reports the accepted count. Rodtester retains eight echo-shaped bytes, sets the backend wire-error bit and marks the bus unavailable. The write and ACK diagnostics retain the flush error. | An attempted read during recovery sends zero bytes. After recovery the scripted device returns 100 mm. The original IO outcome survives. |

Each trace sends the seed FC03, one FC06 and the independent FC03. Extended addresses and send-only vendor writes belong to the standalone owner. [Maintenance tests](../test/native/test_fieldcore_configuration.cpp) · [Configuration API](../include/SEN059x/Configuration.h) · [Standalone owner tests](../test/native/test_scheduler_vendor.cpp)

## Reproduce

From the repository root, run the following with a desktop C++17 compiler.

```sh
cmake -S test/native -B .pio/sen059x-native -DCMAKE_BUILD_TYPE=Release
cmake --build .pio/sen059x-native --config Release
ctest --test-dir .pio/sen059x-native -C Release -R "^fieldcore_" --no-tests=error --output-on-failure
```

At SEN059x revision `38f348e749445577bd9bafb43e1b723e4f4cb4f9`, all five FieldCore tests and all 23 native tests passed in a clean GCC 15.1.0 Release build. Separate checks against each pinned product's headers matched all six Disabled/Absent result shapes and rejected a false valid bit in every case. [Acceptance record](prompts/PROGRESS.md#phase-d-fieldcore-fixture-acceptance)

For each of SEN0591 and SEN0592, physical frame closure, FC06 echo origin and device behavior after a failed drain still need a bench observation with the identified sensor and backend.
