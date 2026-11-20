# Measurement and configuration

The [register map](protocol/registers.csv) covers all 12 addresses in the
SEN0591 and SEN0592 manuals. Each address is the hexadecimal address carried
in the Modbus request and holds one unsigned 16-bit word. The model columns
give approximate response times for measurements and allowed settings or
level mappings for configuration. The access column follows DFRobot's two
register tables. Identify the SKU from the device or installation record,
since both SKUs use the same addresses. [SEN0591 register tables](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0592 register tables](https://wiki.dfrobot.com/sen0592/docs/19640)

## Choosing a measurement

Reading processed distance, real-time distance or echo time starts a ranging
cycle. A sequence of individual reads therefore samples the target at
different times. DFRobot's Arduino sketches read a single word of real-time
distance. The manuals give a response time for temperature but leave its
sampling instant and support for combined multi-register reads unspecified.
[SEN0591 Arduino example](https://wiki.dfrobot.com/sen0591/docs/19646) · [SEN0592 Arduino example](https://wiki.dfrobot.com/sen0592/docs/19641) · [Register map](protocol/registers.csv)

For temperature, the published example gives a positive reading and shows
the scaling. A subzero reading needs a capture at a known temperature to
establish how the device encodes the sign. Likewise, the manuals provide
no missing-echo value for distance or echo time. Keep the raw word when
investigating either case. [DFRobot response examples](protocol/modbus_vectors.csv) · [SEN0591 Chinese manual](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m) · [SEN0592 Chinese manual](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m)

## Response times and range levels

For SEN0592, the English and Chinese editions give different minimum times
for processed distance, temperature and echo time. The upper estimates
agree. The register map records both editions. Budget for the selected
measurement and serial transmission when scheduling a read. [Register map](protocol/registers.csv) · [Packet durations](protocol/Modbus_RTU_notes.md#wire-time-and-measurement-time) · [SEN0592 English table 1](https://wiki.dfrobot.com/sen0592/docs/19640) · [SEN0592 Chinese table 1](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m)

SEN0591 also gives approximate distance response times for each value of
`0x021F`. Use the model's entry in the register map for the distance
associated with each level. DFRobot rates the SEN0591 to 3 m and describes
level 4 as approximately 3.5 m. Usable performance beyond the rating is
unverified. SEN0592 lists a distance for each level and gives timing only
by measurement register. [SEN0591 product rating](https://www.dfrobot.com/product-2728.html) · [SEN0591 English table 2](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0591 Chinese table 2](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m) · [SEN0592 table 2](https://wiki.dfrobot.com/sen0592/docs/19640)

| SEN0591 range level | Real-time distance, ms | Processed distance, ms |
| ---: | ---: | ---: |
| 1 | 15–80 | 190–500 |
| 2 | 20–90 | 230–550 |
| 3 | 25–100 | 250–600 |
| 4 | 35–110 | 280–650 |

## Beam angle and power-supply noise

For SEN0592, register `0x0208` lists a single angle and two dual-angle
values, horizontal and vertical. Both register tables
give about 45° horizontal at level 1. The product specification gives a
horizontal range beginning at 40°. The Chinese overview separately gives
30–60° without specifying an axis. The register map keeps the per-level
figures and their axis labels. The manuals leave the relationship between
the single- and dual-angle descriptions unexplained. [SEN0592 English table 2](https://wiki.dfrobot.com/sen0592/docs/19640) · [SEN0592 Chinese overview and table 2](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m) · [SEN0592 product specification](https://www.dfrobot.com/product-2729.html)

DFRobot gives the same supply examples for noise suppression register
`0x021A` on both models. The manuals describe progressively greater effects
on the beam angle but give no direction or magnitude for the change.
[SEN0591 table 2](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0592 table 2](https://wiki.dfrobot.com/sen0592/docs/19640)

| Level | Supply or operating condition |
| ---: | --- |
| 1 | Battery supply |
| 2 | High-frequency supply noise, such as USB power |
| 3 | USB supply over a longer cable |
| 4 | Switching power supply |
| 5 | Switching supply with additional environmental interference. DFRobot discourages routine use. |

## Configuration on the RS485 models

- **Address and baud rate.** Use one addressed device when changing either
  setting. The address-change example acknowledges at the original address.
  Both Chinese editions give all eight baud codes for `0x0201`. Both
  English editions lose the mapping at code `0x0005`. The packet-duration
  table numbers its rows independently. Examples 5 and 6 confirm the
  mapping for 9600 baud. The register takes effect immediately, but the
  examples leave the baud rate used for the write acknowledgement
  unspecified. Verify the new link by reading at the new settings.
  Persistence across a power cycle remains unknown. [SEN0591 Chinese examples 4–6](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m) · [SEN0592 Chinese examples 4–6](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m) · [Reference frames](protocol/modbus_vectors.csv)
- **Switch output.** Registers `0x0205` and `0x0206` specify polarity and
  a distance threshold in switch mode. The Chinese SEN0591 table resolves
  the duplicated lower bound in English. The defined polarity applies
  strictly below the threshold. The manuals leave equality unspecified and
  the RS485 pinouts show only supply, ground, A and B. [SEN0591 English table 2](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0591 Chinese table 2 and pinout](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m) · [SEN0592 Chinese table 2 and pinout](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m)
- **UART output units.** DFRobot limits `0x0209` to the automatic and
  controlled UART output modes. Decode each RS485 measurement using its
  own unit in table 1. The two SKU manuals provide no command to select
  UART or switch mode on the four-wire RS485 sensor. [SEN0591 tables 1 and 2](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0592 tables 1 and 2](https://wiki.dfrobot.com/sen0592/docs/19640)
