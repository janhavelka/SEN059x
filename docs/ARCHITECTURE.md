# Doporučení pro repozitář `SEN059x`

## Jak FieldCore-node používá knihovny RS485

Nejbližší vzor je SHZK-PT. Veřejné hlavičky má v `include/SHZK_PT/`,
implementaci v `src/`, testy v `test/` a obsluhu UART v příkladech.
VibWire-108 používá stejné členění. [SHZK-PT](https://github.com/janhavelka/SHZK-PT/tree/59d76c502a90bef8038ced2b72a2d50df98184e9) · [VibWire-108](https://github.com/janhavelka/VibWire-108/tree/b907e05f2e15bce9e4d9820903dc5a0d2d90dfd2)

MotorControl-RS nabízí další příklad modelově specifických rámců RTU a
oddělení komunikace od řízení zařízení. Názvy rodiny a explicitní volba
varianty navazují na TMP11x. [MotorControl-RS](https://github.com/janhavelka/MotorControl-RS/tree/08a88ebb422d1a9f8df9c3036faabf82b3a0983e) · [TMP11x](https://github.com/janhavelka/TMP11x)

V profilu TunnelMonitor je SHZK-PT připnutá na `v3.0.0` a VibWire-108 na
`v1.2.0`. Modul `ShzkDeviceModule` používá veřejnou hlavičku knihovny,
sestaví požadavek do paměti transakce a při zpracování odpovědi předá
parseru očekávanou adresu a počet registrů. Vlastník sběrnice ve firmwaru
řídí UART, směr přenosu, timeout a pořadí dotazů. [Profily FieldCore-node](https://github.com/janhavelka/FieldCore-node/blob/c40ea8341edf8d665460b902cafefbe857a7f73f/config/build_profiles.json) · [ShzkDeviceModule](https://github.com/janhavelka/FieldCore-node/blob/c40ea8341edf8d665460b902cafefbe857a7f73f/src/rs485/ShzkDeviceModule.cpp) · [Transakce vlastníka sběrnice](https://github.com/janhavelka/FieldCore-node/blob/c40ea8341edf8d665460b902cafefbe857a7f73f/include/TunnelMonitor/rs485/Rs485OwnerTransaction.h)

Výchozích 115200 Bd a 8N1 obou senzorů odpovídá formátu, který FieldCore-node
přijímá. Jeho `rs485SerialFormatValid` podporuje také 9600, 19200, 38400 a
57600 Bd. Z osmi rychlostí senzoru tedy 4800, 14400 a 76800 Bd vyžadují
změnu na straně tohoto spotřebitele. [Kontrola formátu ve FieldCore-node](https://github.com/janhavelka/FieldCore-node/blob/c40ea8341edf8d665460b902cafefbe857a7f73f/include/TunnelMonitor/rs485/Rs485OwnerTransaction.h) · [Kódy rychlostí senzoru](reference/protocol/registers.csv)

## Navržené softwarové oddělení

```
FieldCore-node / samostatný příklad
  ├── SEN059x       sestavení dotazu, kontrola odpovědi, jednotky a profil modelu
  └── RS485 owner   fronta dotazů, časování, UART a řízení DE/RE
```

- Název projektu `SEN059x`, veřejná hlavička `<SEN059x/SEN059x.h>`, jmenný prostor `SEN059x`.
- `DeviceModel::SEN0591` / `DeviceModel::SEN0592`, model explicitně z konfigurace.
- Žádné hardcoded GPIO, `HardwareSerial`, Arduino, FreeRTOS nebo ESP-IDF funkce v core.
- Podle vzoru SHZK-PT má aplikace předat knihovně buffer pro sestavení dotazu a po přijetí odpovědi předat bajty ke kontrole. Stejné rozhraní může použít samostatný příklad i sdílená sběrnice ve FieldCore-node.
- Odpověď FC03 vrací data bez adresy čteného registru. U jednoho senzoru mají odpovědi na jedno slovo vzdálenosti, teploty a času echa stejný tvar. Aplikace musí uchovat kontext dotazu a po timeoutu vyřešit pozdní odpověď před dalším měřením.
- Rozlišit dobu dokončení měření od doby přenosu rámce. Obsah `distance` publikovat jen při validním novém výsledku.
- Velmi důležité pro čtyři jednotky: jedna UART/RS485 sběrnice má jednoho vlastníka a maximálně jeden aktivní dotaz. Všechny senzory na stejné lince musí mít unikátní adresy.
- Žádné automatické skenování ve výchozím režimu, žádné implicitní zápisy konfigurace, žádné přepisování adres bez explicitního potvrzení uživatele.
- Čtení teploty: výrobce dokumentuje `uint16_t` v 0,1 °C, ale nezveřejňuje kódování záporných hodnot. Než se zaimplementuje interpretace pro teploty pod nulou, testovat se senzorem při záporné teplotě.

## Kandidáti na vrstvy Modbus

1. `DFRobot_RTU` – vhodný jako porovnávací vendor příklad, Arduino orientovaný, nepřenášet bez rozmyslu jeho závislost `Stream` do čistého core.
2. `ModbusMaster` – Arduino RTU client s DE/RE callbacky; nejjednodušší na laboratorní oživení.
3. `modbus-esp8266` – více transportů, pro minimalistickou senzorovou knihovnu přináší více funkcí, než je potřeba.
4. `esp-modbus` – referenční vendor implementace pro ESP-IDF; dobrá do integračního příkladu, ne jako nutná závislost C++ core.
5. Vlastní úzce vymezené RTU FC03/FC06 bez závislostí – možné, pokud se výslovně kontroluje framing, timing, CRC, Modbus exception, broadcast, burst, multi-device a cancellation.

## Doporučené první unit testy

- FC03 pro 0x0100, 0x0101, 0x0102, 0x010A
- CRC správné, CRC chybné; chybné ID a chybné FC
- Odpověď zkrácená, echo zápisu, exception, poškozená délka
- Rozdílné mezní hodnoty SEN0591/SEN0592, zejména angle/range
- Změna adresy / baudrate nesmí proběhnout implicitně ani při automatickém recovery
- Opožděná odpověď po timeoutu nesmí být uznána jako výsledek jiného senzoru
- Teplota a echo: správné jednotky a vyznačené nejasnosti
- Aplikace s více senzory: sériový owner a periodická výměna podle time budgetu

Toto je doporučená architektura, nikoli potvrzení, že existuje připravený a otestovaný SEN059x driver.
