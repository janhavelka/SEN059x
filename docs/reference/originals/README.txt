Originály stažené 20. 11. 2026

dfrobot/ obsahuje obě jazykové verze dokumentace SEN0591 a SEN0592,
katalogové stránky, příklady Arduino, schémata připojení a montážní výkresy.
Jsou zde také fotografie kabelů, společný snímek sériového monitoru a popis
rozšiřující desky DFR0259 použité v příkladech výrobce.
Uložené HTML obsahuje vlastní text dokumentu. Vložené obrázky nadále
odkazují na web výrobce. Technické výkresy a zapojení lze otevřít zvlášť.

modbus/ obsahuje specifikaci aplikačního protokolu V1.1b3, příručku pro
sériovou linku V1.02 a starší vydání V1.0.

dyp/ obsahuje původní mechanické výkresy rodin A22 a A21. Shoda rozměrů
dokládá původ výkresu. Verzi firmwaru prodávaných čidel DFRobot neuvádí.

Katalog ../source_links/sources.json obsahuje URL, velikost a SHA-256
každého originálu uloženého v Gitu. Následující příkaz z kořene repozitáře
ověří jejich shodu offline.

  python scripts/fetch_originals.py --verify

Čtyři archivní verze cizích knihoven, stránka projektu DFRobot_RTU a tři
přehledy příbuzných senzorů se ukládají lokálně do libraries/ a related/.
V related/dyp/ se ukládají čtyři další PDF s rodinnými specifikacemi
a popisy rozhraní. Při stažení nebo použití kopie skript ověří jejich SHA-256.
Stažení chybějících souborů a kontrolu dostupných kopií spustíš příkazem

  python scripts/fetch_originals.py

Soubor download_results.json popisuje poslední kontrolu nebo stažení přes
výše uvedený příkaz bez parametrů. Platnou místní kopii skript použije přímo.
Při stažení souboru se známým SHA-256 zkontroluje shodu s katalogem.
Původní soubor lze obnovit z historie Gitu. Dokumenty podléhají podmínkám
jednotlivých vydavatelů.
