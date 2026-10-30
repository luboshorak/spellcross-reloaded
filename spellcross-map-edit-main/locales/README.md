# Spellcross Reloaded UI localization

This directory contains **Reloaded-authored UI translations only**. Do not copy
commercial game narrative or mission texts here. Original names, research and
briefings continue to come from the imported edition's archives (including
`TEXTS.FS`, `JEDNOTKY.DEF`, and native `COMMON.FS` art).

## Format and runtime behavior

- `en.lang` and `cs.lang` are UTF-8 text files, `key=value`, one string per line.
- Lines starting with `#` or `;` are comments. Values may contain `=`.
- Keys must not change when a translation changes, and cannot be duplicated.
- A blank/missing translation uses the English catalog; if English is missing,
  the English fallback literal passed to `spellcross_ui_locale::Tr` is used.
- At startup, `main.cpp` selects `en` or `cs` from the
  `JEDNOTKY.DEF` retail-edition decoder, **not** from Windows locale.
- Catalogs are loaded from `locales/` beside the executable or the game's
  selected runtime root. Release packaging copies the entire `locales` folder
  beside `Spellcross.exe`.
- `SetLanguage("pl")` or `SetLanguage("de")` can later load `pl.lang`
  or `de.lang` without changing the loader; missing keys fall back to EN.

## Add a new language

1. Copy `en.lang` to e.g. `pl.lang`. Translate values, never keys.
2. Keep the same key set. Use the regression checker below.
3. Add recognition of that particular **retail edition** to the game's data
   loader before expecting automatic selection. Currently the game's
   `JEDNOTKY.DEF` decoder recognizes two record layouts: Czech and English.
   Additional retail editions must be reverse-engineered and validated
   rather than guessed from ISO filenames or OS settings.
4. Verify glyph coverage and visual fit in the original small fixed-width
   Spellcross UI. Original artwork may embed language-specific text, which
   cannot be replaced by this UI catalog without changing the artwork.

Run the catalog consistency check:

```shell
python spellcross-map-edit-main/tests/test_ui_localization_catalog.py
```

## Integration QA (Windows / original legally owned ISOs)

- Start clean EN data: menus, original strategic screens (map, statistics,
  options), result fallbacks and original text briefings should be English.
- Start clean CZ data: the same screens should be Czech, with correct accents.
- Test CR, LF and CRLF wrapped mission texts and actual word boundaries.
- Make sure changing the imported edition does not reuse stale archives.
- Inspect remaining hard-coded strings in tactical, editor, secondary dialogs,
  research and custom screens. This patch is the **first migration pass**, not
  full localization certification.
