Originály stažené 20. 11. 2026

dfrobot/ obsahuje obě jazykové verze dokumentace SEN0591 a SEN0592,
katalogové stránky, příklady Arduino, schémata připojení a montážní výkresy.
Uložené HTML obsahuje vlastní text dokumentu. Vložené obrázky nadále
odkazují na web výrobce. Technické výkresy a zapojení lze otevřít zvlášť.

modbus/ obsahuje specifikaci aplikačního protokolu V1.1b3, příručku pro
sériovou linku V1.02 a starší vydání V1.0.

Katalog ../source_links/sources.json obsahuje URL, velikost a SHA-256
každého originálu uloženého v Gitu. Následující příkaz z kořene repozitáře
ověří jejich shodu offline.

  python scripts/fetch_originals.py --verify

Čtyři archivní verze cizích knihoven, stránka projektu DFRobot_RTU a tři
přehledy příbuzných senzorů se ukládají lokálně do libraries/ a related/.
Stažení chybějících souborů a kontrolu dostupných kopií spustíš příkazem

  python scripts/fetch_originals.py

Soubor download_results.json popisuje poslední stažení. Pokud vydavatel
mezitím změnil obsah stránky, skript ohlásí rozdíl vůči uloženému SHA-256.
Původní soubor lze obnovit z historie Gitu. Dokumenty podléhají podmínkám
jednotlivých vydavatelů.
