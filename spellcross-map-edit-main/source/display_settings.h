#pragma once
// Runtime display preferences.  This is game-internal pixel scaling, NOT a
// change to Windows DPI awareness: the PerMonitorV2 executable manifest stays
// enabled so Windows cannot bilinearly stretch the finished framebuffer.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>

#include <wx/wx.h>
#include <wx/choice.h>
#include <wx/slider.h>
#include "spell_ui_locale.h"

namespace spellcross_display
{
    struct Settings
    {
        int scale_percent = 125;   // 100% original / 125% enlarged, nearest pixel
        int brightness = 100;      // linear multiplier, 50..150 percent
        double gamma = 1.3;        // original Spellcross default, 0.5..2.0
        int temperature = 6500;    // neutral daylight, 3500..9500 kelvin
    };

    inline Settings Normalize(Settings s)
    {
        s.scale_percent = (s.scale_percent == 100) ? 100 : 125;
        s.brightness = std::clamp(s.brightness, 50, 150);
        s.gamma = std::isfinite(s.gamma) ? std::clamp(s.gamma, 0.5, 2.0) : 1.3;
        s.temperature = std::clamp(s.temperature, 3500, 9500);
        return s;
    }

    inline Settings& MutableSettings()
    {
        static Settings s;
        return s;
    }

    inline const Settings& Current() { return MutableSettings(); }

    inline std::uint64_t& MutableRevision()
    {
        static std::uint64_t rev = 1;
        return rev;
    }

    inline std::uint64_t Revision() { return MutableRevision(); }

    // Single-threaded wx main-loop controls these settings.
    inline void Set(Settings s)
    {
        s = Normalize(s);
        auto& old = MutableSettings();
        if (old.scale_percent == s.scale_percent && old.brightness == s.brightness &&
            old.gamma == s.gamma && old.temperature == s.temperature)
            return;
        old = s;
        ++MutableRevision();
    }

    inline void SetGamma(double gamma)
    {
        Settings s = Current();
        s.gamma = gamma;
        Set(s);
    }

    inline int ToLogical(int devicePixel)
    {
        const int scale = Current().scale_percent;
        return static_cast<int>(static_cast<std::int64_t>(devicePixel) * 100 / scale);
    }

    inline int ToDevice(int logicalPixel)
    {
        const int scale = Current().scale_percent;
        return static_cast<int>((static_cast<std::int64_t>(logicalPixel) * scale + 99) / 100);
    }

    inline int LogicalExtent(int deviceExtent)
    {
        const int scale = Current().scale_percent;
        return static_cast<int>((static_cast<std::int64_t>(deviceExtent) * 100 + scale - 1) / scale);
    }

    inline double KelvinChannel(int kelvin, int channel)
    {
        const double t = std::clamp(kelvin / 100.0, 10.0, 400.0);
        double v;
        if (channel == 0)
            v = t <= 66.0 ? 255.0 : 329.698727446 * std::pow(t - 60.0, -0.1332047592);
        else if (channel == 1)
            v = t <= 66.0 ? 99.4708025861 * std::log(t) - 161.1195681661
                          : 288.1221695283 * std::pow(t - 60.0, -0.0755148492);
        else
            v = t >= 66.0 ? 255.0 :
                t <= 19.0 ? 0.0 : 138.5177312231 * std::log(t - 10.0) - 305.0447927307;
        return std::clamp(v, 0.0, 255.0);
    }

    inline unsigned char AdjustChannel(unsigned char input, int channel,
                                       const Settings& settings, bool legacyPalette = true)
    {
        // The tactical palette has always had gamma 1.3.  Existing menu/strategic
        // images already have their final RGB values, so treat 1.3 as their
        // neutral gamma rather than applying the legacy correction twice.
        const double effectiveGamma = legacyPalette ? settings.gamma : settings.gamma / 1.3;
        const double neutral = KelvinChannel(6500, channel);
        const double warmCool = KelvinChannel(settings.temperature, channel) /
                                std::max(1.0, neutral);
        const double corrected = 255.0 * std::pow(input / 255.0, 1.0 / effectiveGamma) *
                                 (settings.brightness / 100.0) * warmCool;
        return static_cast<unsigned char>(std::lround(std::clamp(corrected, 0.0, 255.0)));
    }

