# Spellcross — reakční palba: hlubší porovnání originálu a Reloaded

**Datum analýzy:** 22. 11. 2026  
**Závěr (doplněný po následné kontrole):** Hlavní princip mechaniky Reloaded odpovídá původní hře. Dodatečný rozbor odhalil **jednu konkrétní chybu v podmínce kontaktu**: remake dříve vyžadoval vlastní přímý dohled každého střelce, zatímco originál používá **sdílenou viditelnost obráncovy strany** a následně samostatně ověřuje dostřel zbraně. Tento bod byl cíleně opraven; ostatní rozdíly v pořadí střelců a RNG zůstávají zdokumentovanými odlišnostmi.

## Dodatek: původní podmínka viditelnosti a oprava Reloaded

Původní závěr v sekci 2, že konkrétní obránce musí mít na novém poli **vlastní přímý dohled**, byl nesprávný. Následná kontrola EN binárky ukázala:

- Pohybová větev na **`0x1D690–0x1D6C9`** a obdobná větev **`0x28890–0x288B8`** testují stav viditelnosti starého a nového pole v mapových příznacích jedné strany; teprve při novém kontaktu volají sběr obránců `0x9BE32`.
- Sběr obránců volá na **`0x9C012`** (a ve zvláštní větvi **`0x9C04F`**) funkci **`0x62562`**, která testuje legální dosah střelby; nikoli samostatnou funkci **`0x625BE`** pro dohled konkrétní jednotky.
- **Dělostřelectvo proto může reagovat na cíl nově odhalený předsunutou spojeneckou jednotkou**, i mimo vlastní dohled děla, pokud ostatní podmínky palby platí. Nejde o neomezenou palbu na již dříve viditelné cíle.

**Oprava v Reloaded:** `SpellMap::CheckReactionFire` nyní vyhodnocuje nově vzniklý kontakt z kombinovaného přímého dohledu živých jednotek *bránící se strany*; střelec se pak kontroluje podle `CalcAttackRange`, dostupných AP, typu cíle a iniciativy. `ProcessReactionFire` již nevyžaduje vlastní `HasDirectSight` reagujícího střelce. Sdílená viditelnost se přepočítává symetricky pro Alianci i Other Side, nikoli z hráčského bufferu zamlžení pro obě strany.

To je **cílená oprava konkrétního rozdílu od originálu**, ne změna zásady výběru střelců nebo RNG. Stále zbývá runtime porovnání DOS a Reloaded na shodném scénáři.

## 1. Rozsah, podklady a míra jistoty

Analýza se záměrně opírá o **náš vlastní originál hry a jeho rekonstruovanou logiku**, nikoli o cizí kopie, návody z diskusí či jiné hry.

- **Originální EN hra:** dodaný `Spellcros_EN_Verze_Puvodni.zip`, soubor `SPELL/SPELCROS.EXE`, SHA-256 `ff6b6554a2a8c532a2a2ed2c6d437ce63b7fc6b480efb29e9f7e14f441837623` (shoduje se s hashem uvedeným v existující projektové reverzní analýze).
- **Statická kontrola originálu:** přímo z tohoto souboru byla znovu přečtena jeho LE/x86 kódová sekce. Provedena disassemblace pomocí `objdump` v režimu 32bit x86; adresy v dokumentu jsou virtuální adresy kódového objektu s bází `0x10000`. Zobrazené relativní cíle `CALL` a instrukce jsou ověřené přímo z binárky.
- **Reloaded:** `luboshorak/spellcross-reloaded`, `main`, commit `3d6b41f443639b32f053b0d1a3d2adbdd15dce6f` (22. 11. 2026), zejména `spellcross-map-edit-main/source/map.cpp` a `spellcross-map-edit-main/source/spell_units.cpp`.
- **Existující výzkum:** `docs/issue-188-tactical-reaction-fire.md`, `packaging/original-mechanics-parity.md`, `docs/verification.md`, manuál původního Spellcrossu (pasáž „Střelba z obrany“).
- **Hranice důkazu:** tentokrát se **nespouštěla původní DOS hra** a nebylo porovnáno přesné chování obou programů za stejného průběhu mise. Tato práce je nová statická analýza **EN** binárky. Stejný výsledek pro CZ nelze tvrdit bez samostatného rozboru CZ binárky. Výstupy Ghidry `REAWork\Spellcross\AnalysisEN` a `AnalysisCZ`, zmíněné v repozitáři, nebyly v tomto prostředí k dispozici.

