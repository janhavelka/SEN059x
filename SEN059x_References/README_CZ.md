# SEN059x – vývojové reference a podklady

**SEN0591 (3m) / SEN0592 (5m), DFRobot; aktualizace 20. 11. 2026.**

Tento balík je výzkumná a vývojová příprava knihovny `SEN059x` pro Arduino/PlatformIO a ESP-IDF. **Nejde o hotovou knihovnu SEN059x ani o hardwarově ověřené měření.**

## Obsah

- `docs/SEN059x_reference.pdf` – námi nově sestavené PDF s technickým přehledem, odkazy a riziky.
- `docs/SEN059x_reference.md` – strojově čitelná referenční příručka v češtině.
- `protocol/registers.csv` – názvy, adresy a rozdíly mezi SEN0591 a SEN0592.
- `docs/ARCHITECTURE.md` – návaznost na tvoje knihovny TMP11x, MotorControl-RS, SensorCore a doporučený návrh nové knihovny.
- `protocol/modbus_vectors.csv` – oficiální příklady DFRobot; CRC přezkoušeno nezávisle.
- `protocol/Modbus_RTU_notes.md` – rámce, CRC, časování, RS485, odchylky v DFRobot dokumentaci.
- `examples/python/sen059x_probe.py` – náš bezpečný read-only nástroj pro USB–RS485 adaptér (`pyserial`). Umí test bez HW.
- `examples/platformio-esp32/` – náš minimální příklad pro ESP32, Arduino/PlatformIO; UART + poloduplexní RS485, bez skrytých knihoven.
- `tests/test_vectors.py` – offline testy rámců a dekódování, nezávislé na senzoru.
- `source_links/ORIGINAL_SOURCES.md` – původní adresy všech zásadních zdrojů, plus přehled knihoven a licencí.
- `source_links/sources.json` – seznam podkladů pro automatické stažení.
- `scripts/fetch_originals.py` – stáhne **originální** PDF specifikace, HTML DFRobot a ZIPy cizích knihoven na tvém PC.
- `originals/` – vyhrazeno pro soubory získané skriptem; ve výchozím ZIPu není žádný externí soubor vydáván za skutečně stažený.

## Základní zjištění

- Oba senzory používají **Modbus RTU** na **RS485**, default 115200, 8N1, adresa 1, FC03 čtení a FC06 zápis.
- Modbus registry měření jsou shodné. Rozdíl je maximální vzdálenost a konfigurace `0x0208` a `0x021F`.
- SEN0592: **30–5000 mm**, napájení **3,3–24 V**, odezva základního režimu **15–140 ms**, zpracovaného **190–750 ms**.
- SEN0591: **20–3000 mm** podle produktového přehledu, napájení **3,3–12 V**. Konfigurační dokument pro úrovně dosahu uvádí až 350 cm; jde o rozpor zdroje, který je nutné ověřit na fyzickém zařízení.
- Barvy vodičů SEN059x: červená VCC, černá GND, žlutá RS485 B, bílá RS485 A.

## Jak získat originální soubory

Na počítači s internetem a Pythonem 3.9+:

```powershell
python scripts/fetch_originals.py
```

Skript uloží kompletní originální HTML stránky DFRobot, originální PDF dokumenty Modbus Organization a ZIP archivní kopie otevřených knihoven. Pokud URL vrátí chybu nebo vyžaduje přihlášení, zaznamená to a archiv nebude vykazovat jako stažený. Přehled všech zdrojů je v `source_links/sources.json`.

**Původní DFRobot Wiki nepředstavuje samostatný datasheet PDF.** V době přípravy tohoto balíku nebyl dohledán ověřený originální SEN0592 PDF datasheet; závazným původním zdrojem je DFRobot Wiki. Nevyplňujeme to vymyšleným PDF URL.

## Rychlý test

```powershell
python tests/test_vectors.py
python examples/python/sen059x_probe.py --self-test
pip install pyserial
python examples/python/sen059x_probe.py --port COM16 --address 1 --mode realtime
```

`COM16` je pouze příklad. Pro každou fyzickou jednotku je nutné zvolit skutečný port a **před společným připojením čtyř jednotek přidělit unikátní adresy**. Příklady záměrně neprovádějí změny EEPROM/adresy/baudrate.

## Poznámky pro návrh knihovny

Pro C++ knihovnu držet přenos (UART/RS485/Modbus) oddělený od senzoru: předávat transakční callbacky, bez alokací a automatických retry; explicitně validovat CRC, adresu, délky, function code, Modbus exceptions, timeouty a state změn trvalé konfigurace. Vzory v `protocol/modbus_vectors.csv` jsou vhodné jako unit testy. Neodvozovat identitu modelu ze samotného přijetí Modbus odpovědi.

## Licenční a původové informace

Námi vytvořené texty a testovací příklady jsou nově sepsané pro tento balík. Originální knihovny a specifikace se stahují ze zdrojů vlastníků, jejich případné licence zůstávají zachovány. `DFRobot_RTU` MIT; `esp-modbus` Apache-2.0 s poznámkou o dědictví třetích stran; `modbus-esp8266` BSD; `ModbusMaster` – ověřit LICENSE v konkrétně stažené verzi. DFRobot produktové podklady mají vlastní práva a nejsou zde plošně přelicencovány.
