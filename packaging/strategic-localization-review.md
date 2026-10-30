# Strategic localization and original text reflow

Updated on 2026-10-30, branch `reverse`.

## Observed causes and changes

- `SpellData` previously always decoded `TEXTS.FS` using
  `SpellLang::CZE`, independently of the imported EN/CZ retail edition.
  It now uses the already loaded `JEDNOTKY.DEF` edition signal
  `SpellUnits::IsEnglish()`. The same edition signal drives new strategic
  labels. Windows locale must not determine game language.
- `SpellTextRec` reflow previously recognized only CRLF line breaks.
  Original display-width wraps using lone CR/LF could leave hard breaks
  or run words together; the decoder now handles CRLF/CR/LF consistently
  with existing paragraph separation.
- Selected strategic territory briefing, counterattack and status labels,
  navigation buttons, notebook pages and roster captions now have EN/CZ
  counterparts. The original-style strategic status footer is also
  edition-aware.

## Scope and limitations

This is an incremental localization pass, **not a certified translation of
every hard-coded Reloaded message**. Full UI strings span tactical gameplay,
strategic submenus, settings, modal warnings, editing and import flows; many
are still manually authored in English or Czech. Art and original data texts
may themselves differ by edition. Do not claim entire UI localization parity
until these are inventoried and tested.

## Suggested integration checks

1. Compile Release x64 and open a fresh original EN import; check that English
   briefings, navigation and original-style strategic status are English.
2. Import original CZ data into a separate configuration; check Czech accents
   and text reflow, including previously joined words and paragraph breaks.
3. Test switching editions without a mixed/stale `game_data` directory.
4. Check map view, unit roster, resources, research, hierarchy, mission result,
   counterattack prompts, tactical interface, settings and save/load dialogs
   for still-hardcoded strings; record the exact screens for the next pass.
5. Check source text with CRLF, LF and CR separators and genuine split words;
   do not insert spaces into legitimately unbroken names or control markers.

The changes have been reviewed from source. A fresh MSVC build and an in-game
EN/CZ comparison are still required.
