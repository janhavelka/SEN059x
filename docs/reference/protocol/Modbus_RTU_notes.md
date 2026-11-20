# Modbus RTU on SEN0591 and SEN0592

## Serial settings and addressing

Both sensors ship at **115200 baud, 8 data bits, no parity, 1 stop bit, address 1**. Configure the UART for this 8N1 format. The generic Modbus serial guide specifies even parity by default and two stop bits when parity is omitted. DFRobot explicitly specifies one stop bit for these devices. [SEN0591 parameters](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0592 parameters](https://wiki.dfrobot.com/sen0592/docs/19640) · [Serial Line V1.02, §2.5.1](../originals/modbus/Modbus_over_serial_line_V1_02.pdf#page=12)

Use a unique address in **1–247** on a shared bus. DFRobot lists 1–254 as configurable and names `0xFF` as broadcast. Standard Modbus reserves 248–255 and assigns address 0 to broadcast writes, which have no reply. The supplied probes use unicast. Behavior at either broadcast address has no sensor capture in the supplied materials. [SEN0591 register 0x0200](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m) · [SEN0592 register 0x0200](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m) · [Serial Line V1.02, §§2.1–2.2](../originals/modbus/Modbus_over_serial_line_V1_02.pdf#page=7)

## Frame layout

Each row gives a whole RTU frame in transmission order. `A` is the device address in one byte. `reg`, `count`, `value` and each returned word occupy two bytes, high byte first. Use the hexadecimal register address directly from the [register map](registers.csv).

| Transaction | Fields on the wire | Frame size |
| --- | --- | --- |
| FC03 read request | `A 03 reg count CRClo CRChi` | 8 bytes |
| FC03 successful reply | `A 03 byteCount words… CRClo CRChi` | 5 + 2 × count bytes. A one-word read returns 7. |
| FC06 write request and successful reply | `A 06 reg value CRClo CRChi` | 8 bytes each. The reply echoes the written register and value. |
| Exception reply | `A 83 exceptionCode CRClo CRChi` for FC03, or `A 86 exceptionCode CRClo CRChi` for FC06 | 5 bytes |

FC03 defines counts of 1–125 consecutive holding registers in the standard. The device manuals demonstrate one-word reads. They give no device-specific block-read guarantee across registers or gaps. The standard lists exception codes 1 (function), 2 (address), 3 (value), and 4 (device failure) for FC03/06. DFRobot provides no example of an exception returned by these SKUs. [Application Protocol V1.1b3, §§6.3, 6.6, 7](../originals/modbus/Modbus_Application_Protocol_V1_1b3.pdf#page=15)

CRC-16/MODBUS covers the address through the last data byte. Start at `0xFFFF`, XOR each byte into the low byte of the accumulator, then right-shift eight times, XORing `0xA001` whenever the outgoing bit is one. Send the low CRC byte first. DFRobot’s Arduino helper swaps the final accumulator for its comparison with the received pair. [Serial Line V1.02, Appendix B](../originals/modbus/Modbus_over_serial_line_V1_02.pdf#page=39) · [SEN0591 sketch](https://wiki.dfrobot.com/sen0591/docs/19646) · [SEN0592 sketch](https://wiki.dfrobot.com/sen0592/docs/19641)

### Published examples and corrections

The [six manufacturer frame pairs](modbus_vectors.csv) appear in the same order in both Chinese manuals. Each row records the published request and reply plus its decoded value. They are documentation examples. Their CRCs can be reproduced offline. [SEN0591 CN, examples 1–6](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m) · [SEN0592 CN, examples 1–6](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m)

- Both languages label the write-layout headings `03`. The device function table, write examples 4 and 6, and Modbus §6.6 establish `06`.
- In both English editions, example 4 drops the high byte of the new address value from both request and reply. The Chinese editions print all eight bytes with the same CRC. The reply shown uses the old device address. Example 6 similarly echoes a baud-rate write but gives no evidence of the baud rate used to send that acknowledgement. [SEN0591 EN](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0592 EN](https://wiki.dfrobot.com/sen0592/docs/19640) · [Application Protocol V1.1b3, §6.6](../originals/modbus/Modbus_Application_Protocol_V1_1b3.pdf#page=19)

## Wire time and measurement time

DFRobot gives the same eight single-packet durations in all four SKU and language editions. It leaves the packet length unspecified. The three right-hand columns give the actual serialization time for the frames above, calculated as `1000 × bytes × 10 / baud` for 8N1. The row numbers 1–8 in the manuals are table positions. [Baud configuration codes](registers.csv) · [SEN0591 timing table](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0592 timing table](https://wiki.dfrobot.com/sen0592/docs/19640)

| Baud | DFRobot single packet, ms | 8-byte request / FC06 reply, ms | 7-byte FC03 reply, ms | 5-byte exception, ms |
| ---: | ---: | ---: | ---: | ---: |
| 4800 | 16 | 16.667 | 14.583 | 10.417 |
| 9600 | 8 | 8.333 | 7.292 | 5.208 |
| 14400 | 5.6 | 5.556 | 4.861 | 3.472 |
| 19200 | 4 | 4.167 | 3.646 | 2.604 |
| 38400 | 2.4 | 2.083 | 1.823 | 1.302 |
| 57600 | 1.6 | 1.389 | 1.215 | 0.868 |
| 76800 | 0.8 | 1.042 | 0.911 | 0.651 |
| 115200 | 0.6 | 0.694 | 0.608 | 0.434 |

At 76800 baud even a seven-byte response takes longer than the published 0.8 ms. Size a read deadline for the actual request and reply, sensor response, turnaround and end-of-frame detection. Four sequential real-time reads at the general 140 ms upper estimate already use 560 ms of sensor time. Four processed reads at 750 ms use 3 s before serial and scheduling overhead. DFRobot gives no SKU-specific execution time for a configuration write. [Measurement times and range settings](../measurement-and-configuration.md#response-times-and-range-levels)

At or below 19200 baud, the RTU guide requires at least 3.5 character times of silence between frames. A gap exceeding 1.5 character times inside a frame makes it incomplete. Above 19200 baud, the guide recommends fixed timers of 750 µs (inter-character timeout) and 1.750 ms (inter-frame gap). These timers govern frame boundaries once bytes are exchanged. [Serial Line V1.02, §2.5.1.1](../originals/modbus/Modbus_over_serial_line_V1_02.pdf#page=13)

## Receiving a reply on a shared bus

Keep one request outstanding. Retain its device address, function, register and quantity or written value. Validate the complete frame, CRC and expected address/function. For FC03, check that the returned byte count is twice the requested word count. For FC06, compare the echoed fields and account for local adapter echo. Check exceptions as five-byte frames before decoding a normal reply. [Application Protocol V1.1b3, §§6.3, 6.6, 7](../originals/modbus/Modbus_Application_Protocol_V1_1b3.pdf)

FC03 replies omit the starting register. Two one-word replies from the same sensor can therefore look identical on the wire even when their units differ. After a timeout, let the bus owner settle the outstanding exchange and handle a late frame before issuing a new request. Retrying a distance read can start another measurement. After a lost write acknowledgement, verify the setting at the expected endpoint before issuing another write. [FieldCore-node ownership notes](../../ARCHITECTURE.md)

### Timing in DFRobot’s Arduino sketches

Both sketches wait 100 ms before each request, send one FC03 read of `0x0101`, then wait 10 ms before reading the UART. They read the address, function and byte count separately, then the final four bytes together. Each of those four receive calls starts its own 500 ms timer. The outer loop retries until a seven-byte reply passes the checks. After printing a successful result the sketch waits another 500 ms. These delays describe the demo’s polling rate. The register tables supply the device response times. [SEN0591 sketch](https://wiki.dfrobot.com/sen0591/docs/19646) · [SEN0592 sketch](https://wiki.dfrobot.com/sen0592/docs/19641)

DFRobot uses a 5 V DFR0259 shield in AUTO direction mode. Set its RUN/COMPILE switch to OFF for upload and ON for operation. On the Uno, the example sends both binary requests and printable distances through the same UART. The last request byte `0x36` displays as the character `6`, visible before “Distance” in the shared example screenshot. The timestamp spacing there is roughly 0.70–0.75 s between printed results. For a multidrop installation, send console output on a separate interface and give each bus transaction a deadline. [Shield specification](../originals/dfrobot/DFR0259_overview.html) · [Manufacturer’s serial monitor](../originals/dfrobot/SEN059x_arduino_result.webp)