Značky důkazů v textu: **PROKÁZÁNO** = přímo viditelná instrukční sekvence / zdrojový kód; **SILNÁ INTERPRETACE** = funkce odvozená z kontextu volání; **NEOVĚŘENO** = zbývá runtime nebo další analýza stavů.

## 2. Základní model je správně zvolený

Udržování akčních bodů, obranná střelba při kontaktu během pohybu, legální cíl zbraně a rozhodnutí iniciativou odpovídají konstrukci původního Spellcrossu. Není důvod předělávat reakční palbu na „odvetný výstřel po každém zásahu“.

Reloaded po jednom kroku pohybující se jednotky zavolá `SpellMap::CheckReactionFire`. Nejprve kontroluje **nové odhalení pro celou stranu obránce**; pak u jednotlivých střelců ověří nepřátelskou stranu, živého a aktivního obránce, dostupné útoky, **legální dosah zbraně (ne osobní dohled)** a iniciativu. Obranné výstřely jsou standardní útoky s odpočtem AP. Hráč i Other Side používají společnou reakční logiku.

Původní `SPELCROS.EXE` skutečně obsahuje rutinu sběru kandidátů **`0x9BE32`**. Je volána na čtyřech místech kódových větví pohybu: **`0x1D6C9`, `0x1DC9A`, `0x288B8`, `0x28D07`**. Rutina prochází protilehlou skupinu až 50 slotů a výsledkem je seznam jednotek pro obrannou palbu. **PROKÁZÁNO.** Podrobné přiřazení všech čtyř větví konkrétním variantám pohybu (včetně teleportu) ještě není úplně hotové.

## 3. Sporný bod A: první reagující jednotka — co je skutečně v originálu

### 3.1 Nově dohledaná rutina výběru

Původní `SPELCROS.EXE` obsahuje výběrovou rutinu **`0x8D7E7`**. Připravuje si seznam kandidátů (předává se přes pomocnou rutinu **`0x8D77D`**) a vybírá **jednu** dvojici střelec–cíl. V jejím rozhodovacím cyklu je z instrukcí patrné:

1. Pro kandidáta se načte **aktuální počet AP** a **cena jednoho výstřelu**. Na `0x8D97A` až `0x8D9A3` se vypočte celočíselný počet zbývajících výstřelů (`AP / AP_na_výstřel`). Jednotky bez AP se odstraní z výběru (`0x8D881` až `0x8D90F`).
2. Zavolá se běžná funkce zjišťující, zda je zamýšlený útok možný, **`0x9B6C1`** (`CALL` z `0x8D9BC`).
3. Při kladném výsledku se zavolá podrobnější funkce **`0x9B907`** (`CALL` z `0x8D9D9`). Ta vypočítává **odhad efektu útoku**, který bere v úvahu více okolností než prostá tabulková síla zbraně. Její instrukce pracují mimo jiné s parametry/aktuálním stavem cíle a přepočty účinku. V této zprávě není tvrzeno, že je celá tato dlouhá funkce plně sémanticky zrekonstruována.
4. Návratová hodnota se **násobí zbývajícím počtem výstřelů**: `IMUL EAX,[EBP-0x10]` na **`0x8D9E7`**.
5. Převýší-li výsledek dosavadní maximum, uloží se nový vítězný střelec (`0x8D9EE` až `0x8DA08`). Na konci rutina vrací jednu vybranou dvojici (`0x8DA1A` až `0x8DA32`).

**PROKÁZÁNO: originál používá součin odhadované účinnosti a počtu zbývajících výstřelů.** Nejde tedy jen o domněnku z remaku.

Zjednodušený popis doložené části originálu:

