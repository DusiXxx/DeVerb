# DusiX DeVerb v0.5

Delay, reverb, filtr s LFO, chorus a lo-fi, 3D prostor, EQ ve stylu FabFilter a prohlížeč presetů. Formát **Audio Unit** (Logic Pro) + VST3 + samostatná aplikace.

## Instalace / aktualizace

1. Zavři Logic.
2. V Terminálu: `bash /Users/dusix/Downloads/DusiX-DeVerb/build.sh`
3. Až skript napíše HOTOVO, otevři Logic: **Audio FX → Audio Units → DusiX → DusiX DeVerb**.

Kamarádovi stačí poslat zip z GitHubu (Releases) – uvnitř je návod `JAK NAINSTALOVAT.txt`, žádný Terminál.

## Aktualizace z GitHubu (nové ve v0.5)

Vedle loga je **tlačítko verze** (`v0.5.0 · aktuální ✓`). Plugin se sám podívá na GitHub nejvýš jednou za 6 hodin, klik = zkontrolovat hned.
- **AKTUALIZOVAT x.y.z** (zelené) → klik stáhne novou verzi a ověří ji (velikost + SHA-256).
- **INSTALOVAT x.y.z** → klik → potvrdíš → **zavři Logic**. Instalace proběhne sama na pozadí (stará verze se zazálohuje, při chybě se vrátí), přijde oznámení.
- **RESTARTUJ LOGIC ✓** → nová verze je nainstalovaná, stačí znovu otevřít Logic.
- Soubory: `Hudba / DusiX DeVerb / Aktualizace` (záloha, `install.log`).

Novou verzi vydává GitHub sám (`.github/workflows/release.yml`): když se na `main` změní verze v `CMakeLists.txt`, postaví plugin na Macu v cloudu, zabalí ho a zveřejní `latest.json`.

## Okno (960 × 760)

Nahoře logo s verzí, záložky **PROSTOR & EQ** a **EFEKTY**, **ECO** (statický náhled bez animací pro starší Mac) a **Zpět / Vpřed**.
Pod tím preset (název + kategorie, klik = prohlížeč), **Náhodný**, **Uložit…** a **A | B** s kopírováním **A→B**.

### PROSTOR & EQ
- **PROSTOR** – svítící bod táhneš myší (doleva/doprava = pan, dopředu/dozadu = vzdálenost), úchyty nebo kolečko = šířka.
- **STEREO** – Pan, Šířka, Bass Mono, Output, SAFE a měřič korelace.
- **TEMPO** – tabulka časů not pro tempo projektu.
- **EQ** se přepíná mezi dvěma režimy (každý má vlastní vypínač ⏻):
  - **VÝSTUP** – 8 pásem na celý zvuk: Low/High Cut (12/24/48 dB/okt.), Low/High Shelf, Bell, Notch. V pozadí je analyzátor spektra.
    *Dvojklik do grafu = nové pásmo, táhni bod, kolečko = Q, pravé tlačítko = typ a strmost, dvojklik na bod = smazat.*
  - **DECAY** – jako ve FabFilter Pro-R: 4 body určují, jak **dlouho** dozvuk zní v basech, středech a výškách (×0.25 až ×4).
    Bod nahoru = delší dozvuk v tom pásmu. *Dvojklik na bod = zpět na ×1.* Typicky: kratší basy (míň bahna), delší výšky (vzduch).

### Úspora výkonu (v0.4, platí dál)
- Grafika se kreslí jen když se něco změní (statické části jsou uložené v paměti), animace běží na 15 snímků/s (ECO 8).
- **ECO** navíc vypne analyzátor a animace – okno pak skoro nic nestojí.
- Vypnutý efekt se vůbec nepočítá. Když nic nehraje a dozvuky doznějí, plugin „usne“ a nepočítá nic.
- Měřeno: grafika hlavní stránky −73 %, v ECO −94 %; zvuk s typickým nastavením −25 až −60 %, ve vypnutém/tichém stavu skoro 0.

### EFEKTY
- Každý efekt má dole **vizualizaci**: delay = opakování v čase, reverb = obálka dozvuku (pre-delay, early, decay −60 dB), filtr = LFO na 2 takty + odezva filtru, chorus = pohyb hlasů L/P, lo-fi = „schody“ signálu.
- **Řetěz** nahoře: POZICE → DELAY, REVERB, FILTR, CHORUS, LO-FI (pořadí přetáhneš myší) → EQ.
  **Klik na blok** otevře jeho nastavení, **tečka** efekt zapne/vypne.
- **Delay** – sync, feedback, tone, ping-pong, mix.
- **Reverb** – TYP (Plate, Room, Chamber, Hall, Spring), EARLY (odrazy z 3D místnosti), **SHIMMER** (oktáva nahoru ve dozvuku),
  **FREEZE** (dozvuk hraje donekonečna, nový zvuk do něj nejde), pre-delay/decay do tempa.
- **Filtr s LFO** – LP/BP/HP, LFO v tempu, 5 tvarů, stereo.
- **Chorus / ensemble** – rozvlnění a rozšíření (pady, el. piano, synthy). Před reverbem = širší a měkčí prostor.
- **Lo-fi** – bity, vzorkování, tón (retro sampler, rádio).
- **Dynamika efektů** (dole, platí pro delay i reverb):
  **DUCKING** – ozvěna a dozvuk ustoupí, když zdroj hraje, vrátí se v pauzách (RELEASE = jak rychle);
  **KOMPRESE** – srovná dozvuk, aby nevyskakoval.

### Presety
- 60 továrních v kategoriích Univerzální, Vokály, Bicí, Piano & klávesy, Synthy, Kytara, Kreativní a Mix busy.
- Klik na název presetu v hlavičce otevře **prohlížeč**: kategorie, hledání a **hvězdička** pro oblíbené.
- **Uložit…** uloží tvůj preset do `Hudba / DusiX DeVerb / Presety`. Soubor `.dvpreset` můžeš poslat kamarádovi, stačí ho dát do stejné složky (tlačítko **Otevřít složku** v prohlížeči).
- **A/B** – dvě nastavení, mezi kterými přepínáš. **A→B** zkopíruje aktuální nastavení do druhého slotu.
- **Náhodný** – náhodné nastavení efektů pro inspiraci (EQ a stereo zůstanou).

## Co je kde

| Soubor | Co dělá |
|---|---|
| `Source/Dsp*.h` | Zvuk: delay, reverby (Plate/Room/Chamber/Hall/Spring), early reflections, decay EQ, shimmer, freeze, filtr s LFO, chorus, lo-fi, ducking, komprese, výstupní EQ |
| `Source/PluginProcessor.cpp` | Parametry, výpočty podle tempa, A/B, zpět, ukládání |
| `Source/Presets.cpp` | Tovární presety |
| `Source/PresetManager.cpp`, `PresetBrowser.cpp` | Vlastní presety a prohlížeč |
| `Source/MainPage.cpp`, `SpaceView.cpp`, `EqView.cpp` | Stránka PROSTOR & EQ |
| `Source/EffectsPage.cpp`, `ChainStrip.cpp` | Stránka EFEKTY |
| `Source/PluginEditor.cpp`, `Widgets.cpp` | Hlavička, záložky, tlačítko verze, společné ovládací prvky, písmo Inter |
| `Source/EffectViews.cpp` | Vizualizace efektů, FREEZE, měřič ducking |
| `Source/Updater.cpp`, `Resources/deverb-install.sh` | Aktualizace z GitHubu a instalátor |
| `Resources/Fonts` | Písmo Inter (SIL Open Font License) |
