#pragma once

#include "strategic_ui_theme.h"
#include "spell_ui_locale.h"
#include <wx/textctrl.h>

// Small native wx modal helpers shared by the tactical and main-menu layers.
// File pickers remain OS-native, but game-owned confirmations and the console
// use the same dark-green/bright-green design as the strategic pages.
namespace spellcross_dialog_ui
{
    inline wxBoxSizer* ActionRow(wxDialog* dialog,
        int positiveId, const wxString& positive,
        int negativeId, const wxString& negative,
        bool defaultNegative = false)
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->AddStretchSpacer();
        auto* yes = new spellcross_strategic_ui::Button(dialog, positiveId, positive);
        auto* no = new spellcross_strategic_ui::Button(dialog, negativeId, negative);
        row->Add(yes, 0, wxRIGHT, dialog->FromDIP(8));
        row->Add(no, 0);
        auto finish = [dialog](int id) { dialog->EndModal(id); };
        yes->Bind(wxEVT_BUTTON, [finish, positiveId](wxCommandEvent&) { finish(positiveId); });
        no->Bind(wxEVT_BUTTON, [finish, negativeId](wxCommandEvent&) { finish(negativeId); });
        (defaultNegative ? no : yes)->SetDefault();
        dialog->SetEscapeId(negativeId);
        return row;
    }

    inline int Confirm(wxWindow* parent, const wxString& title,
        const wxString& message, const wxString& yes, const wxString& no)
    {
        wxDialog dialog(parent, wxID_ANY, title, wxDefaultPosition,
            wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
        auto* layout = new wxBoxSizer(wxVERTICAL);
        auto* label = new wxStaticText(&dialog, wxID_ANY, message);
        label->Wrap(dialog.FromDIP(540));
        layout->Add(label, 0, wxEXPAND | wxALL, dialog.FromDIP(16));
        layout->Add(ActionRow(&dialog, wxID_YES, yes, wxID_NO, no, true),
            0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, dialog.FromDIP(16));
        dialog.SetSizerAndFit(layout);
        spellcross_strategic_ui::Apply(&dialog);
        dialog.SetMinSize(wxSize(dialog.FromDIP(450), dialog.FromDIP(170)));
        dialog.CentreOnParent();
        return dialog.ShowModal();
    }

    inline bool PromptText(wxWindow* parent, const wxString& title,
        const wxString& question, wxString& value)
    {
        wxDialog dialog(parent, wxID_ANY, title, wxDefaultPosition,
            wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
        auto* layout = new wxBoxSizer(wxVERTICAL);
        auto* label = new wxStaticText(&dialog, wxID_ANY, question);
        layout->Add(label, 0, wxALL, dialog.FromDIP(14));
        auto* input = new wxTextCtrl(&dialog, wxID_ANY, value,
            wxDefaultPosition, wxSize(dialog.FromDIP(410), -1), wxTE_PROCESS_ENTER);
        input->SetBackgroundColour(spellcross_menu::Highlight());
        input->SetForegroundColour(spellcross_menu::Text());
        layout->Add(input, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            dialog.FromDIP(14));
        layout->Add(ActionRow(&dialog, wxID_OK,
            spellcross_ui_locale::Tr("dialog.ok", "OK"), wxID_CANCEL,
            spellcross_ui_locale::Tr("dialog.cancel", "Cancel")),
            0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, dialog.FromDIP(14));
        input->Bind(wxEVT_TEXT_ENTER, [&dialog](wxCommandEvent&) {
            dialog.EndModal(wxID_OK);
        });
        dialog.SetSizerAndFit(layout);
        spellcross_strategic_ui::Apply(&dialog);
        input->SetBackgroundColour(spellcross_menu::Highlight());
        input->SetForegroundColour(spellcross_menu::Text());
        dialog.CentreOnParent();
        input->SetFocus();
        if (dialog.ShowModal() != wxID_OK)
            return false;
        value = input->GetValue();
        return true;
    }
}
