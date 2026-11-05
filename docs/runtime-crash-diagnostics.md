# Runtime crash diagnostics

Spellcross Reloaded writes rotating game-session breadcrumbs to `debug.log` in
its writable runtime folder (normally next to `Spellcross.exe`). The four
`debug*.log` files are limited to about 2 MiB total: 512 KiB each, with three
older files `debug.1.log` through `debug.3.log`. This log does not save the
campaign or change the Continue behavior.

## New automatic crash reports (Windows)

A fatal native SEH exception (such as access violation `0xC0000005`) or
unhandled C++ `std::terminate` now **attempts** to write:

| File | Contents |
| --- | --- |
| `crash-last.txt` | Exception code and access READ/WRITE/EXECUTE address (SEH), thread ID, registers, build identity, and up to 32 call stack frames (module+RVA; function, source path and line when matched symbols are available). |
| `crash-last.dmp` | Windows minidump with exception context, thread stacks and loaded/unloaded module information. |
| `crash-prev.txt`, `crash-prev.dmp` | The preceding crash, rotated only when a *new crash* happens. |

The text report and call stack are **also appended to `debug.log`** so that
one log is often enough to identify the relevant function. Build metadata
(version, Git commit and executable SHA-256) is read from `BUILD-INFO.txt`
in packaged releases; a local build without it still reports module+RVA.

The normal release packager now includes `symbols/Spellcross.pdb` **matching
the packaged game executable**. DbgHelp can then resolve call stacks all the
way to source file and line at crash time: no need for players to send the EXE
or locate `.pdb`/`.ipdb` themselves. If symbols are missing or Windows
DbgHelp cannot initialize, the report falls back to module-relative addresses
and the minidump can be analyzed later with the matching build.

There is **no guarantee** a severely corrupted process can execute its own
crash handler, walk stacks or write a minidump (particularly stack overflow,
memory corruption or forced termination). Windows Error Reporting / Event
Viewer remain fallbacks. Dumps capture only a limited set of memory by default,
not full process RAM; a minidump may still include private strings or paths.

## What is logged while playing

- Game startup and build identity.
- Mission/map transitions, save/load, victory/loss and relevant strategic actions.
- AI turn start/end and significant movements/attacks.
- A tactical heartbeat once a minute.
- Caught exceptions, worker stalls (over 5 seconds), unhandled failures and
  normal shutdown.
- No per-frame cursor/timer/animation spam; the log remains bounded.

## Reporting a crash

1. Do not restart the game many times before copying crash files; the two
   most recent crash reports are retained and older logs rotate.
2. Send `debug.log` and `crash-last.txt` from the game folder. If the text
   does not explain the crash, send `crash-last.dmp` as well. Older
   `debug.1.log`–`debug.3.log` or `crash-prev.*` can help with regressions.
3. Mention which mission/map was running and what action you performed.
4. If no report exists, supply Event Viewer → Windows Logs → Application →
   Application Error (1000) and Windows Error Reporting (1001), or a Windows dump.

**Privacy:** Review files before sharing. Minidumps, logs and PDB symbol paths
may reveal usernames, local file paths, game-state text or other in-memory
information. Crash diagnostics are never automatically uploaded.

## Verification and operational notes

The regression test `spellcross-map-edit-main/tests/test_runtime_diagnostics.cpp`
simulates an unhandled access violation and verifies the report, access
address, stack, rotating previous report, build fingerprint and minidump.
The Windows job in `.github/workflows/fullscreen-menu.yml` compiles and runs
this test with MSVC. This is a writer test, **not** proof that every real
crash can be captured.

A new build is required before these changes take effect. Debug artifacts
are best-effort; they do not turn an underlying memory-safety bug into a
recoverable error.