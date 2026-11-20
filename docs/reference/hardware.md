# SEN0591 and SEN0592 hardware and installation

Identify the SKU before connecting power. A 24 V supply is within the SEN0592 rating and exceeds the SEN0591 maximum. The two sensors answer the same measurement registers. [SEN0591 specification](https://wiki.dfrobot.com/sen0591/) · [SEN0592 specification](https://wiki.dfrobot.com/sen0592/) · [Register map](protocol/registers.csv)

| Manufacturer rating | SEN0591 (3 m) | SEN0592 (5 m) |
| --- | --- | --- |
| Supply voltage | 3.3–12 V | 3.3–24 V |
| Rated distance, nearest to farthest target | 20–3000 mm | 30–5000 mm |
| Detection angle, English and Chinese storefront specifications | 30–60° | Horizontal 40–65°, vertical 60–80° |

DFRobot lists operating current ≤10 mA and a power-on delay ≤1000 ms for both sensors. Its SKU sheets give no peak current during an acoustic burst. Both enclosures are rated IP67. DFRobot specifies −25 to +65 °C and 65–80% RH for operation, and −30 to +80 °C and 65–90% RH for storage. [SEN0591 product](https://www.dfrobot.com/product-2728.html) · [SEN0592 product](https://www.dfrobot.com/product-2729.html)

## Measurement limits

Both DFRobot specifications give the accuracy as `1 + (S × 0.3%) cm` and omit the definition of `S` and the test conditions. Dianyingpu's related A22 and A21 family sheets use the same expression, define `S` as measured distance, and describe a 50 × 60 cm flat carton facing the sensor at 25 °C and 65% RH. That test is documented for the DYP families. Its applicability to a delivered SEN0591 or SEN0592 remains unconfirmed. [DFRobot SEN0591](https://www.dfrobot.com.cn/goods-3751.html) · [DFRobot SEN0592](https://www.dfrobot.com.cn/goods-3752.html) · [DYP A22, §1.5 note 3](https://www.dypcn.com/uploads/A22-Datasheet1.pdf#page=5) · [DYP A21, §1.5 note 3](https://www.dypcn.com/uploads/A21-Datasheet-231128.pdf#page=6)

DFRobot gives an overall output response time of 15–750 ms for both SKUs. For a read deadline, use the selected register and range setting, then allow for time on the bus. [SEN0591 specification](https://wiki.dfrobot.com/sen0591/) · [SEN0592 specification](https://wiki.dfrobot.com/sen0592/) · [Measurement times and range settings](measurement-and-configuration.md)

For a SEN0591 installation, size the detection zone using the rated distance above. The highest range setting has a larger nominal value. For SEN0592, check which beam axis the source describes before using its angle to plan coverage. [Range settings](measurement-and-configuration.md#response-times-and-range-levels) · [SEN0592 beam specifications](measurement-and-configuration.md#beam-angle-and-power-supply-noise)

DFRobot describes filtering ultrasound from other emitters. The product documents give no guaranteed cross-sensor immunity, minimum spacing or firing interval for adjacent heads. For obstacle detection, check the installed geometry with small targets, angled surfaces, dust and nearby emitters. The supplied sources establish no personnel-protection rating. The project treats these sensors as supplementary inputs where safety depends on detecting a person. [SEN0591 product](https://www.dfrobot.com/product-2728.html) · [SEN0592 product](https://www.dfrobot.com/product-2729.html) · [Original project application notes, p. 4](SEN059x_reference.pdf#page=4)

## Wiring

| Wire on either sensor | Connection |
| --- | --- |
| Red | Positive supply within the SKU rating |
| Black | Ground / supply return |
| White | RS485 A |
| Yellow | RS485 B |

Connect the data pair to an RS485 transceiver between the sensor and the MCU UART. Match A/B against the adapter pinout. Both Chinese storefronts and the English SEN0592 storefront spell the interface “RS4585” in their introductions. Their specification tables and wiring diagrams identify RS485. [SEN0591 wiring](originals/dfrobot/SEN0591_arduino_wiring.webp) · [SEN0592 wiring](originals/dfrobot/SEN0592_arduino_wiring.webp) · [SEN0591 China store](originals/dfrobot/SEN0591_product_CN.html) · [SEN0592 China store](originals/dfrobot/SEN0592_product_CN.html) · [SEN0592 English store](https://www.dfrobot.com/product-2729.html)

DFRobot's product photos show four individually terminated leads on the SEN0591 and an inline cable coupling before the four leads on the SEN0592. Use the wire colours above at the host end. DFRobot's documents give no connector series or cable length. [SEN0591 cable](originals/dfrobot/SEN0591_cable.jpg) · [SEN0592 cable](originals/dfrobot/SEN0592_cable.jpg)

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

The flange-hole spacing and the panel-opening dimensions come from separate drawings. Dianyingpu's A22 family datasheet reproduces both views and their tolerances with a dated drawing title block. [Sensor outline](originals/dfrobot/SEN0591_dimension.jpg) · [Chinese outline](originals/dfrobot/SEN0591_outline_CN.png) · [Panel drawing](originals/dfrobot/SEN0591_mounting.png) · [DYP A22 datasheet, §1.6](https://www.dypcn.com/uploads/A22-Datasheet1.pdf#page=6) · [DYP A22 mechanical drawing](originals/dyp/A22-Mechanical-Dimensions.pdf)

### SEN0592

The sensing face measures 51.5 × 26.5 with the two elements on 25 centres. From the front face, allow 27 to the back of the housing and 31.25 to the cable exit, plus clearance for the cable. The rear view marks 50.6 × 26.16 across the profile and the side view marks 47.4 across the housing. The outline gives nominal dimensions. [Dimension drawing](originals/dfrobot/SEN0592_dimension.jpg) · [Chinese front and rear views](originals/dfrobot/SEN0592_outline_CN_1.png) · [Chinese side view](originals/dfrobot/SEN0592_outline_CN_2.png)

| Panel profile in the SEN0592 drawing | Overall opening | Detail |
| --- | --- | --- |
| Plain obround (left) | 49.5 (+0.1/−0.2) × 24.5 (+0/−0.2) | End arcs on 25 ±0.3 centres |
| Notched (right) | 49.16 (+0.1/−0.2) × 24.75 (+0/−0.2) | Main opening height 23.4 ±0.3, end radius 11.7. Tabs in the outline measure 5 ±0.3 across the long edges and 3.8 ±0.3 across the ends. Reference spacing 25 ±0.3. |

Both sections show a 3–5 panel thickness and R0.6 on both edges of the opening. DFRobot publishes both profiles on the same sheet without assigning either to a hardware revision or beam setting. Match the physical housing to the drawing before cutting a panel. [SEN0592 panel drawing](originals/dfrobot/SEN0592_mounting.png) · [Manufacturer installation section](https://wiki.dfrobot.com/sen0592/docs/19640)

For a CAD model, the rear view also marks radii R13.25 and R11.2, an inner height of 22.38 and retaining details 4 and 2.8 wide. The side view locates features 2, 2.9 and 17.3 from the front face. Use the dimension arrows in the original views to locate each feature. [Rear view](originals/dfrobot/SEN0592_outline_CN_1.png) · [Side view](originals/dfrobot/SEN0592_outline_CN_2.png)

The rear view carries the mark `DYP-A21-A`. It also appears on Dianyingpu's A21 family outline, shared by several angle and output variants. This identifies the drawing family. DFRobot gives no DYP ordering code or firmware revision for either SKU. [DFRobot rear view](originals/dfrobot/SEN0592_outline_CN_1.png) · [DYP A21 mechanical drawing](originals/dyp/A21-Mechanical-Dimensions.pdf) · [Source inventory](source_links/ORIGINAL_SOURCES.md)
