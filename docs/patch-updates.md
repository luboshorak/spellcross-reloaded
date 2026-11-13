# Optional patch updates (experimental)

The patch system is developed in feature/optional-auto-patching, not in main.

## Player flow

When Spellcross.exe starts, the release checker requests published GitHub releases.
If there is a newer comparable version, a dialog offers Install patch or Not now.
Declining continues into the installed game. Network errors never block gameplay.
The Info window retains Check for updates and Open releases and adds an Install
patch button for a newer release. "--no-update-check" bypasses one startup check.

When accepted, the game starts updater/Install-Patch.ps1 as a separate Windows
PowerShell process, then exits before any executable is replaced. The updater
downloads and validates SHA-256 signed-by-manifest ZIPs, including intermediate
releases if the player skipped versions, backs up replaced files, and restarts
Spellcross.exe if the update succeeds.

## One-time bootstrap requirement

Old packages, including v0.9.5, do not contain this client. Players need to
download ONE complete ZIP/SFX release with the updater before using patches.

## Publisher workflow

1. Prepare the complete ZIP/SFX with `-PreviousStageDirectory` and
   `-PreviousVersion`. A normal versioned release refuses to proceed without
   these inputs; `-NoPatch` is an explicit bootstrap/full-only exception.
2. Test the generated patch against a copy of the previous installation.
3. Run `packaging/Publish-Release.ps1`. It validates the full ZIP, manifest,
   patch filename and SHA-256 before creating the release with every asset.
4. Use exact version labels from app_version.h and BUILD-INFO.txt.

Example PowerShell:

    .\packaging\Build-CombinedRelease.ps1 -Rebuild -PreviousStageDirectory 'D:\PreviousRelease' -PreviousVersion '0.9.6'

    .\packaging\Publish-Release.ps1 -Version '0.9.7'

Test on a COPY of the installed game:

    .\packaging\Install-Patch.ps1 -InstallRoot 'D:\TestInstall' -CurrentVersion '0.9.6' -TargetVersion '0.9.7' -LocalPatchDirectory 'D:\PatchOutput' -NoRestart -NoPause

## Safety and remaining limitations

The updater can only change explicit, project-owned files. Imported retail data,
saves, DLC and configuration are excluded. All paths are validated; symlinks,
unsafe paths, unexpected ZIP files and changed local files are rejected.
Downloads use GitHub HTTPS release assets and SHA-256 validation; files are
staged on the same volume and backed up before replacement. Installation errors
trigger rollback.

Updates require a continuous chain of valid release manifests. A release with
no patch assets is full-download only. The updater script is a managed patch
file, so fixes to it are included in the next delta as well as the full build.

File replacement is per-file rather than a single atomic directory rename.
Power loss during application is NOT yet automatically recoverable, because
there is no durable transaction journal. Update discovery is synchronous with
network timeouts, so a production version should move it off the UI thread.

The Windows CI test in .github/workflows/patcher-smoke.yml exercises a synthetic
delta, preserved user files, and rejection of an altered installed binary.
Still required before merge: native Windows x64 build and UI test, real
GitHub Release asset test, multi-release chain test, interrupted update
recovery test, and antivirus/Windows permission smoke test.
