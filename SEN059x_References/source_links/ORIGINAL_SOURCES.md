# Původní zdroje a odkazy ke stažení

Ověřeno pro přípravu knihovny SEN059x dne 20. 11. 2026. Podrobnosti a strojově čitelná tabulka v `sources.json`. Soubor níže není náhradou licencovaných originálů.

## DFRobot – přímo SEN0592

1. [SEN0592 – produkt, elektrické a mechanické parametry](https://wiki.dfrobot.com/sen0592/)
2. [SEN0592 – kompletní Modbus registry a ukázkové rámce](https://wiki.dfrobot.com/sen0592/docs/19640)
3. [SEN0592 – originální Arduino ukázka](https://wiki.dfrobot.com/sen0592/docs/19641)
4. [SEN0592 – mechanický rozměrový obrázek](https://dfimg.dfrobot.com/wiki/19426/SEN0592_ultrasonic-obstacle-avoidance-sensor_dimension_V1.0.jpg)
5. [SEN0592 – katalogový list výrobku v e-shopu výrobce](https://www.dfrobot.com/product-2729.html)

## DFRobot – kompatibilní model SEN0591

1. [SEN0591 – přehled](https://wiki.dfrobot.com/sen0591/)
2. [SEN0591 – Modbus registry](https://wiki.dfrobot.com/sen0591/docs/19645)
3. [SEN0591 – Arduino příklad](https://wiki.dfrobot.com/sen0591/docs/19646)
4. [SEN0591 – mechanický rozměrový obrázek](https://dfimg.dfrobot.com/wiki/19424/SEN0591_ultrasonic-ranging-sensor_dimension_V1.0.jpg)

## Specifikace protokolu – oficiální PDF

1. [Modbus Application Protocol Specification V1.1b3 (PDF)](https://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf) – FC03, FC06, výjimky a PDU.
2. [Modbus Serial Line Protocol and Implementation Guide V1.02 (PDF)](https://www.modbus.org/file/secure/modbusoverserial.pdf) – RTU rámce, CRC, časování, RS485. Pokud oficiální přístup vyžaduje autorizaci, použít [rozcestník výrobce standardu](https://www.modbus.org/modbus-specifications).
3. [Modbus over Serial Line V1 (PDF, starší)](https://www.modbus.org/docs/Modbus_over_serial_line_V1.pdf) – pouze jako fallback, ne nová norma.

## Existující knihovny a příklady

| Název | Systém | Licence | K čemu použít |
|:--|:--|:--|:--|
| [DFRobot_RTU](https://github.com/DFRobot/DFRobot_RTU) | Arduino, ESP32 Serial | MIT | Nejbližší obecná knihovna výrobce, umí Modbus FC03/FC06. [PlatformIO](https://registry.platformio.org/libraries/dfrobot/DFRobot_RTU). |
| [4-20ma/ModbusMaster](https://github.com/4-20ma/ModbusMaster) | Arduino | podmínky v repozitáři | Jednoduchý Modbus RTU master, DE/RE callbacks. |
| [emelianov/modbus-esp8266](https://github.com/emelianov/modbus-esp8266) | Arduino/ESP32 | BSD | RTU klient/server, asynchronnější přístup. |
| [Espressif esp-modbus](https://github.com/espressif/esp-modbus) | ESP-IDF | Apache-2.0 a licence částí | Doporučená referenční implementace pro native ESP-IDF, příklady Serial Master. |
| [pyserial](https://pyserial.readthedocs.io/) | PC / Python | licence projektu | Přímé testování USB–RS485, bez nutnosti další Modbus knihovny. |
| [pymodbus](https://github.com/pymodbus-dev/pymodbus) | PC / Python | podmínky v repozitáři | Alternativní PC nástroj pro Modbus zařízení. |

Přímo dedikovanou publikovanou knihovnu DFRobot pro SEN0592 jsem ve zdrojích nepotvrdil; původní ukázka na wiki používá vlastní příkazy po `Serial`, bez samostatně instalované knihovny.

## Další technicky související senzory – **nejsou** automaticky kompatibilní

- [SEN0519 URM15 RS485 Modbus](https://wiki.dfrobot.com/sen0519/) – jiný profil.
- [SEN0492 RS485 laser](https://wiki.dfrobot.com/sen0492/) – jiná technologie a mapa registrů.
- [SEN0246 URM08 RS485](https://wiki.dfrobot.com/sen0246/) – dokumentace ukazuje rámce začínající `55 AA`, není přímo kompatibilní s Modbus RTU SEN0592.

## Co v balíku **není**

Nenalezen ověřitelný samostatný originální PDF datasheet `SEN0592.pdf`. DFRobot poskytuje webovou produktovou stránku, protokolovou referenci a obrázek rozměrů. Tato sada obsahuje nově vytvořené PDF shrnutí a skript pro získání oficiálních PDF Modbus standardu, nikoli nepravdivě pojmenované PDF senzoru.

Pozor: webový dokument DFRobot obsahuje překlepy/chybějící bajt u nastavení adresy, chybné označení FC v řádku zápisu a nejasnosti broadcastu. Výslovně zdokumentováno v `docs/SEN059x_reference.md`.
