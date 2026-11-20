# SEN059x – vývojové reference a podklady

**SEN0591 (3m) / SEN0592 (5m), DFRobot; aktualizace 20. 11. 2026.**

Tento balík je výzkumná a vývojová příprava knihovny `SEN059x` pro Arduino/PlatformIO a ESP-IDF. **Nejde o hotovou knihovnu SEN059x ani o hardwarově ověřené měření.**

## Obsah

- [Technická reference](SEN059x_reference.md) uvádí parametry a rozdíly mezi zdroji. [Původní PDF přehled](SEN059x_reference.pdf) zachycuje stav před porovnáním anglické a čínské dokumentace.
- [Tabulka registrů](protocol/registers.csv), [ověřené rámce](protocol/modbus_vectors.csv) a [poznámky k RTU](protocol/Modbus_RTU_notes.md) pokrývají adresy, kódování a časování obou modelů.
- [Návrh architektury](../ARCHITECTURE.md) vychází z knihoven SHZK-PT a VibWire-108 a jejich použití ve FieldCore-node.
- [USB–RS485 sonda](../../examples/python/) a [příklad pro ESP32-S3](../../examples/platformio-esp32/) čtou hodnoty ze senzoru. [Offline testy](../../test/test_vectors.py) kontrolují CRC, konstrukci dotazů a povolené adresy.
- [Seznam originálů](source_links/ORIGINAL_SOURCES.md) a [průzkum katalogů](source_links/registry-survey.md) popisují původ dokumentů a dostupné knihovny. [Stahovací skript](../../scripts/fetch_originals.py) pracuje se [seznamem zdrojů](source_links/sources.json) a ukládá soubory do [originals/](originals/).

## Základní zjištění

- Oba senzory používají **Modbus RTU** na **RS485**, default 115200, 8N1, adresa 1, FC03 čtení a FC06 zápis.
- Adresy měřicích registrů jsou shodné. Modely se liší jmenovitým dosahem, napájením, úhly paprsku, rozsahem prahu spínání a úrovněmi dosahu.
- SEN0592 pokrývá **30–5000 mm** při napájení **3,3–24 V**. Okamžité měření trvá podle výrobce přibližně **15–140 ms**. Pro zpracovanou hodnotu uvádí anglická wiki **190–750 ms**, čínská **300–750 ms**. [Anglický popis registrů](https://wiki.dfrobot.com/sen0592/docs/19640) · [Čínský popis registrů](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m)
- SEN0591: **20–3000 mm** podle produktového přehledu, napájení **3,3–12 V**. Konfigurační dokument pro úrovně dosahu uvádí až 350 cm; jde o rozpor zdroje, který je nutné ověřit na fyzickém zařízení.
- Barvy vodičů SEN059x: červená VCC, černá GND, žlutá RS485 B, bílá RS485 A.

## Jak získat originální soubory

Příkazy v této příručce spouštěj z kořenového adresáře repozitáře. Pro stažení zdrojů použij Python 3.9+ a připojení k internetu.

```powershell
python scripts/fetch_originals.py
```

Skript uloží kompletní originální HTML stránky DFRobot, originální PDF dokumenty Modbus Organization a ZIP archivní kopie otevřených knihoven. Pokud URL vrátí chybu nebo vyžaduje přihlášení, zaznamená to a archiv nebude vykazovat jako stažený. Přehled všech zdrojů je v `source_links/sources.json`.

Výrobce na stránkách obou produktů odkazuje na wiki s parametry, výkresy a příklady. Popisy jednotlivých dokumentů a výsledky hledání jsou v [seznamu zdrojů](source_links/ORIGINAL_SOURCES.md).

## Rychlý test

```powershell
python -m unittest discover -s test -v
python examples/python/sen059x_probe.py --self-test
pip install pyserial
python examples/python/sen059x_probe.py --port COM16 --address 1 --mode realtime
```

`COM16` je pouze příklad. Pro každou fyzickou jednotku je nutné zvolit skutečný port a **před společným připojením čtyř jednotek přidělit unikátní adresy**. Příklady záměrně neprovádějí změny EEPROM/adresy/baudrate.

## Poznámky pro návrh knihovny

Knihovna má sestavit dotaz do bufferu volajícího a zkontrolovat přijatý rámec. Aplikace vlastní UART, řídí směr přenosu, timeouty a opakování. Model určí konfigurace zařízení. Vzory pro testy jsou v [tabulce rámců](protocol/modbus_vectors.csv).

## Licenční a původové informace

Texty balíku a jeho testovací příklady vznikly pro tento projekt. Knihovny a dokumentace třetích stran mají vlastní licenční podmínky. Přehled licencí a odkazy na původní soubory jsou v [seznamu zdrojů](source_links/ORIGINAL_SOURCES.md).
