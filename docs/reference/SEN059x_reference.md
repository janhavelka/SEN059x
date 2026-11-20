# SEN059x – český průvodce technickou referencí

DFRobot dodává dvě ultrazvuková čidla s rozhraním RS485. SEN0591 je varianta
pro 3 m a SEN0592 pro 5 m. Před připojením zjisti SKU ze štítku a podle něj
zvol napájecí napětí i montážní výkres. Obě jednotky používají stejná čísla
měřicích registrů, takže odpověď na čtení sama model neurčí.
[Parametry, vodiče a montážní výkresy](hardware.md)

| Co potřebuješ zjistit | Kde je údaj veden |
| --- | --- |
| Kdy se spustí měření, jak dlouho trvá a co ovlivní nastavení dosahu, úhlu nebo filtrace | [Měření a konfigurace podle modelu](measurement-and-configuration.md) |
| Adresa registru, jednotka, povolené hodnoty, výchozí nastavení a rozdíly mezi modely | [Tabulka registrů](protocol/registers.csv) |
| Sestavení požadavku, kontrola odpovědi, výjimky, CRC a časování RS485 | [Protokol Modbus RTU](protocol/Modbus_RTU_notes.md) |
| Skutečné bajty z dokumentace výrobce a jejich převod | [Příklady rámců](protocol/modbus_vectors.csv) |
| Rozpory mezi anglickým a čínským návodem a otázky pro měření na zařízení | [Výsledek porovnání dokumentace DFRobot](README.md#manufacturer-cross-check) |
| Původní návody, ukázky Arduino, výkresy a normy ke čtení offline | [Přehled zdrojů](source_links/ORIGINAL_SOURCES.md) |

Pro první připojení jsou k dispozici [USB–RS485 sonda](../../examples/python/README.md)
a [příklad pro ESP32-S3](../../examples/platformio-esp32/README.md). Při práci s
více senzory přiděl každému vlastní adresu a nech jednu aplikaci řídit pořadí
dotazů, směr přenosu a timeout. Před opakováním dotazu musí aplikace vyřešit
případnou opožděnou odpověď. [Návrh pro FieldCore-node](../ARCHITECTURE.md)

[Původní český přehled v PDF](SEN059x_reference.pdf) zachycuje stav při předání
podkladů. Aktuální hodnoty jsou v tematických odkazech výše. Kontrolu
uložených originálů a stažení doplňkových materiálů popisuje [návod](README.md#offline-use).
