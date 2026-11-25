/*
 * The contents of this file are subject to the Common Public Attribution License Version 1.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at https://opensource.org/license/cpal-1-0. The License is based on the Mozilla Public License Version 1.1 but Sections 14 and 15 have been added to cover use of software over a computer network and provide for limited attribution for the Original Developer. In addition, Exhibit A has been modified to be consistent with Exhibit B.
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for the specific language governing rights and limitations under the License.
 * The Original Code is Spellcross Reloaded restoration code covered by CPAL-1.0.
 * The Initial Developer and Original Developer of the Original Code is Luboš Horák. All portions of the code written by Luboš Horák and covered by this License are Copyright (c) 2026 Luboš Horák. All Rights Reserved.
 * Contributor: see AUTHORS.md and the Git history.
 * SPDX-License-Identifier: CPAL-1.0
 * Previously granted MIT rights are not withdrawn; see LICENSING.md.
 */
#pragma once
#include <array>

namespace spellcross_menu_video
{
    // Names referenced by the original Czech and English executables.
    // Other retail editions use the SMK-first resolver, with legacy aliases.
    constexpr const char* Intro(bool czech)
    {
        return czech ? "INTRO.CAN" : "INTRO.SMK";
    }

    constexpr const char* Credits(bool czech)
    {
        return czech ? "CREDITS.DPK" : "CREDITS.SMK";
    }

    // Retail CZ 0x110CA..0x11113; EN 0x11385..0x113CE.
    // Both TITAN films are the victory ending, not alternative endings.
    constexpr std::array<const char*, 5> Victory(bool czech)
    {
        return {czech ? "TITAN1.CAN" : "TITAN1.SMK",
                czech ? "TITAN2.CAN" : "TITAN2.SMK",
                "S_10.DP2",
                czech ? "LOGO.CAN" : "LOGO.SMK",
                Credits(czech)};
    }

    constexpr const char* Defeat()
    {
        return "S_OVER.DP2";
    }
}
