# SEN0591 and SEN0592 hardware and installation

Identify the SKU before connecting power. A 24 V supply is within the SEN0592 rating and exceeds the SEN0591 maximum. The two sensors answer the same measurement registers. [SEN0591 specification](https://wiki.dfrobot.com/sen0591/) · [SEN0592 specification](https://wiki.dfrobot.com/sen0592/) · [Register map](protocol/registers.csv)

| Manufacturer rating | SEN0591 (3 m) | SEN0592 (5 m) |
| --- | --- | --- |
| Supply voltage | 3.3–12 V | 3.3–24 V |
| Rated distance, nearest to farthest target | 20–3000 mm | 30–5000 mm |
| Detection angle, English product specification | 30–60° | Horizontal 40–65°, vertical 60–80° |

Both sensors draw up to 10 mA in operation and take up to 1000 ms to become ready after power-on. DFRobot rates both enclosures IP67. It specifies −25 to +65 °C and 65–80% RH for operation, and −30 to +80 °C and 65–90% RH for storage. [SEN0591 product](https://www.dfrobot.com/product-2728.html) · [SEN0592 product](https://www.dfrobot.com/product-2729.html)

## Measurement limits

Both specifications give the accuracy as `1 + (S × 0.3%) cm`. They leave the meaning and units of `S` and the test target unspecified. The headline output response time is 15–750 ms. For a read deadline, use the selected register and range setting, then allow for time on the bus. [SEN0591 specification](https://wiki.dfrobot.com/sen0591/) · [SEN0592 specification](https://wiki.dfrobot.com/sen0592/) · [Measurement times and range settings](measurement-and-configuration.md)

For a SEN0591 installation, size the detection zone using the rated distance above. The highest range setting has a larger nominal value. For SEN0592, check which beam axis the source describes before using its angle to plan coverage. [Range settings](measurement-and-configuration.md#response-times-and-range-levels) · [SEN0592 beam specifications](measurement-and-configuration.md#beam-angle-and-power-supply-noise)

DFRobot describes filtering ultrasound from other emitters. The product documents give no guaranteed cross-sensor immunity, minimum spacing or firing interval for adjacent heads. For obstacle detection, check the installed geometry with small targets, angled surfaces, dust and nearby emitters. The supplied sources establish no personnel-protection rating. The project treats these sensors as supplementary inputs where safety depends on detecting a person. [SEN0591 product](https://www.dfrobot.com/product-2728.html) · [SEN0592 product](https://www.dfrobot.com/product-2729.html) · [Original project application notes, p. 4](SEN059x_reference.pdf#page=4)

## Wiring

| Wire on either sensor | Connection |
| --- | --- |
| Red | Positive supply within the SKU rating |
| Black | Ground / supply return |
| White | RS485 A |
| Yellow | RS485 B |

Connect the data pair to an RS485 transceiver between the sensor and the MCU UART. Match A/B against the adapter pinout. The SEN0592 storefront prose spells the interface “RS4585” in one place. Its specification, pinout and wiring diagram identify RS485. [SEN0591 wiring diagram](originals/dfrobot/SEN0591_arduino_wiring.webp) · [SEN0592 wiring diagram](originals/dfrobot/SEN0592_arduino_wiring.webp) · [SEN0592 product](https://www.dfrobot.com/product-2729.html)

On a non-isolated multidrop bus, provide a common reference, terminate the two physical ends of the trunk, and apply bias at one location if the network needs it. The sensor manuals leave internal termination and bias unspecified. Both Arduino examples use a DFR0259 shield and set the serial monitor to 115200 baud. [Modbus Serial Line V1.02, §§3.3.2 and 3.4](originals/modbus/Modbus_over_serial_line_V1_02.pdf#page=22) · [Shield switches and sample behavior](protocol/Modbus_RTU_notes.md#receiving-a-reply-on-a-shared-bus)

## Mechanical dimensions and panel openings

All dimensions below are in millimetres. The outline drawings describe the sensor. Use the separate panel drawing for the opening and its tolerances.

### SEN0591

| Feature | Drawing dimensions |
| --- | --- |
| Sensing face and element spacing | 31 ±0.2 × 16 ±0.2, on 15 ±0.2 centres |
| Housing | 33 ±0.3 × 18 ±0.3, overall depth 22.5 ±0.2 |
| Mounting flange | 47 ±0.3 across ears, thickness 2 ±0.2 |
| Flange holes | Two Ø3 ±0.2 on 40 ±0.2 centres |
| Front offsets | Face projects 4 ±0.2 from housing, 10 ±0.2 from front of flange |
| Panel opening | Obround 31.2 ±0.2 × 16.2 ±0.2, end arcs on 15 ±0.2 centres |
| Panel thickness | 1–4 |

The flange-hole spacing and the panel-opening dimensions come from separate drawings. [Sensor outline](originals/dfrobot/SEN0591_dimension.jpg) · [Chinese outline](originals/dfrobot/SEN0591_outline_CN.png) · [Panel drawing](originals/dfrobot/SEN0591_mounting.png)

### SEN0592

The sensing face measures 51.5 × 26.5 with the two elements on 25 centres. From the front face, allow 27 to the back of the housing and 31.25 to the cable exit, plus clearance for the cable. The rear view marks 50.6 × 26.16 across the profile and the side view marks 47.4 across the housing. The outline gives nominal dimensions. [Dimension drawing](originals/dfrobot/SEN0592_dimension.jpg) · [Chinese front and rear views](originals/dfrobot/SEN0592_outline_CN_1.png) · [Chinese side view](originals/dfrobot/SEN0592_outline_CN_2.png)

| Panel profile in the SEN0592 drawing | Overall opening | Detail |
| --- | --- | --- |
| Plain obround (left) | 49.5 (+0.1/−0.2) × 24.5 (+0/−0.2) | End arcs on 25 ±0.3 centres |
| Notched (right) | 49.16 (+0.1/−0.2) × 24.75 (+0/−0.2) | Main opening height 23.4 ±0.3, end radius 11.7. Tabs in the outline measure 5 ±0.3 across the long edges and 3.8 ±0.3 across the ends. Reference spacing 25 ±0.3. |

Both sections show a 3–5 panel thickness and R0.6 on both edges of the opening. DFRobot publishes both profiles on the same sheet without assigning either to a hardware revision or beam setting. Match the physical housing to the drawing before cutting a panel. [SEN0592 panel drawing](originals/dfrobot/SEN0592_mounting.png) · [Manufacturer installation section](https://wiki.dfrobot.com/sen0592/docs/19640)
