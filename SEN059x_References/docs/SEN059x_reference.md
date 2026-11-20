# SEN059x – technická reference pro implementaci

Zdroj: DFRobot SEN0592 a SEN0591 Wiki (odkazy v `source_links/ORIGINAL_SOURCES.md`), MODBUS Organization. Kontrolováno 20. 11. 2026.

## Podporované varianty

| Vlastnost | SEN0591 | SEN0592 |
|:--|:--|:--|
| DFRobot SKU | SEN0591 | SEN0592 |
| Typ | ultrazvukový senzor vzdálenosti, RS485 | ultrazvukový senzor vzdálenosti, RS485 |
| Jmenovitý rozsah | 20–3000 mm (web produktu) | 30–5000 mm |
| Napájení | 3,3–12 V | 3,3–24 V |
| Ochrana | výrobce uvádí voděodolný/prachotěsný | IP67 |
| Komunikace | Modbus RTU, RS485 | Modbus RTU, RS485 |
| Default | 115200 8N1, slave 0x01 | 115200 8N1, slave 0x01 |
| Funkce | 0x03 / 0x06 | 0x03 / 0x06 |
| Rozsahy registrů | obecně stejné, profilově rozdílné limity | obecně stejné, profilově rozdílné limity |

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
- Po úspěchu `0x0101` teprve dostaneš nově změřenou vzdálenost. Odezva 15–140 ms; nevynucovat 10ms timeout.
- Pro `0x0100` výrobce uvádí 190–750 ms. Navrhnout timeout pokrývající měření, ne jen přenos 7 B po UART.

## Registry

Nejsou přepisovány do volně distribuované oficiální tabulky; přehled a modelové rozdíly jsou v souboru `protocol/registers.csv`. Primární zdroj detailních omezení viz výrobce.

| Adresa | Přístup | Popis |
|:--|:--|:--|
| `0x0100` | R | Processed range, mm |
| `0x0101` | R | Real-time range, mm |
| `0x0102` | R | Teplota, raw 0,1 °C |
| `0x010A` | R | Čas ultrazvukového echa, µs |
| `0x0200` | R/W | Slave adresa, změna může ovlivnit další komunikaci |
| `0x0201` | R/W | Kód baudrate, okamžitý účinek při zápisu |
| `0x0205` | R/W | Polarita spínacího výstupu jen pro switch mode |
| `0x0206` | R/W | Prahová vzdálenost jen pro switch mode |
| `0x0208` | R/W | Úroveň šířky paprsku (různá mapování dle modelu) |
| `0x0209` | R/W | Jednotky v UART režimu – nedoložený pro běžné RS485 čtení |
| `0x021A` | R/W | Potlačení rušení napájení, 1–5 |
| `0x021F` | R/W | Úroveň dosahu, rozdílná pro modely |

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

- V původní referenci SEN0592 má řádek „Host send (write)“ **chybně uvedený function code 0x03**. Správně zbytek dokumentace a ukázkové rámce používají FC06.
- Ukázka změny adresy vypsaná v dokumentaci `01 06 02 00 05 48 71` obsahuje **7 bytů**, a tedy chybí vysoký bajt hodnoty adresy. Správný 8bajtový rámec je `01 06 02 00 00 05 48 71`. CRC z originálu odpovídá právě 8bajtové podobě.
- SEN059x uvádí `0xFF` jako broadcast; standardní Modbus RTU definuje broadcast adresu `0x00`. Do ověření na HW nepoužívat neadresované zápisy.
- Popis registru `0x0209` zmiňuje UART režimy, ale senzor vyvádí RS485 – není jisté, zda pro tento model vůbec může být tento režim použit.
- U SEN0591 je rozpor mezi produktovou specifikací do 3000 mm a popisem konfiguračního stupně 4 s dosahem 350 cm. Validovat fyzicky.
- Přesné mapování **všech** hodnot registru baudrate `0x0201` ve webovém extraktu vypadá zkráceně/poškozeně; nespoléhat na parafráze, kontrolovat originální stránku a HW.
- DFRobot příklad pro Arduino používá neomezené pokusy `while(flag)` a `Serial.flush()`; není vhodný přímo do produkční deterministické knihovny.

## Bezpečnost FGI

Tyto senzory nejsou deklarované jako funkčně bezpečnostní detektory osob. Vhodné jako dodatečná senzorika a redundance měření, **nikoli** jako výhradní bezpečnostní zastavení stroje. Prach, překážky malých průřezů, nakloněné povrchy a vzájemné rušení je nutné ověřit na skutečné instalaci.
