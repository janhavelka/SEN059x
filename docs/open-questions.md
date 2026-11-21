# SEN059x open questions

Use this register when selecting a bench fixture or defining how a product
will use SEN0591 and SEN0592. Follow the
[implementation sequence](prompts/README.md) for bench procedures and the
[progress ledger](prompts/PROGRESS.md) for actual coverage and evidence.

## Choices for the bench or product owner

### Which sensor should the first bench run qualify?

**Example.** One marked SEN0591 is available now, and a product also needs
the SEN0592. The same addressed read can succeed on both. A run on the
SEN0591 can establish measured behavior for that unit, while the SEN0592
still needs its own evidence.

| Option | Pros | Cons |
| --- | --- | --- |
| Start with one available, identified unit | Gets a known-target read and timing trace sooner. | The evidence covers the model and bench setup actually tested. |
| Equip the bench for both sensor models | Gives comparable results and can settle model differences. | Requires a second unit and a separate run under the correct supply, target and setup. |

Start with the sensor model intended for the first product if it is available.
Identify the unit by marking or installation record before the run.
[Read-only bench plan](prompts/28-bench-measurements.md)

### Which isolated commissioning changes can the bench qualify?

Commissioning means setting up the sensor before putting it on the shared
RS485 cable. Here its address and serial speed (baud rate) are the two
settings the controller must match to reach it.

**Example.** A unit starts at 115200 baud. A change to 9600 might take
effect even if the host misses the acknowledgment. A successful read at
9600 establishes the current endpoint. A separate restart observation is
needed to tell whether the setting survives loss of power.

| Option | Pros | Cons |
| --- | --- | --- |
| Read only on the available fixture | Collects measurement and baseline evidence while preserving the configuration. | Leaves settings writes, address and baud changes, and persistence open. |
| Change a setting that leaves address and speed unchanged | Changing the range on one isolated sensor, reading it back and putting the old value back tests setup over the existing link. | Leaves address and baud changes open. Needs permission to change and restore the selected setting. |
| Change the address or speed on an isolated sensor | Tests whether the controller can reach the new address or speed, whether it survives power loss, and whether the original settings can be restored. | Needs known original settings and a recovery plan. Requires permission for power cycling or commands that send without waiting for a reply if the campaign includes those cases. |

Begin with read-only evidence. Record which changes and restart tests are
allowed on the isolated unit before dispatching the commissioning campaign.
[Commissioning bench plan](prompts/29-bench-commissioning.md)

### How fast does the application need fresh distance from each head?

**Example.** A controller polls four heads and shows a stale indicator
when readings get old. A slow processed-distance read holds the shared
exchange open, so the other heads wait even though their own requests
may be quick.

| Option | Pros | Cons |
| --- | --- | --- |
| Schedule real-time distance for regular updates | Shorter documented response estimates give the shared bus more time for other heads. | Each consumer needs evidence that the real-time output meets its stability and target-detection needs. |
| Schedule processed distance for regular updates | Preserves the device's processed output for every head with one simple schedule. | Longer requests can increase the worst-case reading age across the bus. The processing algorithm is unspecified. |
| Mix regular real-time reads with processed reads for the workflows that need them | Keeps both outputs available without scheduling a processed read for every head on every pass. | A processed request still occupies the bus until it settles. Check other heads' worst-case reading age during that slot. |

Set an acceptable reading age per head, then test the chosen scan at
target range with the actual number of heads. Use the broader documented
estimates for deadlines until the installation has suitable evidence.
[Measurement modes and timing](reference/measurement-and-configuration.md) · [Shared-bus scheduler](prompts/11-bus-settlement.md)

### What spacing or placement is acceptable if adjacent heads interfere?

**Example.** Four heads point into the same work area. A head occasionally
reports a plausible distance to an adjacent reflector after its neighbour
has ranged. Correct Modbus framing alone cannot establish which physical
echo was received.

| Option | Pros | Cons |
| --- | --- | --- |
| Keep the mounting and space the ranging requests | Easy to vary on the bench and may reduce interaction. | Reduces scan rate. The manual gives no guaranteed spacing interval. |
| Alternate requests between heads that are acoustically separated | Gives nearby emitters more time apart without adding the same delay after every request. | Needs evidence that the chosen pairs can range consecutively. May offer no improvement if all heads face the same reflector. |
| Change emitter position or direction when mounting allows | Can reduce coupling and may allow shorter intervals. | Costs fixture changes and requires a new check of target geometry and coverage. |

Capture a single-head baseline and compare it with sequential tests on
the intended four-head geometry. Use the measured scan rate and target
coverage to choose. [Multidrop and acoustic bench plan](prompts/30-bench-shared-bus.md)

## Questions that need evidence first

| Open device behavior | Example and current handling | Ways to proceed and their limits |
| --- | --- | --- |
| Missing echo and subzero temperature | A head aimed outside its usable range returns a word, or a cold head returns an unfamiliar high temperature word. Keep the raw word available, reject out-of-rating distance for typed use, and avoid assigning an undocumented no-echo code or temperature sign. | Ask the manufacturer for the encoding for the exact model and firmware. This may cover more configurations than a single unit, but only if the answer identifies the applicable variants. A focused capture with a known target, controlled no-return condition and calibrated cold reference proves what the tested unit emits. It needs a fixture, and other variants still need matching evidence. [Measurement bench plan](prompts/28-bench-measurements.md) |
| Reading several adjacent registers together and checking when they were measured | One Modbus request can ask for adjacent registers, for example processed distance, real-time distance and temperature. Keep that request for examining the returned values. A valid reply can still contain readings taken at different times. | Use separate measurement requests if the application can accept readings from different times. This uses more exchanges. If values must refer to the same time, test the exact group of registers on each intended model and use a timing reference to check when they were measured. [Wire reference](reference/protocol/Modbus_RTU_notes.md#frame-layout) |
| Switch and UART behavior in RS485 units | A register readback confirms a switch threshold or UART-unit value. The four-wire RS485 pinout provides no observed switch output and the RS485 measurement units stay defined by the measurement register. | For an RS485-only product, check addressed readback and implement any required distance threshold or display conversion in the application using verified readings. This keeps those functions available over RS485 and adds application logic. If a physical switch or UART output is required, test an identified mode-capable part. It can verify the native behavior, including threshold equality, for that part and requires a different fixture if those signals are absent. [Configuration reference](reference/measurement-and-configuration.md#configuration-on-the-rs485-models) |
| Which speed the reply uses, and whether the settings survive power loss | The adapter might echo the command it just sent. That can look like the sensor's acknowledgement (ACK). A successful read at the new speed only shows the sensor uses that speed now. | Read back at the new speed, then repeat after an authorized power cycle to check what the sensor kept. To find the ACK speed, capture traffic on the wire so the command echo and sensor reply can be distinguished. Verify restoration in either case. [Commissioning bench plan](prompts/29-bench-commissioning.md) |

Record any observed result with the SKU, firmware or visible device
identification, settings, fixture and raw evidence in the owning stage.
[Manufacturer cross-check](reference/README.md#manufacturer-cross-check) · [Progress and evidence](prompts/PROGRESS.md)