```text
pro každého dosud způsobilého obránce:
    zbývající_výstřely = aktuální_AP // cena_výstřelu
    pokud zbývající_výstřely == 0: vyřadit
    pokud útok není dovolen: přeskočit
    odhad = původní_funkce_0x9B907(obránce, cíl, ...)
    skóre = odhad * zbývající_výstřely
    pokud skóre > nejlepší_skóre:
        nejlepší = obránce
vrátit jednoho nejlepšího střelce
```

### 3.2 Jak to dělá Reloaded

`SpellMap::CheckReactionFire` sestaví pro každého úspěšného kandidáta skóre:

```cpp
priority = std::max(0, shooter->GetAttack(moving_unit)) * shots;
```

`GetAttack(moving_unit)` vrací nominální útok na třídu cíle s přičtením bonusů zkušeností, formace a vylepšení. Nejde o totéž co komplexnější projekce **`0x9B907`** z originálu.

Reloaded potom seřadí kandidáty jednou při kontaktu a vloží do `reaction_fire_queue` **všechny jejich tehdy dostupné výstřely**. `ProcessReactionFire` před každým výstřelem znovu bezpečně kontroluje živý cíl, AP, stav střelce, přímý dohled a zákonnost útoku, ale **znovu nepřepočítá prioritu mezi ostatními střelci**.

### 3.3 Kde jsou reálné rozdíly

| Otázka | Originál EN | Reloaded | Závažnost |
| --- | --- | --- | --- |
| Princip priority | Odhadovaný účinek × zbývající výstřely | Tabulkově vyřešený útok × zbývající výstřely | Stejný základ; jiná přesnost odhadu |
| Výběr střelce | `0x8D7E7` při jednom volání vrací jednoho vítěze; při dalším vyhodnocení znovu čte aktuální AP/účinnost | Jeden snapshot priority při kontaktu; všechny střely předem zařazeny | Může změnit pořadí palby |
| Shoda skóre | Aktualizace jen při **striktně vyšším** skóre, tedy první kandidát v pořadí zůstává | Při shodě nakonec přednost **vyššímu ID** | Úzká, ale přímo doložená odlišnost |
| Kontrola před animovaným výstřelem | V originále existují samostatné testy aktuálního stavu | Reloaded bezpečnostní testy opakuje | Správné a rozumné technické zajištění |

**Pozor na důležitý rozdíl mezi výběrem jednoho útoku a celou sekvencí:** Původní `0x8D7E7` volaná ze stavového řízení vrací vždy jedinou dvojici; každé její další zavolání znovu porovnává AP a účinnost. Přiřazení každého dalšího zavolání konkrétnímu snímku originální animace je předmětem dalšího runtime ověření. Neoznačuji proto veškeré možné sekvence výstřelů za stoprocentně doložené.

**Příklad potenciálně jiného pořadí (ilustrativní, ne změřená bitva):**

- Obránce A: odhad účinku 20, zbývající 2 výstřely → priorita 40.
- Obránce B: odhad účinku 12, zbývající 3 výstřely → priorita 36.
- První volba je A. Po jeho výstřelu má A už jen `20 × 1 = 20`, kdežto B stále `12 × 3 = 36`.
- Při přepočtu může následovat B; při předem sestavené frontě Reloaded může A vystřelit dvakrát po sobě.

Tohle je **úzká rozdílnost věrnosti**, nikoli důkaz nefungující reakční palby. Pro běžnou hru je současná volba konzistentní a pochopitelná. Pokud však chceme reprodukci taktického rozhodování originálu do posledního výstřelu, je to kandidát na cílené zpřesnění, nikoli na urgentní opravu.

### 3.4 Ještě jeden původní detail: iniciativa není bez výjimky jedinou větví

Ve sběru kandidátů `0x9BE32` lze najít rozcestí kolem **`0x9BFA7`**:

- Standardní větev na `0x9BFB5` až `0x9BFF0` provede oba losy iniciativy a propustí obránce pouze při striktní výhře.
- Druhá větev na **`0x9C02E`** přejde k dalším testům účinnosti a zákonnosti útoku **bez těchto dvou losů**, pokud byly předtím splněny určité stavové podmínky.

