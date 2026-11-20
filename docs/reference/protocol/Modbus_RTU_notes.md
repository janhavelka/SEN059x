# Modbus RTU – poznámky pro SEN059x

Formát PDU a funkce FC03/FC06 popisuje [Modbus Application Protocol V1.1b3, §6.3 a §6.6](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf). Adresování a časování RTU popisuje [Modbus over Serial Line V1.02, §2.2 a §2.5.1.1](https://www.modbus.org/file/secure/modbusoverserial.pdf).

- Pořadí datových bytů v 16bitovém registru: high, low.
- Pořadí CRC bytů v RTU rámci: low, high.
- CRC16 (polynom 0xA001 v zrcadlené reprezentaci), start 0xFFFF.
- FC03 = čtení holding registrů, FC06 = zápis jednoho holding registru.
- Modbus exception = FC | 0x80, data exception kód. Ověř délku a CRC i u exception.
- Do 19 200 Bd včetně je mezera mezi rámci alespoň 3,5 znaku. Mezera delší než 1,5 znaku uvnitř rámce znamená neúplný rámec. Pro vyšší rychlosti včetně výchozích 115 200 Bd specifikace doporučuje pevné časy 750 µs uvnitř rámce (t1.5) a 1,75 ms mezi rámci (t3.5).
- Čas měření senzoru převažuje nad přenosovým časem UART. Konfigurovat logický timeout pro operaci zvlášť od byte receive timeoutu.
- Nepoužívej neznámé broadcast hodnoty ani opakované automatické zápisy do konfigurace.

## Známé ověřené dotazy

```
read filtered:  01 03 01 00 00 01 85 F6
read realtime:  01 03 01 01 00 01 D4 36
read temp:      01 03 01 02 00 01 24 36
read baud code: 01 03 02 01 00 01 D4 72
set baud 9600:  01 06 02 01 00 03 99 B3   (POUZE bench/test)
set address 5:  01 06 02 00 00 05 48 71   (úplný rámec z čínské wiki)
```

Rámce ve zdroji kontrolovány lokálním CRC16 algoritmem. Není to HW test senzoru.
