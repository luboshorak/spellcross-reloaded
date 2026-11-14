"""Themed strategic message dialogs preserve the original gameplay decisions."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
STRATEGIC = (ROOT / "source/forms/form_level.cpp").read_text(encoding="utf-8")
DIALOGS = (ROOT / "source/game_dialogs.h").read_text(encoding="utf-8")


class StrategicDialogThemeRegression(unittest.TestCase):
    def test_all_strategic_message_boxes_are_themed(self):
        self.assertIn('#include "../game_dialogs.h"', STRATEGIC)
        self.assertNotIn("wxMessageBox(", STRATEGIC)
        self.assertIn("spellcross_dialog_ui::MessageBox(", STRATEGIC)
        self.assertIn('spellcross_dialog_ui::MessageBox(text, "Event"', STRATEGIC)

    def test_confirmations_keep_yes_no_and_safe_defaults(self):
        self.assertIn("const bool yesNo = (style & wxYES_NO) == wxYES_NO", DIALOGS)
        self.assertIn("(style & wxNO_DEFAULT) != 0", DIALOGS)
        self.assertIn("result == wxID_YES ? wxYES : wxNO", DIALOGS)
        self.assertIn("return yesNo ?", DIALOGS)
        self.assertIn("dialog.SetEscapeId(wxID_OK)", DIALOGS)
        self.assertIn("dialog->SetEscapeId(negativeId)", DIALOGS)
        self.assertIn("wxYES_NO | wxNO_DEFAULT | wxICON_WARNING", STRATEGIC)

    def test_theme_is_shared_with_about_and_console(self):
        for expected in (
            "spellcross_strategic_ui::Button",
            "spellcross_strategic_ui::Apply(&dialog)",
            'spellcross_ui_locale::Tr("dialog.ok"',
            'spellcross_ui_locale::Tr("dialog.yes"',
            'spellcross_ui_locale::Tr("dialog.no"',
        ):
            self.assertIn(expected, DIALOGS)

    def test_yes_no_buttons_are_translated(self):
        cs = (ROOT / "locales/cs.lang").read_text(encoding="utf-8")
        en = (ROOT / "locales/en.lang").read_text(encoding="utf-8")
        self.assertIn("dialog.yes=Ano", cs)
        self.assertIn("dialog.no=Ne", cs)
        self.assertIn("dialog.yes=Yes", en)
        self.assertIn("dialog.no=No", en)

    def test_original_campaign_event_queue_unmodified(self):
        show = STRATEGIC.split(
            "void StrategicLevelFrame::ShowPendingStrategicEventMessages()", 1
        )[1].split("void StrategicLevelFrame::RunDeferredChapterIntroEvents()", 1)[0]
        self.assertIn("m_pendingStrategicEventTexts.erase(", show)
        self.assertIn("m_strategicEventMessageActive = true;", show)
        self.assertIn("m_strategicEventMessageActive = false;", show)
        self.assertIn("spellcross_dialog_ui::MessageBox(text,", show)


if __name__ == "__main__":
    unittest.main()
