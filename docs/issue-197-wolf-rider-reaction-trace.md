# Issue #197 — opt-in Wolf rider reaction-fire trace

This is **diagnostic instrumentation only**. It does not modify the reaction-fire
contact trigger, eligible shot/range checks, AP consumption, initiative thresholds
or RNG sequence. It only emits records while a **Wolf riders** unit
(`JEDNOTKY.DEF` type ID **61**) moves/elicits a reaction.

## Testing on Windows (EN ISO)

1. Build the branch `debug/issue-197-wolf-rider-reaction-fire` normally.
   Use the same English original-game data as in the issue.
2. Open PowerShell **in the folder containing the just-built executable**
   and launch the game from that same PowerShell window:

   ```powershell
   $env:SPELLCROSS_TRACE_REACTION = '1'
   .\Spellcross.exe
   ```

   If your executable has another name or lives in a subfolder, use its real
   executable path instead. The environment variable must be set **before**
   starting the game. Other launches without this variable behave normally
   and do not produce `REACTION197` messages.

3. Load Divac1's **original, unmodified** `test.scsave`.
4. Reproduce the recorded sequence: kill the injured Magotar with a distant
   Commando; end turn; heal the injured Commando during the following free
   player phase; end turn again; watch the Wolf rider approach.
5. Close the game normally. Find `debug.log` in the game's writable runtime
   folder (usually next to the executable); if a log rotated, also collect
   `debug.1.log` to `debug.3.log`.
6. Send the `debug.log` files. Entries for this experiment are tagged
   **`REACTION197`**. A quick PowerShell extraction in the log directory:

   ```powershell
   Get-ChildItem -File -Filter 'debug*.log' |
       Select-String 'REACTION197' |
       Out-File -Encoding utf8 'issue197-trace.txt'
   ```

The diagnostic log may contain local game paths; inspect it before posting
to a public issue. A separate save or video is useful for temporal context,
but the trace is what identifies the rejected condition.

## Reading the trace

- `STEP mover=… from=… to=… seen_before=… seen_now=… new_contact=…` —
  contact gate at **every movement step**, including steps that do not fire.
- `SPOTTER observer=… current=… previous=…` — friendly units that can see
  the current/previous tile. The earliest `STEP new_contact=1` is the contact
  moment, **not** necessarily the tile where a weapon enters firing range.
- `NO_TRIGGER reason=already_seen_before` — the enemy is advancing within
  already-established side-wide sight, so the candidate and initiative tests
  are not run. `NO_TRIGGER_SHOOTER` gives AP, shots, nominal range and
  distance for context; this is **not** a true legal-attack-map test.
- `REJECT shooter=… reason=…` — candidate failed at contact time. For
  `outside_legal_attack_map`, consider terrain, LOS, minimum artillery range
  and weapon range.
- `ROLL … def_roll=… mover_roll=… pass=…` — the **actual** existing two
  RNG rolls, with no additional random draws.
- `RESULT candidates=… queued_shots=…`, `DEQUEUE`, `DROP`, `FIRED` —
  whether a qualifying shot survived from candidate selection to animation.

### Expected diagnostic fork

If the Wolf rider is first revealed by a long-sight scout/Hummer but all
weapons are out of legal range, the log should show `new_contact=1`,
`REJECT … outside_legal_attack_map`, then later
`NO_TRIGGER reason=already_seen_before` despite improved range.
If the first contact happens **within** range and a high-initiative Commando
still does not fire, the log will instead show whether `REJECT`, `ROLL`,
queue discard, or attack start is responsible.

Do not merge this instrumentation to `main` as a gameplay fix; first establish
the cause from an actual player trace and compare the outcome to the retail
English executable where feasible.
