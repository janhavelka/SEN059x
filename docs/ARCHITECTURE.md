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

Odkazy výše vedou na větev `rodtester`. Na výchozí větvi `main` je pro TX
k dispozici 8 bajtů a příjem se ukončuje délkou nebo příponou. Větev
`rodtester` má 21 bajtů pro TX a přidává režim RTU s rozlišením průkazné
a nejednoznačné odpovědi. Samotná shoda osmi bajtů FC06 s požadavkem
neodliší odpověď čidla od místního echa. Implementační zadání ověřuje obě
rozhraní a zvlášť sleduje potvrzení zápisu a přečtenou hodnotu na novém
spojení. [Rozhraní na main](https://github.com/janhavelka/FieldCore-node/blob/eccdbdcc0e9ce22beee4f54494c6c179a320d3aa/include/TunnelMonitor/rs485/Rs485OwnerTransaction.h) · [Zadání integračního testu](prompts/13-fieldcore-consumer.md) · [Názvy veřejného API](prompts/CONVENTIONS.md)

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
- Začít čtením čtyř měřicích registrů. Výsledek má odlišit timeout, chybu CRC, cizí adresu, neočekávanou odpověď, výjimku Modbus a neplatnou hodnotu. Po přidání zápisů ověřit povolený rozsah a potvrdit nové nastavení zpětným čtením.
- Čtení teploty: výrobce dokumentuje `uint16_t` v 0,1 °C, ale nezveřejňuje kódování záporných hodnot. Než se zaimplementuje interpretace pro teploty pod nulou, testovat se senzorem při záporné teplotě.

Pro samostatné příklady lze porovnat čtyři knihovny Modbus uvedené v přehledu
zdrojů. Ve FieldCore-node sestavuje přenosy vlastník sběrnice. Přenosná část
knihovny SEN059x má připravit požadavek FC03/FC06 a ověřit odpověď i bez
závislosti na konkrétním sériovém ovladači. [Porovnávané knihovny a licence](reference/source_links/ORIGINAL_SOURCES.md#supporting-references) · [Formát rámců](reference/protocol/Modbus_RTU_notes.md#frame-layout)

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
