# PlatformIO ESP32-S3 UART/RS485 proof of concept

Příklad je nově napsaný read-only program pro SEN0591/SEN0592, **není hardwarově otestovaný**. Používá default 115200/8N1/slave=1 a registr `0x0101`. Předpokládá RS485 half-duplex transceiver s DE a /RE elektricky spojeným na jediný GPIO (vysoká úroveň = vysílání).

V `platformio.ini` uprav `board`, RX, TX, DE podle skutečné desky; výchozí GPIO16/17/4 jsou **pouze ilustrace**, nikoliv schéma tvého zařízení. Připoj VCC/GND podle modelu a RS485 A/B na oddělený transceiver. Bez transceiveru nelze připojit RS485 vedení přímo na ESP32.

Následující příkazy spouštěj z adresáře `examples/platformio-esp32/`.

```sh
pio run
pio run -t upload
pio device monitor
```

Z kořene repozitáře lze tentýž příklad sestavit příkazem `pio run -e esp32-s3-devkitc-1`. Ve Windows lze použít `.\scripts\pio.cmd run`.

Po zapnutí program vyčká 1200 ms a poté cyklicky čte aktuální vzdálenost v mm. Mezi ukončením dotazu a zahájením dalšího čeká 500 ms. Příklad obsluhuje UART i směr přenosu sám. Ve sdílené instalaci zajišťuje obě úlohy vlastník sběrnice ve FieldCore-node.