    inline wxBitmap AdjustBitmap(const wxBitmap& bitmap)
    {
        if (!bitmap.IsOk())
            return bitmap;
        const Settings& settings = Current();
        if (settings.brightness == 100 && settings.temperature == 6500 &&
            std::abs(settings.gamma - 1.3) < 0.0001)
            return bitmap;
        wxImage image = bitmap.ConvertToImage();
        unsigned char* data = image.GetData();
        if (!data)
            return bitmap;
        const std::size_t count = static_cast<std::size_t>(image.GetWidth()) * image.GetHeight();
        for (std::size_t i = 0; i < count; ++i)
            for (int channel = 0; channel < 3; ++channel)
                data[3 * i + channel] = AdjustChannel(data[3 * i + channel], channel, settings, false);
        return wxBitmap(image);
    }

    // All three top-level windows use this very same dialog and settings.
    // Changes preview live; Cancel restores the complete original state.
    inline bool ShowScreenDialog(wxWindow* parent, const std::function<void()>& refresh)
    {
        const Settings original = Current();
        wxDialog dlg(parent, wxID_ANY,
            spellcross_ui_locale::Tr("screen.title", "Screen"),
            wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto addLabel = [&](const wxString& title) {
            root->Add(new wxStaticText(&dlg, wxID_ANY, title), 0, wxLEFT | wxRIGHT | wxTOP, 10);
        };

        addLabel(spellcross_ui_locale::Tr("screen.dpi", "Game scale (DPI)"));
        auto* scale = new wxChoice(&dlg, wxID_ANY);
        scale->Append(spellcross_ui_locale::Tr("screen.scale.original", "100% - original pixels"));
        scale->Append(spellcross_ui_locale::Tr("screen.scale.enlarged", "125% - enlarged / sharp"));
        scale->SetSelection(original.scale_percent == 100 ? 0 : 1);
        root->Add(scale, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

        addLabel(spellcross_ui_locale::Tr("screen.brightness", "Brightness (%)"));
        auto* brightness = new wxSlider(&dlg, wxID_ANY, original.brightness, 50, 150,
            wxDefaultPosition, wxSize(340, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
        root->Add(brightness, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

        addLabel(spellcross_ui_locale::Tr("screen.gamma", "Gamma (x100)"));
        auto* gamma = new wxSlider(&dlg, wxID_ANY,
            static_cast<int>(std::lround(original.gamma * 100)), 50, 200,
            wxDefaultPosition, wxSize(340, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
        root->Add(gamma, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

        addLabel(spellcross_ui_locale::Tr("screen.temperature", "Color temperature (K)"));
        auto* temp = new wxSlider(&dlg, wxID_ANY, original.temperature, 3500, 9500,
            wxDefaultPosition, wxSize(340, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
        root->Add(temp, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

        root->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), 0, wxALL | wxEXPAND, 10);
        dlg.SetSizerAndFit(root);

        const auto apply = [&]() {
            Settings next;
            next.scale_percent = scale->GetSelection() == 0 ? 100 : 125;
            next.brightness = brightness->GetValue();
            next.gamma = gamma->GetValue() / 100.0;
            next.temperature = temp->GetValue();
            Set(next);
            if (refresh) refresh();
        };
        scale->Bind(wxEVT_CHOICE, [&](wxCommandEvent&) { apply(); });
        brightness->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        gamma->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        temp->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });

        if (dlg.ShowModal() == wxID_OK)
        {
            apply();
            return true;
        }
        Set(original);
        if (refresh) refresh();
        return false;
    }
}
