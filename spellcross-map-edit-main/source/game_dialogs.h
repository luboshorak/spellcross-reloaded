#pragma once

#include "strategic_ui_theme.h"
#include "spell_ui_locale.h"
#include <wx/textctrl.h>
#include <wx/display.h>
#include <algorithm>

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

    // Styled replacement for the strategic game's standard wxMessageBox calls.
    // Preserve wxMessageBox's wxYES / wxNO / wxOK result contract so changing
    // the presentation cannot alter counter-attacks or end-turn decisions.
    // Keep the usual one-line confirmations compact. Story / event text can
    // span several pages; it must never push the buttons or the dialog itself
    // outside the available strategic-map viewport (issue #178, level 09).
    inline int ShowThemedMessage(const wxString& message, const wxString& title,
        long style, wxWindow* parent)
    {
        wxDialog dialog(parent, wxID_ANY, title, wxDefaultPosition,
            wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
        const int padding = dialog.FromDIP(16);

        // Respect both the current monitor (including taskbar) and the owning
        // strategic window. Centering a huge fitted wxStaticText used to put
        // the beginning/end of chapter narratives outside the screen.
        wxDisplay display(parent);
        const wxRect screen = display.IsOk()
            ? display.GetClientArea() : wxGetClientDisplayRect();
        const wxSize owner = parent ? parent->GetClientSize() : screen.GetSize();
        const int availableWidth = (std::min)(screen.width,
            owner.x > 0 ? owner.x : screen.width);
        const int availableHeight = (std::min)(screen.height,
            owner.y > 0 ? owner.y : screen.height);
        const int maxDialogWidth = (std::max)(dialog.FromDIP(260),
            (std::min)(dialog.FromDIP(760), availableWidth - dialog.FromDIP(48)));
        const int maxDialogHeight = (std::max)(dialog.FromDIP(190),
            (std::min)(dialog.FromDIP(540), availableHeight - dialog.FromDIP(96)));

        auto* layout = new wxBoxSizer(wxVERTICAL);
        const bool longMessage = message.length() > 280 ||
            std::count(message.begin(), message.end(), wxChar('\n')) >= 5;
        wxTextCtrl* scrollText = nullptr;
        if (longMessage)
        {
            // Native multiline control supplies word wrapping, wheel/keyboard
            // scrolling and text selection. Unlike wxStaticText it has a
            // bounded height, so no authored chapter text is discarded.
            scrollText = new wxTextCtrl(&dialog, wxID_ANY, message,
                wxDefaultPosition,
                wxSize(maxDialogWidth - 3 * padding, dialog.FromDIP(230)),
                wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP |
                wxVSCROLL | wxBORDER_SIMPLE);
            layout->Add(scrollText, 1, wxEXPAND | wxALL, padding);
        }
        else
        {
            auto* label = new wxStaticText(&dialog, wxID_ANY, message);
            label->Wrap((std::min)(dialog.FromDIP(540), maxDialogWidth - 3 * padding));
            layout->Add(label, 0, wxEXPAND | wxALL, padding);
        }

        const bool yesNo = (style & wxYES_NO) == wxYES_NO;
        if (yesNo)
        {
            layout->Add(ActionRow(&dialog, wxID_YES,
                spellcross_ui_locale::Tr("dialog.yes", "Yes"), wxID_NO,
                spellcross_ui_locale::Tr("dialog.no", "No"),
                (style & wxNO_DEFAULT) != 0),
                0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, padding);
        }
        else
        {
            auto* row = new wxBoxSizer(wxHORIZONTAL);
            row->AddStretchSpacer();
            auto* ok = new spellcross_strategic_ui::Button(&dialog, wxID_OK,
                spellcross_ui_locale::Tr("dialog.ok", "OK"));
            row->Add(ok);
            layout->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                padding);
            ok->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) {
                dialog.EndModal(wxID_OK);
            });
            ok->SetDefault();
            dialog.SetEscapeId(wxID_OK);
        }

        dialog.SetSizerAndFit(layout);
        spellcross_strategic_ui::Apply(&dialog);
        if (scrollText)
        {
            // Apply() fits dialogs, so bound the final size AFTER styling.
            // The weighted text area shrinks/expands while the action row
            // stays visible at the bottom, including when resized by hand.
            dialog.SetSize(wxSize(maxDialogWidth, maxDialogHeight));
            dialog.Layout();
            scrollText->SetInsertionPoint(0);
        }
        else
        {
            dialog.SetMinSize(wxSize((std::min)(maxDialogWidth, dialog.FromDIP(420)),
                (std::min)(maxDialogHeight, dialog.FromDIP(150))));
            dialog.GetSizer()->Fit(&dialog);
        }
        dialog.CentreOnParent();
        const int result = dialog.ShowModal();
        return yesNo ? (result == wxID_YES ? wxYES : wxNO) : wxOK;
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
