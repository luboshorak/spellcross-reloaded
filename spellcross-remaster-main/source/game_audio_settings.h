#pragma once

#include "strategic_ui_theme.h"
#include "spellcross.h"
#include "spell_ui_locale.h"
#include <algorithm>
#include <cmath>

// One shared, themed audio settings dialog for the tactical and main-menu
// windows. The original volume preview and Cancel rollback are preserved.
namespace spellcross_audio_ui
{
    inline void ShowDialog(wxWindow* parent, SpellData* data)
    {
        if (!data || !data->sounds || !data->sounds->channels || !data->midi)
            return;

        const double originalMusic = data->midi->GetVolume();
        const double originalSfx = data->sounds->channels->GetVolume();
        wxDialog dialog(parent, wxID_ANY,
            spellcross_ui_locale::Tr("dialog.audio.title", "Audio"),
            wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
        auto* layout = new wxBoxSizer(wxVERTICAL);
        auto addLabel = [&](const char* key, const char* fallback) {
            layout->Add(new wxStaticText(&dialog, wxID_ANY,
                spellcross_ui_locale::Tr(key, fallback)), 0,
                wxLEFT | wxRIGHT | wxTOP, dialog.FromDIP(14));
        };

        addLabel("dialog.audio.music", "Music volume");
        auto* music = new spellcross_strategic_ui::Slider(&dialog, wxID_ANY,
            static_cast<int>(std::lround(originalMusic * 100.0)), 0, 100);
        layout->Add(music, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, dialog.FromDIP(14));

        addLabel("dialog.audio.sound", "Sound volume");
        auto* sound = new spellcross_strategic_ui::Slider(&dialog, wxID_ANY,
            static_cast<int>(std::lround(originalSfx * 100.0)), 0, 100);
        layout->Add(sound, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, dialog.FromDIP(14));

        auto* buttons = new wxBoxSizer(wxHORIZONTAL);
        buttons->AddStretchSpacer();
        auto* ok = new spellcross_strategic_ui::Button(&dialog, wxID_OK,
            spellcross_ui_locale::Tr("dialog.ok", "OK"));
        auto* cancel = new spellcross_strategic_ui::Button(&dialog, wxID_CANCEL,
            spellcross_ui_locale::Tr("dialog.cancel", "Cancel"));
        buttons->Add(ok, 0, wxRIGHT, dialog.FromDIP(8));
        buttons->Add(cancel, 0);
        layout->Add(buttons, 0, wxEXPAND | wxALL, dialog.FromDIP(14));

        dialog.SetSizerAndFit(layout);
        dialog.SetMinSize(wxSize(dialog.FromDIP(410), dialog.FromDIP(230)));
        spellcross_strategic_ui::Apply(&dialog);
        dialog.SetEscapeId(wxID_CANCEL);
        ok->SetDefault();
        dialog.CentreOnParent();

        const auto preview = [&]() {
            data->midi->SetVolume(music->GetValue() / 100.0);
            data->sounds->channels->SetVolume(sound->GetValue() / 100.0);
        };
        music->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { preview(); });
        sound->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { preview(); });

        if (dialog.ShowModal() == wxID_OK)
            preview();
        else
        {
            data->midi->SetVolume(originalMusic);
            data->sounds->channels->SetVolume(originalSfx);
        }
    }
}
