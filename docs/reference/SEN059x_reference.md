# SEN059x – technická reference pro implementaci

Zdroj: DFRobot SEN0592 a SEN0591 Wiki (odkazy v `source_links/ORIGINAL_SOURCES.md`), MODBUS Organization. Kontrolováno 20. 11. 2026.

## Podporované varianty

| Vlastnost | SEN0591 | SEN0592 |
|:--|:--|:--|
| DFRobot SKU | SEN0591 | SEN0592 |
| Typ | ultrazvukový senzor vzdálenosti, RS485 | ultrazvukový senzor vzdálenosti, RS485 |
| Jmenovitý rozsah | 20–3000 mm (web produktu) | 30–5000 mm |
| Napájení | 3,3–12 V | 3,3–24 V |
| Krytí | IP67 | IP67 |
| Provozní proud | ≤10 mA | ≤10 mA |
| Doba od zapnutí do připravenosti | ≤1000 ms | ≤1000 ms |
| Provozní teplota | −25 až +65 °C | −25 až +65 °C |
| Komunikace | Modbus RTU, RS485 | Modbus RTU, RS485 |
| Default | 115200 8N1, slave 0x01 | 115200 8N1, slave 0x01 |
| Funkce | 0x03 / 0x06 | 0x03 / 0x06 |
| Rozsahy registrů | obecně stejné, profilově rozdílné limity | obecně stejné, profilově rozdílné limity |

