# Původní zdroje a odkazy ke stažení

Ověřeno pro přípravu knihovny SEN059x dne 20. 11. 2026. [Katalog stažení](sources.json) uvádí veřejné URL, velikosti a SHA-256 přiložených originálů. Samostatný [průzkum katalogů a registrů knihoven](registry-survey.md) uvádí prohledané databáze a nalezené příklady.

## Lokální kopie DFRobot

| Podklad | SEN0591 | SEN0592 |
|:--|:--|:--|
| Přehled a zapojení vývodů | [Wiki](../originals/dfrobot/SEN0591_overview.html) | [Wiki](../originals/dfrobot/SEN0592_overview.html) |
| Anglická specifikace registrů | [Modbus](../originals/dfrobot/SEN0591_modbus_reference.html) | [Modbus](../originals/dfrobot/SEN0592_modbus_reference.html) |
| Čínská specifikace s úplnou tabulkou kódů baudrate | [Wiki CN](../originals/dfrobot/SEN0591_wiki_CN.html) | [Wiki CN](../originals/dfrobot/SEN0592_wiki_CN.html) |
| Parametry výrobku a IP67 | [Katalog](../originals/dfrobot/SEN0591_product.html) | [Katalog](../originals/dfrobot/SEN0592_product.html) |
| Arduino s převodníkem RS485 | [Ukázka](../originals/dfrobot/SEN0591_arduino_example.html), [zapojení](../originals/dfrobot/SEN0591_arduino_wiring.webp) | [Ukázka](../originals/dfrobot/SEN0592_arduino_example.html), [zapojení](../originals/dfrobot/SEN0592_arduino_wiring.webp) |
| Rozměry | [Výkres EN](../originals/dfrobot/SEN0591_dimension.jpg), [výkres CN](../originals/dfrobot/SEN0591_outline_CN.png) | [Výkres EN](../originals/dfrobot/SEN0592_dimension.jpg), [CN 1](../originals/dfrobot/SEN0592_outline_CN_1.png), [CN 2](../originals/dfrobot/SEN0592_outline_CN_2.png) |
| Montážní otvor | [Výkres](../originals/dfrobot/SEN0591_mounting.png) | [Výkres](../originals/dfrobot/SEN0592_mounting.png) |

## DFRobot – přímo SEN0592

1. [SEN0592 – produkt, elektrické a mechanické parametry](https://wiki.dfrobot.com/sen0592/)
2. [SEN0592 – kompletní Modbus registry a ukázkové rámce](https://wiki.dfrobot.com/sen0592/docs/19640)
3. [SEN0592 – originální Arduino ukázka](https://wiki.dfrobot.com/sen0592/docs/19641)
4. [SEN0592 – mechanický rozměrový obrázek](https://dfimg.dfrobot.com/wiki/19426/SEN0592_ultrasonic-obstacle-avoidance-sensor_dimension_V1.0.jpg)
5. [SEN0592 – katalogový list výrobku v e-shopu výrobce](https://www.dfrobot.com/product-2729.html)
6. [SEN0592 – původní čínská dokumentace](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m)

## DFRobot – kompatibilní model SEN0591

1. [SEN0591 – přehled](https://wiki.dfrobot.com/sen0591/)
2. [SEN0591 – Modbus registry](https://wiki.dfrobot.com/sen0591/docs/19645)
3. [SEN0591 – Arduino příklad](https://wiki.dfrobot.com/sen0591/docs/19646)
4. [SEN0591 – mechanický rozměrový obrázek](https://dfimg.dfrobot.com/wiki/19424/SEN0591_ultrasonic-ranging-sensor_dimension_V1.0.jpg)
5. [SEN0591 – katalogová stránka výrobce](https://www.dfrobot.com/product-2728.html)
6. [SEN0591 – původní čínská dokumentace](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m)

## Specifikace protokolu – oficiální PDF

1. FC03, FC06, výjimky a PDU. [Application Protocol V1.1b3](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf), [lokální PDF](../originals/modbus/Modbus_Application_Protocol_V1_1b3.pdf).
2. RTU rámce, CRC, časování a RS485. [Serial Line Guide V1.02](https://www.modbus.org/file/secure/modbusoverserial.pdf), [lokální PDF](../originals/modbus/Modbus_over_serial_line_V1_02.pdf).
3. Starší revize sériového protokolu pro srovnání. [Serial Line Guide V1.0](https://www.modbus.org/file/secure/modbusoverseriallegacy.pdf), [lokální PDF](../originals/modbus/Modbus_over_serial_line_V1_LEGACY.pdf).

## Existující knihovny a příklady

| Název | Systém | Licence | K čemu použít |
|:--|:--|:--|:--|
| [DFRobot_RTU](https://github.com/DFRobot/DFRobot_RTU) | Arduino, ESP32 Serial | MIT | Nejbližší obecná knihovna výrobce, umí Modbus FC03/FC06. [PlatformIO](https://registry.platformio.org/libraries/dfrobot/DFRobot_RTU). |
| [4-20ma/ModbusMaster](https://github.com/4-20ma/ModbusMaster) | Arduino | Apache-2.0 | Jednoduchý Modbus RTU master, DE/RE callbacks. |
| [emelianov/modbus-esp8266](https://github.com/emelianov/modbus-esp8266) | Arduino/ESP32 | BSD-3-Clause | RTU klient/server, asynchronnější přístup. |
| [Espressif esp-modbus](https://github.com/espressif/esp-modbus) | ESP-IDF | Apache-2.0 a licence částí | Doporučená referenční implementace pro native ESP-IDF, příklady Serial Master. |
| [pyserial](https://pyserial.readthedocs.io/) | PC / Python | licence projektu | Přímé testování USB–RS485, bez nutnosti další Modbus knihovny. |
| [pymodbus](https://github.com/pymodbus-dev/pymodbus) | PC / Python | podmínky v repozitáři | Alternativní PC nástroj pro Modbus zařízení. |

Přímo dedikovanou publikovanou knihovnu DFRobot pro SEN0592 jsem ve zdrojích nepotvrdil; původní ukázka na wiki používá vlastní příkazy po `Serial`, bez samostatně instalované knihovny.

## Další technicky související senzory – **nejsou** automaticky kompatibilní

- [SEN0519 URM15 RS485 Modbus](https://wiki.dfrobot.com/sen0519/) – jiný profil.
- [SEN0492 RS485 laser](https://wiki.dfrobot.com/sen0492/) – jiná technologie a mapa registrů.
- [SEN0246 URM08 RS485](https://wiki.dfrobot.com/sen0246/) – dokumentace ukazuje rámce začínající `55 AA`, není přímo kompatibilní s Modbus RTU SEN0592.

## Původ dokumentace

U obou senzorů výrobce zveřejňuje technické parametry a protokol jako webovou dokumentaci. V prozkoumaných katalozích se samostatný technický datasheet výrobce v PDF nepodařilo ověřit. Souhrnné PDF přiložené v původním balíku vzniklo v rámci projektu. U registrů se řiď také opravami a zaznamenanými rozdíly jazykových verzí v aktuální technické referenci projektu.
