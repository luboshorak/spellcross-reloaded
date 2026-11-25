# Retail campaign ending (CZ and EN)

Original CZ executable (LE code object mapped at 0x10000):
- 0x10CCE: terminal territory success sets state 7; 0x10F25..0x10F35: chapter 10 changes state to 5.
- 0x110C1..0x11113: state 5 plays TITAN1.CAN, TITAN2.CAN, S_10.DP2 (+ S_10 audio), LOGO.CAN, CREDITS.DPK in this order.
- 0x1106D..0x11099: state 4 plays S_OVER.DP2 (+ S_OVER audio) and returns to menu state 1.
- 0x10E32..0x10E8A: campaign loss flag or no player-owned territories sets state 4.
- 0x29C1D..0x29C2E: destruction of type 0x53 (83, Alexander) sets the loss flag.

EN executable: 0x11328..0x11354 and 0x1137C..0x113CE confirm the same loss and victory sequences, using TITAN1.SMK, TITAN2.SMK, LOGO.SMK and CREDITS.SMK. DP2 names are identical. CZ MOVIE.FS and SPEAKER.FS contain all referenced files. The retail LEVEL_10.DEF End(3) and M10_03A -> M10_03B successor chain establish the final mission. No PlayEnd commands are defined for the ending.

The restored engine appends the executable-authored sequence to the existing final chapter queue, not to ordinary mission victories. Losing all strategic territories or Alexander triggers the separate defeat movie. The executable has no direct evidence that every failed/retreated M10_03A/B battle should be treated as terminal; that is not assumed. Existing congratulations/defeat dialog then returns to menu. No game data redistributed. Runtime Windows validation is required.
