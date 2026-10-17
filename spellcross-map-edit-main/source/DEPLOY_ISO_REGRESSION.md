# ISO download and tactical deployment — regression checks

Test against branch `fix/strategic-loader-mechanics-tick` after a **Windows Rebuild**.

## Source wizard: CZ and EN ISO

1. With required Spellcross data missing, choose **Download CZ ISO**. If HTTP succeeds, verify progress in MiB, complete ISO9660 validation, extraction of `INSTALL.DTA`, optional CD sidecars, and saved source paths. Repeat with **Download EN ISO**.
2. When the host denies a WinINet request (Cloudflare HTTP 403/503, TLS, DNS), the error must include **HTTP status or WinINet error**, not merely say "no connection".
3. Choose **Open browser** in that warning; let the browser finish downloading, select the resulting ISO in the file picker, and verify that it passes through the **same** import path as the direct download.
4. Choosing **Back to setup**, canceling the ISO picker, or canceling download progress must not replace `config.ini` source paths or destroy a previously cached ISO.
5. Verify both cached-ISO reuse and a corrupt/HTML/truncated response: only valid ISO9660 data may be handed to the importer. No `.part` should survive a failed transfer.
6. The ordinary manually selected ISO, mounted CD and existing DATA-folder options must remain unchanged.

**Note:** Whether Cloudflare accepts in-app direct HTTP requests cannot be proven by static review; test from Windows on the real network. The browser fallback handles authentication/challenge cases without attempting to bypass them.

## Tactical deployment

1. Start a strategic mission with several START crosses spread across a map (include maps with separated groups). The **first tactical view** should show the largest nearby cluster, not the old origin or the first arbitrary DEF START entry.
2. Check a small and a large tactical window and a second mission loaded after the first. Camera sizing must use the actual current canvas dimensions.
3. Before deploying, **all non-START terrain remains masked**. Camera-centering must not reveal enemies, alter map geometry or unit placement, or bypass placement restrictions.
4. During blinking, click **UKONČEN** both while the icon is visible and while hidden. It must respond in both phases. When not all companies are placed, the normal "deployment incomplete" warning must still appear.
5. Place all companies and finish deployment. Confirm that the normal tactical HUD replaces the placement controls, initial unit view and mission events begin, and the camera is **not** forcibly re-centered if the player manually scrolls later.
6. Continue a mission after returning from the main menu: the deployment camera should not reposition once initial setup is complete.

## Verification status

Static branch checks passed; MSVC compile and runtime validation are outstanding.
