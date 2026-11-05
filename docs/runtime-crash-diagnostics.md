# Runtime crash diagnostics (volatile-threads branch)

Spellcross Reloaded writes a **bounded** game-session log to **debug.log** in
its runtime directory (normally the folder containing Spellcross.exe). It
rotates to debug.1.log, debug.2.log, and debug.3.log at **512 KiB per file**:
approximately **2 MiB maximum** for all four logs, even if the game is played
for weeks. Oldest entries are discarded on rotation; saves are never touched.

## What is recorded

- Startup and build version, relevant map/mission load and launch failures.
- Strategic turns, territory and mission transitions, completion or failure.
- Continue and tactical/strategic save/load success and failure.
- Enemy AI phase start/end, meaningful movement and attacks; company deaths and spawns.
- A gameplay heartbeat once per minute **while a tactical map is visible**.
- Worker stalls (>5 seconds), caught tactical exceptions, unhandled native
  Windows exceptions (code/address/RIP on x64), and unhandled C++ exceptions
  terminating a worker or the process.
- Clean application exit (helps distinguish a crash from a normal quit).

Ordinary 10 ms timer ticks, animation frames, cursor movement, rendering, and
per-tile line-of-sight operations are **not logged**. Each line includes local
timestamp, severity, process ID and thread ID. Runtime logging is best-effort
and never prevents the game from starting if the folder cannot be written.

## How to report a crash

1. Do not delete the files, or repeatedly restart/play for hours before copying
   them (rotation will eventually overwrite the evidence).
2. Send **debug.log** and, if possible, **debug.1.log**, **debug.2.log** and
   **debug.3.log** from the game folder. Tell us what action happened just
   before the hang/crash.
3. If the process dies without a CRASH record (for example a forced kill or
   hardware fault), include Windows Event Viewer > Windows Logs >
   Application > Application Error (event 1000) and Windows Error Reporting
   (event 1001) entries for Spellcross.exe. A Windows crash dump is even more
   useful and may be needed to identify the exact instruction.

Logs are diagnostic breadcrumbs, not a replacement for crash dumps. A frozen
process cannot write future log entries, but the last successfully written
events and any worker-timeout warnings remain. The log does **not** continuously
save the campaign or alter the behavior of the **Continue** button.

Players should inspect logs before sharing; even file paths or usernames in
third-party error messages may be sensitive. The game does not automatically
upload diagnostics anywhere.
