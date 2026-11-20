# USB–RS485 test SEN059x

Na PC s Python 3.9+ `pip install pyserial`. Senzor napájej ze samostatného zdroje v rozsahu dané varianty, signály A/B připoj na USB–RS485 adaptér. Některé adaptéry mohou mít opačné označení A/B – ověřit dokumentaci.

```
python sen059x_probe.py --self-test
python sen059x_probe.py --port COM16 --model SEN0592 --address 1 --mode all
python sen059x_probe.py --port /dev/ttyUSB0 --model SEN0591 --address 1 --mode realtime
```

Skript umožňuje **pouze čtení**; neobsahuje rizikové automatické změny konfigurace. Záměrně používá základní `pyserial` bez třetí strany typu pymodbus, aby šlo ověřit přesný Modbus rámec.

`--model` označuje měření v konzoli. Oba modely používají pro čtení stejné adresy registrů. U teploty sonda vypisuje původní slovo `raw` a jeho neznaménkový přepočet. Záporné teploty vyžadují ověření kódování na zařízení. [Měření teploty a chybějící echo](../../docs/reference/measurement-and-configuration.md#choosing-a-measurement)

## Poznámka o směrování

USB–RS485 adaptér obvykle řídí vysílací směr automaticky. Pokud používáš vlastní UART+transceiver na MCU, musíš řídit signály DE a /RE s přihlédnutím k úplnému odeslání rámce; viz PlatformIO příklad.