**PROKÁZÁNO** je samotné rozdvojení; **NEOVĚŘENO** zůstává úplný význam podmiňujících příznaků (`0x57446C`, `0x574470`, položka struktury jednotky na `+0x41`) a to, jak často se tato větev v normální reakční palbě používá. Není rozumné ji bez dokončené interpretace překopírovat do Reloaded. Starší jednoduché tvrzení „originál *vždy* hází iniciativou“ je ale třeba upřesnit na „originál má větev s hodem iniciativy a další speciální větev“.

## 4. Sporný bod B: RNG — jiná posloupnost není jiná pravděpodobnost

### 4.1 Co dělá originál

Původní funkce **`0x575DC`** je přímo dohledaný generátor omezeného náhodného čísla. Při nenulové mezi provede transformaci interního stavu (instrukce posuvů/rotací a sčítání, následně zápis stavu), poté použije 16bitové neznaménkové dělení a vrátí **zbytek po dělení** požadovanou mezí. Při argumentu `6` tak vrací `0..5`. Při nule vrací nulu bez stejného kroku generování. **PROKÁZÁNO.**

V rutině `0x9BE32` je doloženo přesné pořadí:

```text
0x9BFB5   vypočti iniciativu POHYBUJÍCÍ SE jednotky
0x9BFC2   volej RNG(6) pro pohybující se jednotku
0x9BFD1   vypočti iniciativu OBRÁNCE
0x9BFDE   volej RNG(6) pro obránce
0x9BFE9   porovnej obránce proti pohybující se jednotce
0x9BFEF   při remíze / prohře NEPOVOL reakci
```

Vlastní výpočet iniciativy je v **`0x9C095`**: základ z tabulky původního typu jednotky **offset `+0x45`** a zkušenostní hodnota konkrétní jednotky. To odpovídá principu rekonstruovanému v Reloaded.

### 4.2 Co dělá Reloaded

```cpp
const int defender_roll = GetReactionInitiative(shooter) + (rand() % 6);
const int mover_roll    = GetReactionInitiative(moving_unit) + (rand() % 6);
if (defender_roll <= mover_roll) continue;
```

Máme tedy **dvě doložené technické odlišnosti**:

1. Reloaded používá C/C++ `rand()`, nikoli přesný DOS generátor `0x575DC` (jiný interní stav, seed, posloupnost a potenciálně i modulační bias).
2. **Dokonce i pořadí dvou tahů je opačné:** Reloaded nejdřív losuje za obránce, originál nejdřív za pohybující se jednotku. To by mohlo změnit jednotlivý výsledek i po výměně generátoru za identický, pokud by zůstalo prohozené pořadí volání.

### 4.3 Dopad na hru

Obě implementace pracují s celými čísly `0..5` a se stejnou striktní podmínkou `obránce > pohybující se`. Za předpokladu rovnoměrných nezávislých hodnot je pravděpodobnost stejná. Například rozdíl iniciativ `0` dává `15/36 = 41,67 %` šanci obránce, rozdíl `+4` dává `33/36 = 91,67 %`, rozdíl `+6` dává jistotu.

Ve skutečném pseudonáhodném proudu mohou vznikat drobné statistické odchylky a odlišné posloupnosti. Kromě toho **jakékoli další volání RNG v jiné části hry** posune následující tahy; `rand()` je navíc závislý na implementaci knihovny/platformě. Nelze tedy očekávat, že stejné uložené postavení v DOS a Reloaded vytvoří stejný sled reakcí.

**Praktický závěr:** Odlišný RNG **není herní chyba**. Je to překážka **deterministické/bitově přesné reprodukce**, nikoli důkaz špatně nastavených šancí. Přesný DOS RNG má smysl implementovat až tehdy, pokud si výslovně stanovíme požadavek na shodnou sekvenci výsledků, a pak je třeba sjednotit také seed, počet a pořadí všech volání, což je výrazně větší úkol než změna `rand()%6`.

