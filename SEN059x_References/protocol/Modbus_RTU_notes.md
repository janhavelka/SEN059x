# Modbus RTU – poznámky pro SEN059x

Primární standard: https://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf
Sériová linka, nová implementace: https://www.modbus.org/file/secure/modbusoverserial.pdf

- Pořadí datových bytů v 16bitovém registru: high, low.
- Pořadí CRC bytů v RTU rámci: low, high.
- CRC16 (polynom 0xA001 v zrcadlené reprezentaci), start 0xFFFF.
- FC03 = čtení holding registrů, FC06 = zápis jednoho holding registru.
- Modbus exception = FC | 0x80, data exception kód. Ověř délku a CRC i u exception.
- Modbus doporučuje oddělovat rámce minimální pauzou 3,5 znaku; detailní pravidla viz oficiální sériová specifikace.
- Čas měření senzoru převažuje nad přenosovým časem UART. Konfigurovat logický timeout pro operaci zvlášť od byte receive timeoutu.
- Nepoužívej neznámé broadcast hodnoty ani opakované automatické zápisy do konfigurace.

## Známé ověřené dotazy

```
read filtered:  01 03 01 00 00 01 85 F6
read realtime:  01 03 01 01 00 01 D4 36
read temp:      01 03 01 02 00 01 24 36
read baud code: 01 03 02 01 00 01 D4 72
set baud 9600:  01 06 02 01 00 03 99 B3   (POUZE bench/test)
set address 5:  01 06 02 00 00 05 48 71   (opravený formát)
```

Rámce ve zdroji kontrolovány lokálním CRC16 algoritmem. Není to HW test senzoru.
