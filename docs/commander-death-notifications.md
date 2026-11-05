# Commander casualty information

The retail COMMON.FS language table OTHER contains index 40 (`zahynul` in CZ,
`died` in EN) and index 41 (`Velitel` / `Commander`). The new notification uses
these original bytes with the actual campaign commander's name, converted from
the existing UTF-8 roster to the original CP895 message encoding. No new graphic
or replacement information window is introduced. Exact original punctuation,
rank placement and timing have not been independently reproduced in DOSBox.

The existing RecordMissionLoss path now queues one information message for each
officer on a destroyed allied hierarchy host. Both attack resolution and dead
unit cleanup already use that path. The host mask remains authoritative;
ordinary companies and enemy hosts do not generate allied commander messages.
Stable company UIDs resolve names from the current strategic hierarchy, including
campaign context restored from tactical saves. Legacy standalone positions without
campaign names use only the original generic title and casualty verb.

Messages wait for the current message window, retain their text until dismissal,
and are shown sequentially before the mission result. They use a separate queue
so completion of the last objective cannot discard them. Existing morale shocks,
formation recalculation, permanent commander removal and save formats are unchanged.
The transient presentation queue is cleared when a map/save is loaded; already
removed casualties do not produce another message on reload. Pending information
windows themselves are not serialized.

The retail hierarchy integration test covers three named officers on one host,
busy-window queuing, sequential dismissal, repeated cleanup, ordinary/enemy
casualties, tactical reload before/after the loss, and surviving formation bonuses.