## 5. Sporný bod C: teleportace a přesné časování

### 5.1 Co lze doložit v Reloaded

- **Obyčejný krok:** `SpellMap::Tick` po dokončení kroku aktualizuje jednotky, viditelnost a skriptované události a volá `CheckReactionFire(unit, this_pos)`. Nový kontakt se posuzuje podle sdíleného dohledu *bránící se strany*, nikoli jen podle dohledu střelce.
- **Teleport:** po dokončení příletu/přemístění `TELEPORT_OUT` je voláno `CheckReactionFire(unit, MapXY())` (okolo řádku **14026**). Neplatná předchozí pozice zde znamená, že podmínka „na předchozím poli byl už vidět“ je záměrně vynechána.
- **Přerušení:** reakční fronta se vyřizuje před dalším krokem nepřátelského tahu v `Tick` (okolo řádku **13620**); střelba probíhá přes standardní útok, pohybující se jednotka je po sekvenci obnovena, pokud přežije.
- **Bezpečnost:** před každým zpožděným výstřelem `ProcessReactionFire` znovu ověřuje živé jednotky, AP, aktivitu, zaměstnanost, vlastní přímý dohled i legální dostřel. To je žádoucí pro plynulé animace a prevenci zaseknutí.
- **Neviditelný nepřítel:** optimalizace přeskakování animace nesmí přeskočit logické kroky, spotřebu AP, kontakty ani reakční palbu; dokument `HIDDEN_ENEMY_SIMULATION_REGRESSION.md` tuto podmínku výslovně popisuje. Jeho historický status však sám neprokazuje dnešní runtime chování.

### 5.2 Co ukazuje původní binárka

V originálu jsou **čtyři skutečné call site** rutiny `0x9BE32` napojené na kód pohybu. Před nimi jsou testy **sdílených mapových příznaků viditelnosti strany** (viz adresy `0x1D690` a `0x28890`); po návratu se podle počtu kandidátů nastavují stavové příznaky přerušení. Jednotliví střelci následně procházejí testem legálního dostřelu `0x62562`, nikoli vlastního dohledu `0x625BE`. To dokládá model „nový kontakt pro stranu při pohybu → obranná reakce“, nikoli libovolný automatický protiútok po zásahu.

**Z této samotné disassemblace ale ještě neplyne**, že všechny čtyři větve představují právě normální krok a teleport pro obě strany. Nemáme dokončené pojmenování datových příznaků ani snímkový průběh DOS hry. Není proto poctivé tvrdit, že originální teleport vždy vytváří nový kontakt, nebo že určitě nevytváří.

### 5.3 Jediná opravdu podezřelá hranice, kterou má cenu cíleně testovat

Reloaded pro teleport předává **neplatnou předchozí pozici**. Tím dovolí vyhodnotit nový kontakt i v případě, že jednotka teleportovala **z jednoho již viditelného místa na jiné viditelné místo**. U normálního pohybu by v takové situaci reakce nevznikla, protože obránce cíl viděl i v předchozím bodě.

Je to **konzistentní rozhodnutí remaku**, nikoli prokázaný bug. Nevíme, zda originál přesně takto chápe teleport jako nový kontakt nebo vychází z jiných již existujících mapových příznaků. Právě tento jeden hraniční scénář má vyšší diagnostickou hodnotu než obecná otázka „funguje teleport?“.

Pokud ani po teleportu obránce nemá AP, nemá přímý dohled, cíl není v legálním dostřelu nebo nevyhraje příslušný test, žádná střelba nevznikne. Teleport proto **neznamená bezpodmínečnou reakci**.

### 5.4 Časování versus mechanika

„Přesné časování“ zahrnuje nejméně tři různé věci:

