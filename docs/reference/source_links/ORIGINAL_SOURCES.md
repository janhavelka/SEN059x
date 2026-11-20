# Source inventory

The [engineering reference](../README.md) draws on the originals listed below.
Use the matching SKU and the language edition named in a finding.
The [download manifest](sources.json) records the retrieval URL
for every item, SHA-256 and byte count for the bundled originals and optional
Dianyingpu PDFs, and the Git revisions of the four software archives.

## DFRobot device documents

| Document | SEN0591 (3 m) | SEN0592 (5 m) |
| --- | --- | --- |
| Storefront specification and IP rating | [Offline](../originals/dfrobot/SEN0591_product.html) · [DFRobot](https://www.dfrobot.com/product-2728.html) | [Offline](../originals/dfrobot/SEN0592_product.html) · [DFRobot](https://www.dfrobot.com/product-2729.html) |
| Chinese storefront specification and product photos | [Offline](../originals/dfrobot/SEN0591_product_CN.html) · [DFRobot China](https://www.dfrobot.com.cn/goods-3751.html) | [Offline](../originals/dfrobot/SEN0592_product_CN.html) · [DFRobot China](https://www.dfrobot.com.cn/goods-3752.html) |
| Cable assembly supplied in the product photo | [SEN0591 photo](../originals/dfrobot/SEN0591_cable.jpg) | [SEN0592 photo](../originals/dfrobot/SEN0592_cable.jpg) |
| English device specifications and pinout | [Offline](../originals/dfrobot/SEN0591_overview.html) · [DFRobot](https://wiki.dfrobot.com/sen0591/) | [Offline](../originals/dfrobot/SEN0592_overview.html) · [DFRobot](https://wiki.dfrobot.com/sen0592/) |
| English protocol tables and six worked transactions | [Offline](../originals/dfrobot/SEN0591_modbus_reference.html) · [DFRobot](https://wiki.dfrobot.com/sen0591/docs/19645) | [Offline](../originals/dfrobot/SEN0592_modbus_reference.html) · [DFRobot](https://wiki.dfrobot.com/sen0592/docs/19640) |
| Chinese manual, including full baud-code table | [Offline](../originals/dfrobot/SEN0591_wiki_CN.html) · [DFRobot](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m) | [Offline](../originals/dfrobot/SEN0592_wiki_CN.html) · [DFRobot](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m) |
| Arduino read example and shield setup | [Offline](../originals/dfrobot/SEN0591_arduino_example.html) · [DFRobot](https://wiki.dfrobot.com/sen0591/docs/19646) | [Offline](../originals/dfrobot/SEN0592_arduino_example.html) · [DFRobot](https://wiki.dfrobot.com/sen0592/docs/19641) |
| Shield wiring diagram | [Drawing](../originals/dfrobot/SEN0591_arduino_wiring.webp) | [Drawing](../originals/dfrobot/SEN0592_arduino_wiring.webp) |
| Dimensioned outlines | [English sheet](../originals/dfrobot/SEN0591_dimension.jpg) · [Chinese sheet](../originals/dfrobot/SEN0591_outline_CN.png) | [English sheet](../originals/dfrobot/SEN0592_dimension.jpg) · [Chinese sheet 1](../originals/dfrobot/SEN0592_outline_CN_1.png) · [Chinese sheet 2](../originals/dfrobot/SEN0592_outline_CN_2.png) |
| Recommended installation opening | [Drawing](../originals/dfrobot/SEN0591_mounting.png) | [Drawing](../originals/dfrobot/SEN0592_mounting.png) |

Both Arduino examples link the same serial-monitor image. It shows the timing
of the complete demonstration loop. The shield's own specification describes
how its two switches route Arduino UART traffic. [Shared result screenshot](../originals/dfrobot/SEN059x_arduino_result.webp) · [DFR0259 shield specification](../originals/dfrobot/DFR0259_overview.html) · [DFRobot shield wiki](https://wiki.dfrobot.com/dfr0259/)

Saved HTML retains the publisher's image URLs. The drawings listed above
can also be opened directly offline. The [historical Czech PDF](../SEN059x_reference.pdf)
was prepared for this project. The [registry survey](registry-survey.md)
records where distributors' datasheet and declaration links led.

## Dianyingpu family documents

The SEN0591 and SEN0592 drawings match the A22 and A21 housing drawings,
respectively. The SEN0592 drawing even carries the A21 housing marking.
No reviewed source identifies the firmware or full DYP order code supplied
under either DFRobot SKU. This matters when interpreting the family tables.
[DFRobot drawing comparison](../hardware.md#mechanical-dimensions-and-panel-openings) · [A22 supplier](https://www.dypcn.com/2cm-blind-zone-ip67-high-precision-ultrasonic-sensor-product/) · [A21 supplier](https://www.dypcn.com/high-precision-ultrasonic-sensor-dyp-a21-product/)

| Family and matching DFRobot outline | Mechanical drawing, bundled | Optional family documents |
| --- | --- | --- |
| A22 / SEN0591 | [Offline](../originals/dyp/A22-Mechanical-Dimensions.pdf) · [Publisher](https://www.dypcn.com/uploads/A22-Mechanical-Dimensions.pdf) | [Datasheet](https://www.dypcn.com/uploads/A22-Datasheet1.pdf) · [Output interfaces](https://www.dypcn.com/uploads/A22-Output-Interfaces1.pdf) |
| A21 / SEN0592 | [Offline](../originals/dyp/A21-Mechanical-Dimensions.pdf) · [Publisher](https://www.dypcn.com/uploads/A21-Mechanical-Dimensions.pdf) | [Datasheet](https://www.dypcn.com/uploads/A21-Datasheet-231128.pdf) · [Output interfaces, 231128](https://www.dypcn.com/uploads/A21-Output-Interfaces-231128.pdf) |

- **Temperature.** The A22 interface PDF includes two entries for `0x0102`, one unsigned and one signed. The DFRobot editions describe an unsigned word and give only a positive example. Keep the raw register word for a subzero test. [A22 interface table](https://www.dypcn.com/uploads/A22-Output-Interfaces1.pdf#page=10) · [DFRobot register map](../protocol/registers.csv)
- **Interference.** DYP describes `0xFFFE` when same-frequency ultrasound interferes. DFRobot separately specifies `0xFFFD` for a missing target on its SEN0754. Neither supplies a missing-target or interference code for SEN0591 or SEN0592. Record the raw response during those two bench conditions. [A21 datasheet](https://www.dypcn.com/uploads/A21-Datasheet-231128.pdf#page=3) · [A22 family description](https://www.dypcn.com/2cm-blind-zone-ip67-high-precision-ultrasonic-sensor-product/) · [DFRobot SEN0754 protocol](https://wiki.dfrobot.com/sen0754/docs/24696)
- **Beam settings.** DYP lists separate single- and dual-angle A21 RS485 order codes. Its interface sheet gives the level-4 dual angle as horizontal 65° and vertical 90°. Both DFRobot SEN0592 register tables give 65° and 80°. Use the DFRobot row for the retail SKU. [A21 model list](https://www.dypcn.com/high-precision-ultrasonic-sensor-dyp-a21-product/) · [A21 level settings](https://www.dypcn.com/uploads/A21-Output-Interfaces-231128.pdf#page=11) · [SEN0592 configuration](../measurement-and-configuration.md#beam-angle-and-power-supply-noise)
- **Range timing.** At levels 1–4, the A21 interface supplement gives the same timing pairs as the SEN0591 level table. Its additional 5 m level gives 40–140 ms real-time and 320–750 ms processed. The A21 test distances span 0.5–5 m. DFRobot's SEN0592 pages give timing by measurement register and leave the per-level values open. [A21 timing conditions](https://www.dypcn.com/uploads/A21-Output-Interfaces-231128.pdf#page=10) · [A21 range-level table](https://www.dypcn.com/uploads/A21-Output-Interfaces-231128.pdf#page=12) · [DFRobot timing by edition](../measurement-and-configuration.md#response-times-and-range-levels)

## Modbus Organization specifications

| Edition | Relevant material | Original |
| --- | --- | --- |
| Application Protocol V1.1b3 | §§6.3 and 6.6 define the read-holding-register and write-single-register PDUs. §7 covers exception responses. | [Offline PDF](../originals/modbus/Modbus_Application_Protocol_V1_1b3.pdf) · [Publisher](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf) |
| Serial Line V1.02 | §§2.2, 2.5.1 and Appendix B cover addresses, RTU framing, silence intervals and CRC. §3 covers the physical bus. | [Offline PDF](../originals/modbus/Modbus_over_serial_line_V1_02.pdf) · [Publisher](https://www.modbus.org/file/secure/modbusoverserial.pdf) |
| Serial Line V1.0 | Legacy edition retained for comparing older documentation. The publisher directs new implementations to V1.02. | [Offline PDF](../originals/modbus/Modbus_over_serial_line_V1_LEGACY.pdf) · [Publisher's edition guide](https://www.modbus.org/modbus-specifications) |

## Supporting references

The fetcher can retrieve the four source archives below, the DFRobot_RTU
project page and three related sensor overviews. The archives illustrate
transport choices for a host application. DFRobot's SKU-specific documents
supply the values for this repository's register map.

| Pinned source | Relevance | License in this revision |
| --- | --- | --- |
| [DFRobot_RTU](https://github.com/DFRobot/DFRobot_RTU/tree/54df8dbace1c636ed0c05cc407b42055a3810919) | Arduino `Stream` interface, optional DE pin and single holding-register operations. | MIT |
| [ModbusMaster](https://github.com/4-20ma/ModbusMaster/tree/3a05ff87677a9bdd8e027d6906dc05ca15ca8ade) | Arduino client with callbacks before and after transmission for direction control. | Apache-2.0 |
| [modbus-esp8266](https://github.com/emelianov/modbus-esp8266/tree/73f19119a4f92b092d395eed0ce29d18499611fe) | Arduino RTU client/server with stream and transmit-enable support. | BSD-3-Clause |
| [esp-modbus](https://github.com/espressif/esp-modbus/tree/16583355879da8e4c4063f0c33f9219e30f7142c) | Espressif's native ESP-IDF stack and serial examples. | Apache-2.0 plus component notices |

Of the related products, URM15 uses another ultrasonic Modbus register map,
SEN0492 measures distance by laser, and URM08 uses its own serial frame
format. Use their own manuals when working with those units. [URM15/SEN0519](https://wiki.dfrobot.com/sen0519/) · [SEN0492](https://wiki.dfrobot.com/sen0492/) · [URM08/SEN0246](https://wiki.dfrobot.com/sen0246/)

The repository's desktop probe uses pySerial for the serial port. PyModbus is
an alternative host-side stack. Community implementations for these exact SKUs
are linked in the registry survey. [pySerial](https://pyserial.readthedocs.io/) · [PyModbus](https://github.com/pymodbus-dev/pymodbus) · [Device-specific community code](registry-survey.md#device-specific-community-code)
