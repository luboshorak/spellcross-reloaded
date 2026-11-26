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
#include "strategic_ui_theme.h"
#include <array>

namespace spellcross_display
{
    struct Settings
    {
        int scale_percent = 125;   // 100% original / 125% enlarged
        bool smooth_scaling = true;  // bilinear 125% mode is the default
        int brightness = 100;      // linear multiplier, 50..150 percent
        double gamma = 1.3;        // original Spellcross default, 0.5..2.0
        int temperature = 6500;    // neutral daylight, 3500..9500 kelvin
    };

    inline Settings Normalize(Settings s)
    {
        s.scale_percent = (s.scale_percent == 100) ? 100 : 125;
        // Native 100% output must never be filtered.
        if (s.scale_percent == 100)
            s.smooth_scaling = false;
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
        if (old.scale_percent == s.scale_percent &&
            old.smooth_scaling == s.smooth_scaling && old.brightness == s.brightness &&
            old.gamma == s.gamma && old.temperature == s.temperature)
            return;
        old = s;
        ++MutableRevision();
    }

    inline bool UseSmoothScaling()
    {
        const auto& s = Current();
        return s.scale_percent == 125 && s.smooth_scaling;
    }

    inline wxImageResizeQuality ResizeQuality()
    {
        return UseSmoothScaling() ? wxIMAGE_QUALITY_BILINEAR : wxIMAGE_QUALITY_NEAREST;
    }

    // Centered source sampling used by the 125% bilinear tactical conversion.
    // Coordinates are clamped to prevent reading outside the logical surface.
    struct LinearSample
    {
        int first;
        int second;
        int weight; // 0..256, fraction of second pixel
    };

    inline LinearSample BilinearSample(int target, int scale, int sourceSize)
    {
        if (sourceSize <= 1)
            return {0, 0, 0};
        const std::int64_t numerator =
            (2LL * target + 1) * 100 - scale;
        const std::int64_t denominator = 2LL * scale;
        if (numerator <= 0)
            return {0, 0, 0};
        const std::int64_t fixed = (numerator * 256 + denominator / 2) / denominator;
        if (fixed >= 256LL * (sourceSize - 1))
            return {sourceSize - 1, sourceSize - 1, 0};
        const int first = static_cast<int>(fixed / 256);
        return {first, first + 1, static_cast<int>(fixed % 256)};
    }