1. **Logické pořadí:** přesun souřadnic → aktualizace dohledu a událostí → možnost obranné střelby → pokračování přesunu. Záměr Reloaded je správně přerušit pohyb bez přeskočení AP nebo událostí.
2. **Okamžik volby střelce:** originál má samostatnou rutinu vybírající jeden útok z aktuálního seznamu; Reloaded si připraví frontu výstřelů předem. To může změnit **pořadí**, i když se všechny útoky nakonec odehrají.
3. **Vizuální/sound timing:** počet snímků teleportace, skryté animace, přepnutí výběru a přehrání zvuku. Tyto věci bez runtime záznamu originálu **nelze označit za přesnou shodu**; samy o sobě ale neurčují správnost výpočtu iniciativy.

**Praktický závěr:** Není doložená chyba teleportace nebo zastavování pohybu, která by vyžadovala preventivní zásah. Máme vymezené okrajové testy, nikoli verdikt o rozbité mechanice.

## 6. Shrnutí toho, co se oproti původní revizi změnilo

| Předchozí formulace | Výsledek hlubší analýzy | Stav |
| --- | --- | --- |
| „První střelec: použitá aproximace“ | Originál **opravdu násobí efekt útoku počtem střel**. Reloaded respektuje základní vzorec, zjednodušuje odhad efektu a předem fixuje pořadí. | **Podstatně vysvětleno; omezené rozdíly doloženy** |
| „RNG není identický s DOS“ | Originál volá vlastní funkci `0x575DC`, dvakrát s mezí 6; originál losuje nejprve za pohybující se jednotku, Reloaded za obránce. | **Doložená technická odlišnost, ne prokázaná chyba vyvážení** |
| „Teleportace/časování neověřeno“ | Zkontrolován stavový automat remaku a čtyři call site originálu. Kritický test je teleport z viditelného na viditelné. | **Neprokázaná odlišnost; vymezena přesná hranice testu** |
| „Dělo musí nepřítele samo nově spatřit“ | **Opraveno.** Originál testuje novou sdílenou viditelnost celé strany, pak samostatně dostřel děla. Stejně nyní postupuje Reloaded. | **Prokázaný rozdíl a cílená oprava** |
| „Iniciativa vždy rozhodne“ | Originál má i podmíněnou větev bez dvojice RNG volání; její stavové předpoklady zatím nejsou zcela dekódované. | **Nově zjištěná nuance, nelze zatím portovat** |

## 7. Doporučení pro projekt

**Jedinou odhalenou odchylku ve sdílené viditelnosti jsme cíleně opravili.** Ostatní zkoumané body (priorita, RNG, teleport a speciální stavová větev) nepředstavují prokázanou závadu vyžadující preventivní zásah. Další změny naslepo by mohly být horší než současný mechanismus.

Pokud bude cílem *striktní mechanická shoda*, doporučuji následující pořadí výzkumu (nejde o povinné opravy):

1. **Priorita obránců:** ve dvoustranném testu se dvěma či třemi reagujícími jednotkami zaznamenat pořadí jejich výstřelů. Ověřit, zda original přepočítá vítěze po každém výstřelu. Zvážit port původního hodnotitele `0x9B907` pouze tehdy, bude-li ověřen celý jeho kontext.
2. **Speciální větev `0x9C02E`:** rozklíčovat stavové příznaky, které umožňují obrannou palbu bez obvyklého hodu iniciativy; bez toho neměnit pravidla šancí.
3. **Teleport:** otestovat přesně čtyři kombinace `neviditelný→neviditelný`, `neviditelný→viditelný`, `viditelný→neviditelný`, `viditelný→viditelný` s jedním obráncem, rezervou AP a pevným cílovým polem; opakovat za obě strany a se změnou výšky terénu.
4. **RNG až nakonec:** implementovat původní generátor pouze jako součást většího cíle deterministické DOS parity (včetně seedu a všech odběrů), nikoli kvůli běžné reakční střelbě.
5. **Regrese:** udržet testy přežití/zničení pohybující se jednotky, přerušení animované střelby, spotřeby AP, dokončení tahu a stavu výběru po návratu z přerušení.

## 8. Technické kotvy pro opakování analýzy

### Původní EN `SPELCROS.EXE` (adresy LE objektu)

