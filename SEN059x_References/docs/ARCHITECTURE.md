# Doporučení pro repozitář `SEN059x`

## Návaznost na stávající knihovny

Už používaná konvence GitHub účtu `janhavelka`:

- [TMP11x](https://github.com/janhavelka/TMP11x), [TMP1x2](https://github.com/janhavelka/TMP1x2), [SHT3x-main](https://github.com/janhavelka/SHT3x-main), [DP8382x](https://github.com/janhavelka/DP8382x): krátké názvy produktových řad, explicitní podpora variant, framework-neutral C++ core.
- [MotorControl-RS](https://github.com/janhavelka/MotorControl-RS): obsahuje ověřované modelově specifické Modbus RTU codecs a průběh na RS485, ale API řídí **motory**. Není vhodné jej bez změny návrhu přímo vložit jako závislost na ultrazvukovém senzoru.
- [SensorCore-node](https://github.com/janhavelka/SensorCore-node): vhodná aplikační úroveň pro pozdější CAN reporting, síťové role a časové značky. Senzorová knihovna by o CAN vůbec neměla vědět.

## Navržené softwarové oddělení

```
FGI vehicle application / SensorCore-node
          |
      SEN059x  (device profiles, register validation, units, typed results)
          |
    Modbus RTU transaction interface  (transport owned by caller)
          |
 ESP32-S3 UART + RS485 DE/RE | ESP32-P4/native ESP-IDF UART
```

- Název projektu `SEN059x`, veřejná hlavička `<SEN059x/SEN059x.h>`, jmenný prostor `SEN059x`.
- `DeviceModel::SEN0591` / `DeviceModel::SEN0592`, model explicitně z konfigurace.
- Žádné hardcoded GPIO, `HardwareSerial`, Arduino, FreeRTOS nebo ESP-IDF funkce v core.
- Explicitní `ITransport::transact(request, expectedReply, deadline)` nebo injektované callbacky, request/response framing patří společné RTU vrstvě.
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