    inline unsigned char BlendBilinear(unsigned char a, unsigned char b,
                                      unsigned char c, unsigned char d,
                                      int fx, int fy)
    {
        const int top = a * (256 - fx) + b * fx;
        const int bottom = c * (256 - fx) + d * fx;
        return static_cast<unsigned char>((top * (256 - fy) +
            bottom * fy + 32768) / 65536);
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

    struct ColourTables
    {
        unsigned char tactical[3][256] = {};
        unsigned char bitmap[3][256] = {};
        std::uint64_t revision = 0;
    };

    inline const ColourTables& Tables()
    {
        static ColourTables tables;
        if (tables.revision != Revision())
        {
            const auto& settings = Current();
            for (int channel = 0; channel < 3; ++channel)
                for (int v = 0; v < 256; ++v)
                {
                    tables.tactical[channel][v] = AdjustChannel(
                        static_cast<unsigned char>(v), channel, settings, true);
                    tables.bitmap[channel][v] = AdjustChannel(
                        static_cast<unsigned char>(v), channel, settings, false);
                }
            tables.revision = Revision();
        }
        return tables;
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
        const auto& lut = Tables().bitmap;
        const std::size_t count = static_cast<std::size_t>(image.GetWidth()) * image.GetHeight();
        for (std::size_t i = 0; i < count; ++i)
            for (int channel = 0; channel < 3; ++channel)
                data[3 * i + channel] = lut[channel][data[3 * i + channel]];
        return wxBitmap(image);
    }

    // All three top-level windows use this very same dialog and settings.
    // Changes preview live; Cancel restores the complete original state.
    inline bool ShowScreenDialog(wxWindow* parent, const std::function<void()>& refresh)
    {
        const Settings original = Current();
        wxDialog dialog(parent, wxID_ANY,
            spellcross_ui_locale::Tr("screen.title", "Screen"),
            wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto addLabel = [&](const wxString& title) {
            root->Add(new wxStaticText(&dialog, wxID_ANY, title),
                0, wxLEFT | wxRIGHT | wxTOP, dialog.FromDIP(14));
        };

        addLabel(spellcross_ui_locale::Tr("screen.dpi", "Game scale (DPI)"));
        const wxString scales[] = {
            spellcross_ui_locale::Tr("screen.scale.original", "100% - original pixels"),
            spellcross_ui_locale::Tr("screen.scale.enlarged", "125% - enlarged / sharp"),
            spellcross_ui_locale::Tr("screen.scale.smooth", "125% - enlarged / smoothed")
        };
        int selectedScale = original.scale_percent == 100 ? 0 :
            (original.smooth_scaling ? 2 : 1);
        std::array<spellcross_strategic_ui::Button*, 3> scaleButtons{};
        auto* scaleRow = new wxBoxSizer(wxHORIZONTAL);
        for (int i = 0; i < 3; ++i)
        {
            scaleButtons[i] = new spellcross_strategic_ui::Button(&dialog,
                wxID_ANY, scales[i]);
            scaleRow->Add(scaleButtons[i], 1, wxRIGHT | wxEXPAND,
                i == 2 ? 0 : dialog.FromDIP(6));
        }
        root->Add(scaleRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, dialog.FromDIP(14));

        addLabel(spellcross_ui_locale::Tr("screen.brightness", "Brightness (%)"));
        auto* brightness = new spellcross_strategic_ui::Slider(&dialog,
            wxID_ANY, original.brightness, 50, 150);
        root->Add(brightness, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, dialog.FromDIP(14));

        addLabel(spellcross_ui_locale::Tr("screen.gamma", "Gamma (x100)"));
        auto* gamma = new spellcross_strategic_ui::Slider(&dialog,
            wxID_ANY, static_cast<int>(std::lround(original.gamma * 100)), 50, 200);
        root->Add(gamma, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, dialog.FromDIP(14));

        addLabel(spellcross_ui_locale::Tr("screen.temperature", "Color temperature (K)"));
        auto* temperature = new spellcross_strategic_ui::Slider(&dialog,
            wxID_ANY, original.temperature, 3500, 9500);
        root->Add(temperature, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
            dialog.FromDIP(14));

        auto* buttons = new wxBoxSizer(wxHORIZONTAL);
        buttons->AddStretchSpacer();
        auto* ok = new spellcross_strategic_ui::Button(&dialog, wxID_OK,
            spellcross_ui_locale::Tr("dialog.ok", "OK"));
        auto* cancel = new spellcross_strategic_ui::Button(&dialog, wxID_CANCEL,
            spellcross_ui_locale::Tr("dialog.cancel", "Cancel"));
        buttons->Add(ok, 0, wxRIGHT, dialog.FromDIP(8));
        buttons->Add(cancel, 0);
        root->Add(buttons, 0, wxEXPAND | wxALL, dialog.FromDIP(14));
        dialog.SetSizerAndFit(root);
        spellcross_strategic_ui::Apply(&dialog);
        dialog.SetEscapeId(wxID_CANCEL);
        ok->SetDefault();
        dialog.CentreOnParent();

        const auto updateSelection = [&]() {
            for (int i = 0; i < 3; ++i)
            {
                wxFont font = scaleButtons[i]->GetFont();
                font.SetWeight(i == selectedScale ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
                scaleButtons[i]->SetFont(font);
                scaleButtons[i]->Refresh(false);
            }
        };
        const auto apply = [&]() {
            Settings next;
            next.scale_percent = selectedScale == 0 ? 100 : 125;
            next.smooth_scaling = selectedScale == 2;
            next.brightness = brightness->GetValue();
            next.gamma = gamma->GetValue() / 100.0;
            next.temperature = temperature->GetValue();
            Set(next);
            if (refresh) refresh();
        };
        for (int i = 0; i < 3; ++i)
        {
            scaleButtons[i]->Bind(wxEVT_BUTTON, [&, i](wxCommandEvent&) {
                selectedScale = i;
                updateSelection();
                apply();
            });
        }
        updateSelection();
        brightness->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        gamma->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        temperature->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });

        if (dialog.ShowModal() == wxID_OK)
        {
            apply();
            return true;
        }
        Set(original);
        if (refresh) refresh();
        return false;
    }

}
