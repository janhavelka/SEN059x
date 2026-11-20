# USB–RS485 test SEN059x

Na PC s Python 3.9+ `pip install pyserial`. Senzor napájej ze samostatného zdroje v rozsahu dané varianty, signály A/B připoj na USB–RS485 adaptér. Některé adaptéry mohou mít opačné označení A/B – ověřit dokumentaci.

```
python sen059x_probe.py --self-test
python sen059x_probe.py --port COM16 --model SEN0592 --address 1 --mode all
python sen059x_probe.py --port /dev/ttyUSB0 --model SEN0591 --address 1 --mode realtime
```

Sonda posílá jednoslovné dotazy FC03 na adresy 1–247. Spusť ji jako jediného vlastníka nečinné sběrnice až po dokončení předchozího měření. Po připojení napájení vyčkej alespoň sekundu.

Pro měření zadej v `--model` variantu podle označení zařízení. Oba modely používají pro čtení stejné adresy registrů. Režim `all` odebírá jednotlivé hodnoty postupně. U teploty sonda vypisuje původní slovo `raw` a jeho neznaménkový přepočet. Záporné teploty vyžadují ověření kódování na zařízení. [Měření teploty a chybějící echo](../../docs/reference/measurement-and-configuration.md#choosing-a-measurement)

`--timeout` omezuje jeden dotaz včetně zápisu, měření, příjmu a mezery na konci rámce. Adaptér USB může doručit jednu odpověď v několika dávkách. Sonda proto počká na všech 7 bajtů odpovědi nebo 5 bajtů výjimky. Pak kontroluje přebytečné bajty po dobu 3,5 znaku 8N1 do 19200 Bd včetně, při vyšší rychlosti 1,75 ms. Skutečnou prodlevu mezi jednotlivými znaky na vodičích nelze z časů USB dávek spolehlivě určit. Chyba přenosu ukončí běh před dalším dotazem. [Časování Modbus RTU](../../docs/reference/protocol/Modbus_RTU_notes.md#wire-time-and-measurement-time)

## Poznámka o směrování

USB–RS485 adaptér obvykle řídí vysílací směr automaticky. Pokud používáš vlastní UART+transceiver na MCU, musíš řídit signály DE a /RE s přihlédnutím k úplnému odeslání rámce; viz PlatformIO příklad.
