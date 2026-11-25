# The contents of this file are subject to the Common Public Attribution License Version 1.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at https://opensource.org/license/cpal-1-0. The License is based on the Mozilla Public License Version 1.1 but Sections 14 and 15 have been added to cover use of software over a computer network and provide for limited attribution for the Original Developer. In addition, Exhibit A has been modified to be consistent with Exhibit B.
# Software distributed under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for the specific language governing rights and limitations under the License.
# The Original Code is Spellcross Reloaded restoration code covered by CPAL-1.0.
# The Initial Developer and Original Developer of the Original Code is Luboš Horák. All portions of the code written by Luboš Horák and covered by this License are Copyright (c) 2026 Luboš Horák. All Rights Reserved.
# Contributor: see AUTHORS.md and the Git history.
# SPDX-License-Identifier: CPAL-1.0
# Previously granted MIT rights are not withdrawn; see LICENSING.md.

"""Attribution and redistribution safeguards."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "spellcross-map-edit-main/source"
LICENSE = (ROOT / "LICENSE").read_text(encoding="utf-8")
TEMPLATE = (ROOT / "LICENSES/CPAL-1.0.txt").read_text(encoding="utf-8")
ATTRIBUTION = (SOURCE / "project_attribution.h").read_text(encoding="utf-8")
ABOUT = (SOURCE / "forms/form_about.cpp").read_text(encoding="utf-8")

class AttributionTest(unittest.TestCase):
    def test_cpal_body_is_unmodified_and_exhibit_b_populated(self):
        self.assertEqual(LICENSE.split("EXHIBIT A.", 1)[0], TEMPLATE.split("EXHIBIT A.", 1)[0])
        phrase = re.search(r"Attribution Phrase \(not exceeding 10 words\): (.+)", LICENSE).group(1)
        self.assertLessEqual(len(phrase.split()), 10)
        for value in ("Luboš Horák", "Stanislav Mašláň"):
            self.assertIn(value, phrase)
        self.assertIn(phrase, ATTRIBUTION)
        self.assertIn("Attribution Copyright Notice: Copyright (c) 2026 Luboš Horák", LICENSE)
        self.assertIn("Display of Attribution Information is required in Larger Works", LICENSE)
        self.assertIn("https://github.com/luboshorak/spellcross-reloaded", LICENSE)

    def test_about_uses_attribution_and_library_notices(self):
        for name in ("Copyright", "Phrase", "Url", "License"):
            self.assertIn("spellcross_attribution::" + name, ABOUT)
        self.assertIn("libsmacker", ABOUT)
        self.assertIn("XMIDI", ABOUT)
        self.assertIn("SPDX-License-Identifier: CPAL-1.0", ATTRIBUTION)

    def test_legacy_mit_and_lgpl_notices_are_preserved(self):
        self.assertEqual((ROOT / "LICENSES/MIT-Maslan.txt").read_bytes(),
                         (ROOT / "spellcross-map-edit-main/LICENSE").read_bytes())
        self.assertIn("MIT License", (ROOT / "LICENSES/MIT-legacy.txt").read_text())
        self.assertIn("GNU LIBRARY GENERAL PUBLIC LICENSE", (ROOT / "LICENSES/LGPL-2.0.txt").read_text())
        for script in ("Build-CombinedRelease.ps1", "Build-UniversalNoRelease.ps1"):
            self.assertIn("Add-DistributionLicenses.ps1", (ROOT / "packaging" / script).read_text(encoding="utf-8-sig"))
        helper = (ROOT / "packaging/Add-DistributionLicenses.ps1").read_text()
        for value in ("SOURCE-CODE.zip", "XMIDI-LGPL-2.0.txt", "libsmacker-LGPL-2.1.txt",
                      "NOTICE.md", "LICENSES", "spellcross_ui_coordinate_catalog"):
            self.assertIn(value, helper)

if __name__ == "__main__":
    unittest.main()
