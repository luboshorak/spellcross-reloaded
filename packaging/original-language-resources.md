# Original Spellcross language resources

The original retail `COMMON.FS` contains **14 independent positional text tables**. Czech releases contain both `.CZ` and `.ENG`; English releases normally contain only `.ENG`. Reloaded selects the table extension from the **actual JEDNOTKY.DEF record layout**, not the user's operating-system locale.

These files are **original game data**. They are not replacements for Reloaded-authored `locales/cs.lang` and `locales/en.lang`, and they are not part of the distributed source code.

| COMMON.FS resource | Original purpose | Current use |
| --- | --- | --- |
| `UNITS` | 87 unit-type display names, by type ID | Loaded into in-memory unit display names, retaining binary stats from `JEDNOTKY.DEF` |
| `HODNOSTI` | Nine pairs of full rank title and abbreviation | Commander rosters, hierarchy, purchase screen and statistics |
| `BIG_MAP` | Native strategic unit/shop captions and categories | Original-style strategic panels |
| `BUTTONS` | Names/tooltips of original action buttons | Native action captions at corresponding positions |
| `DISPLAY` | Tactical attack/movement display mode text | Loaded and available for original-mode UI |
| `INFOTEXT` | Unit information labels | Original-style unit/status captions |
| `MESIACE` | Twelve month names | Loaded and available for original campaign date display |
| `MOSTY` | Four bridge-type names | Bound to destructible bridge class display names |
| `MURY` | Four wall-type names | Bound to destructible wall class display names |
| `OTHER` | Strategic headings, statistics and miscellaneous UI text | Original-style strategic, research and stats screens |
| `RESEARCH` | Research and encyclopedia item names by item ID | Research/Info titles, not loose cached files from another edition |
| `SELTAB` | Tactical action-selection labels | Loaded and available for tactical selection UI |
| `SPECOBJ` | Names of original special object classes | Bound to special-object class display names |
| `UPGRADES` | Upgrade item names by upgrade ID | Upgrade choices and installed-item labels |

The `DISPLAY`, `MESIACE` and `SELTAB` tables are accessible through `SpellData::GetOriginalText`; they are **not** forced into unrelated UI elements when the corresponding original display is not implemented. This avoids inventing mappings or altering gameplay.

Table parsing strips the DOS `0x1A` EOF marker and comment lines beginning with `;`, handles CR/LF/CRLF, and verifies minimum source counts. The `UNITS` and `HODNOSTI` layouts receive stricter checks (87 unit names and 18 rank strings). The parsed values stay in their original 8-bit encoding until converted for wxWidgets display.

**Source-of-truth rule:** never prefer a loose Czech `.CZ` file merely because it exists in `temp/COMMON` when the selected game edition is English. Existing original mission text remains sourced from `TEXTS.FS`; new UI-only strings remain in Reloaded's `locales/*.lang`.