| Adresa | Úloha / důkaz |
| --- | --- |
| `0x1D690–0x1D6C9`, `0x28890–0x288B8` | Test nového kontaktu ve sdílených příznacích mapy a volání sběru obránců |
| `0x62562` | Kontrola dostřelu zbraně volaná z reakční rutiny `0x9C012` / `0x9C04F` |
| `0x625BE` | Odlišná kontrola vlastní viditelnosti jednotky; v těchto větvích reakční rutiny se nevolá |
| `0x575DC` | Původní omezený RNG, návrat `0..limit-1` pomocí zbytku po dělení |
| `0x9BE32` | Sběr kandidátů na obrannou palbu |
| `0x9BFBD–0x9BFEF` | Iniciativa, dvě volání `RNG(6)`, striktní porovnání |
| `0x9C095` | Hodnota iniciativy z původních dat typu jednotky a zkušenosti |
| `0x9C02E` | Speciální kandidátní větev bez dvojice RNG volání |
| `0x9B6C1` | Kontrola možné účinnosti/typu útoku |
| `0x9B907` | Podrobnější projekce účinku útoku |
| `0x8D77D` | Přenos seznamu vybraných kandidátů |
| `0x8D7E7` | Výběr jedné nejlepší dvojice střelec–cíl |
| `0x8D97A–0x8D9A3` | Výpočet počtu výstřelů podle zbývajících AP |
| `0x8D9D9` | Volání projekce účinku `0x9B907` |
| `0x8D9E7` | Násobení projekce počtem zbývajících výstřelů |
| `0x8D9EE–0x8DA08` | Porovnání priorit (striktní `>`); zapsání vítěze |
| `0x1D6C9`, `0x1DC9A`, `0x288B8`, `0x28D07` | Volání kandidátní rutiny z pohybových větví |

**Poznámka k reprodukci:** EN soubor je `MZ` s vnořenou hlavičkou `LE` na offsetu `0x2C80`. První kódový objekt má virtuální bázi `0x10000`, stránkový začátek dat v souboru `0x3EC00` a mapu stránek LE v hlavičce; pro disassemblaci je nutné kódové stránky správně rozbalit/mapovat. Pouhé `objdump -D` nad celým EXE počítá nesprávné offsety. Výše uvedené adresy jsou vztaženy ke správně mapovanému prvnímu objektu.

### Kód a projektová dokumentace

- [Reloaded `map.cpp`](https://github.com/luboshorak/spellcross-reloaded/blob/3d6b41f443639b32f053b0d1a3d2adbdd15dce6f/spellcross-map-edit-main/source/map.cpp) — `CheckReactionFire`, `ProcessReactionFire`, `Tick`; cca řádky 10316–10477, 13620–13665, 14026, 14147.
- [Reloaded `spell_units.cpp`](https://github.com/luboshorak/spellcross-reloaded/blob/3d6b41f443639b32f053b0d1a3d2adbdd15dce6f/spellcross-map-edit-main/source/spell_units.cpp) — `GetFireCount`, `GetMaxFireCount`, `GetAttack`.
- [Issue #188 — technická revize](https://github.com/luboshorak/spellcross-reloaded/blob/main/docs/issue-188-tactical-reaction-fire.md).
- [Dosavadní reverzní analýza originálu](https://github.com/luboshorak/spellcross-reloaded/blob/main/packaging/original-mechanics-parity.md).
- [Registr oblastí čekajících na ověření](https://github.com/luboshorak/spellcross-reloaded/blob/main/docs/verification.md), položka V08.
- Dodaný původní český manuál, část *Útoky na bojové mapě / Střelba z obrany*.

---

**Konečný verdikt (po doplnění):** Základ obranné palby Reloaded odpovídá originálu, ale jedna předchozí podmínka byla chybná: vyžadovali jsme vlastní dohled každého reagujícího střelce, což původní EN algoritmus **nevyžaduje**. Oprava nyní využívá nově získanou viditelnost celé obráncovy strany a samostatně kontroluje legální dostřel jednotlivých zbraní. Ostatní rozdíly v odhadu účinku, volbě pořadí, RNG a teleportaci zůstávají dokumentované a bez dodatečného ověření nebudou měněny.