Parametry jednotlivých variant uvádějí produktové listy výrobce. [SEN0591](https://www.dfrobot.com/product-2728.html) · [SEN0592](https://www.dfrobot.com/product-2729.html)

## Připojení

| Barva | Význam |
|:--|:--|
| Červená | +VCC |
| Černá | GND |
| Žlutá | RS485 B |
| Bílá | RS485 A |

Použít skutečný RS485 transceiver mezi UART MCU a A/B. Nepřipojovat A/B přímo na 3,3V GPIO. U non-isolated RS485 navrhnout referenční signálovou zem a přepěťovou ochranu podle instalace. Terminace 120 ohm pouze na **fyzických koncích** sběrnice, bias na určeném místě; specifické hodnoty závisí na celé síti. Návrh zapojení a význam A/B je nutné ověřit podle konkrétního transceiveru.

## Přesný základ protokolu

- UART 8 datových bitů, bez parity, 1 stop bit; 115200 Bd default.
- Dotaz FC03: `[slave][03][register_hi][register_lo][count_hi][count_lo][crc_lo][crc_hi]`.
- Odpověď na jedno slovo: `[slave][03][02][data_hi][data_lo][crc_lo][crc_hi]`.
- Zápis FC06: `[slave][06][register_hi][register_lo][value_hi][value_lo][crc_lo][crc_hi]`, odpověď zpravidla echo rámce.
- CRC16/MODBUS: init 0xFFFF; reflected polynomial 0xA001; low byte první.
- Čtení `0x0100`, `0x0101` i `0x010A` spouští měření. K době měření je třeba přičíst přenos dotazu a odpovědi.

| Přibližná odezva zařízení | SEN0591 (EN i CN) | SEN0592 (EN) | SEN0592 (CN) |
|:--|--:|--:|--:|
| Zpracovaná vzdálenost (`0x0100`) | 190–750 ms | 190–750 ms | 300–750 ms |
| Okamžitá vzdálenost (`0x0101`) | 15–140 ms | 15–140 ms | 15–140 ms |
| Teplota a doba echa (`0x0102`, `0x010A`) | 5–140 ms | 5–140 ms | 40–140 ms |

Dolní hranice časů se u SEN0592 v jazykových verzích liší. Horní hranice se shodují. [SEN0591 EN](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0591 CN](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m) · [SEN0592 EN](https://wiki.dfrobot.com/sen0592/docs/19640) · [SEN0592 CN](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m)

## Registry

Adresy v [tabulce registrů](protocol/registers.csv) jsou hodnoty pro přímý zápis do adresního pole PDU. Tabulka uvádí také mapování rychlostí, úhlů a rozsahů pro oba modely.

| Adresa | Přístup | Popis |
|:--|:--|:--|
| `0x0100` | R | Processed range, mm |
| `0x0101` | R | Real-time range, mm |
| `0x0102` | R | Teplota v desetinách °C, rozlišení 0,5 °C. Kódování pod nulou výrobce neupřesňuje. |
| `0x010A` | R | Čas ultrazvukového echa v µs. Orientační vzdálenost v mm odpovídá času dělenému 5,75. |
| `0x0200` | R/W | Slave adresa, změna může ovlivnit další komunikaci |
| `0x0201` | R/W | Kód baudrate, okamžitý účinek při zápisu |
| `0x0205` | R/W | Polarita spínacího výstupu jen pro switch mode |
| `0x0206` | R/W | Prahová vzdálenost jen pro switch mode |
| `0x0208` | R/W | Úroveň šířky paprsku (různá mapování dle modelu) |
| `0x0209` | R/W | Jednotky v UART režimu – nedoložený pro běžné RS485 čtení |
| `0x021A` | R/W | Potlačení rušení napájení, 1–5 |
| `0x021F` | R/W | Úroveň dosahu, rozdílná pro modely |

U SEN0591 výrobce rozepisuje odezvu také podle nastavené úrovně dosahu. SEN0592 uvádí vzdálenosti pro každou úroveň a souhrnné časy pro měřicí registry. [SEN0591, registr 0x021F](https://wiki.dfrobot.com/sen0591/docs/19645) · [SEN0592, registr 0x021F](https://wiki.dfrobot.com/sen0592/docs/19640)

| Úroveň | Přibližný dosah SEN0591 | Okamžitá vzdálenost | Zpracovaná vzdálenost |
|--:|--:|--:|--:|
| 1 | 50 cm | 15–80 ms | 190–500 ms |
| 2 | 150 cm | 20–90 ms | 230–550 ms |
| 3 | 250 cm | 25–100 ms | 250–600 ms |
| 4 (výchozí) | 350 cm podle registru, jmenovitě 300 cm | 35–110 ms | 280–650 ms |

## Doporučení pro protokolovou knihovnu

1. Model varianty určit explicitně při konfiguraci, netipovat podle rozsahu nebo adresy.
2. Read-only příkazy vyvíjet jako první: realtime distance, filtered distance, temperature, echo time.
3. Explicitní `Status` včetně `Timeout`, `Crc`, `WrongAddress`, `UnexpectedResponse`, `ModbusException`, `InvalidValue`.
4. U konfigurace vyžadovat explicitní povolení zápisů, validaci rozsahu, kontrolu stavu a zpětné čtení.
5. Po změně baudrate/adresy musí aplikace přeorganizovat UART a vlastní adresování bez automatických plošných skenů.
6. V žádném případě nevysílat reset/změnu adresy broadcastem v síti několika senzorů.
7. Řídit všechny jednotky na jednom RS485 rozhraní sériově; paralelní aplikační požadavky musí arbitrovat owner transportu.
8. Maximální čas čtení dělat konfigurovatelný s hardwarovým měřením, nespolehlivý nebo starý údaj nesmí být považován za nový.

## Nesrovnalosti v původním dokumentu

- Obě reference v záhlaví dotazu i odpovědi pro zápis chybně uvádějí FC03. Parametry protokolu i příklady zápisu používají FC06.
- Anglický příklad změny adresy `01 06 02 00 05 48 71` vynechává vysoký bajt hodnoty v dotazu i odpovědi. Obě čínské wiki uvádějí úplný rámec `01 06 02 00 00 05 48 71`. Jeho CRC ověřují offline testy.
- DFRobot připouští adresy 1–254 a uvádí `0xFF` jako broadcast. Standardní Modbus RTU přiděluje zařízením adresy 1–247, adresu 0 používá pro broadcast a 248–255 rezervuje. Referenční sonda používá rozsah 1–247. [Modbus Serial Line V1.02, §2.2](https://www.modbus.org/file/secure/modbusoverserial.pdf)
- Popis registru `0x0209` zmiňuje UART režimy, ale senzor vyvádí RS485 – není jisté, zda pro tento model vůbec může být tento režim použit.
- U SEN0591 je rozpor mezi produktovou specifikací do 3000 mm a popisem konfiguračního stupně 4 s dosahem 350 cm. Validovat fyzicky.
- Anglické řádky pro `0x0201` jsou od hodnoty `0x0005` poškozené. Obě čínské wiki uvádějí všechny kódy `0x0002` až `0x0009`. Tabulka časů přenosu má vlastní pořadová čísla 1–8. Kódy rychlostí v CSV pocházejí přímo z popisu registru. [SEN0591 CN](https://wiki.dfrobot.com.cn/SKU_SEN0591_Ultrasonic_Ranging_Sensor_3m) · [SEN0592 CN](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m)
- U SEN0592 uvádí čínský souhrn úhel 30–60°. Rozpis `0x0208` rozlišuje čtyři stupně a vodorovný a svislý úhel. Produktová stránka udává pro obě osy rozsahy 40–65° a 60–80°. [SEN0592 CN](https://wiki.dfrobot.com.cn/SKU_SEN0592_Ultrasonic_Ranging_Sensor_5m) · [SEN0592, produktová stránka](https://www.dfrobot.com/product-2729.html)
- Ověření na senzoru vyžaduje také návratová hodnota při chybějícím echu, chování po restartu u konfiguračních zápisů a rychlost, kterou senzor potvrdí změnu `0x0201`. Publikované příklady tyto situace nepokrývají.
- DFRobot příklad pro Arduino používá neomezené pokusy `while(flag)` a `Serial.flush()`; není vhodný přímo do produkční deterministické knihovny.

## Bezpečnost FGI

Tyto senzory nejsou deklarované jako funkčně bezpečnostní detektory osob. Vhodné jako dodatečná senzorika a redundance měření, **nikoli** jako výhradní bezpečnostní zastavení stroje. Prach, překážky malých průřezů, nakloněné povrchy a vzájemné rušení je nutné ověřit na skutečné instalaci.
