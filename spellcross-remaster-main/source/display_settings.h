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
#include <wx/checklst.h>
#include <wx/filedlg.h>
#include <wx/ffile.h>
#include "spell_ui_locale.h"
#include "strategic_ui_theme.h"
#include "tactical_filters.h"
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
        bool advanced_rendering = false; // V2 tactical RGB lighting
        bool procedural_materials = false; // V3 independent material detail
        bool procedural_upscaling = false; // edge-aware tactical sprite reconstruction
        int tactical_filter = spellcross_filters::Legacy;
        bool tactical_dedither = false;
        int tactical_sharpen = 0;
    };

    inline Settings Normalize(Settings s)
    {
        s.scale_percent = (s.scale_percent == 100) ? 100 : 125;
        // Legacy native 100% display scaling is never filtered; the optional
        // tactical filter is independent of DPI and is applied only in battle.
        if (s.scale_percent == 100)
            s.smooth_scaling = false;
        s.brightness = std::clamp(s.brightness, 50, 150);
        s.gamma = std::isfinite(s.gamma) ? std::clamp(s.gamma, 0.5, 2.0) : 1.3;
        s.temperature = std::clamp(s.temperature, 3500, 9500);
        if(!spellcross_filters::Available(s.tactical_filter))
            s.tactical_filter=spellcross_filters::Legacy;
        s.tactical_sharpen=std::clamp(s.tactical_sharpen,0,100);
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
            old.gamma == s.gamma && old.temperature == s.temperature &&
            old.advanced_rendering == s.advanced_rendering &&
            old.procedural_materials == s.procedural_materials &&
            old.procedural_upscaling == s.procedural_upscaling &&
            old.tactical_filter == s.tactical_filter && old.tactical_dedither == s.tactical_dedither &&
            old.tactical_sharpen == s.tactical_sharpen)
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

    // Temporary review tool, deliberately separate from gameplay settings.
    inline void ReviewFilters(wxWindow* parent)
    {
        constexpr int count=spellcross_filters::LastMode+5;
        static std::array<bool,count> rejected{};
        wxDialog dialog(parent,wxID_ANY,spellcross_ui_locale::Tr("screen.review.title","Filter review (debug)"),
            wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
        auto* root=new wxBoxSizer(wxVERTICAL);
        root->Add(new wxStaticText(&dialog,wxID_ANY,spellcross_ui_locale::Tr("screen.review.hint",
            "Check only effects to remove. Export does not change rendering.")),0,wxALL,10);
        auto* list=new wxCheckListBox(&dialog,wxID_ANY);
        wxArrayString keys;
        for(int mode=1;mode<=spellcross_filters::LastMode;++mode)
        {
            if(!spellcross_filters::Available(mode)) continue;
            list->Append(wxString::FromUTF8(spellcross_filters::ModeNames[mode]));
            keys.Add(wxString::Format("tactical_filter:%d",mode));
        }
        const char* extraKeys[]={"advanced_rendering","procedural_materials","procedural_upscaling","tactical_dedither","tactical_sharpen"};
        const wxString extraLabels[]={
            spellcross_ui_locale::Tr("screen.advanced_rendering","Advanced rendering"),
            spellcross_ui_locale::Tr("screen.procedural_materials","Procedural materials"),
            spellcross_ui_locale::Tr("screen.procedural_upscaling","Procedural upscaling"),
            spellcross_ui_locale::Tr("screen.tactical_dedither","Dedither"),
            spellcross_ui_locale::Tr("screen.tactical_sharpen","Sharpen")};
        for(int i=0;i<5;++i) { list->Append(extraLabels[i]); keys.Add(wxString::FromUTF8(extraKeys[i])); }
        for(unsigned i=0;i<list->GetCount();++i) list->Check(i,rejected[i]);
        list->SetMinSize(dialog.FromDIP(wxSize(390,330)));
        root->Add(list,1,wxALL|wxEXPAND,10);
        auto* buttons=new wxBoxSizer(wxHORIZONTAL);
        auto* exportButton=new wxButton(&dialog,wxID_ANY,spellcross_ui_locale::Tr("screen.review.export","Export rejected filters"));
        auto* close=new wxButton(&dialog,wxID_OK,spellcross_ui_locale::Tr("dialog.ok","OK"));
        buttons->Add(exportButton,0,wxRIGHT,8); buttons->Add(close,0);
        root->Add(buttons,0,wxALL|wxALIGN_RIGHT,10);
        const auto remember=[&]() { for(unsigned i=0;i<list->GetCount();++i) rejected[i]=list->IsChecked(i); };
        exportButton->Bind(wxEVT_BUTTON,[&](wxCommandEvent&) {
            remember();
            wxFileDialog save(&dialog,spellcross_ui_locale::Tr("screen.review.export","Export rejected filters"),
                wxEmptyString,"spellcross-filter-review.txt","Text (*.txt)|*.txt",
                wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
            if(save.ShowModal()!=wxID_OK) return;
            wxString report="Spellcross filter review v1\n# checked entries to remove\n";
            for(unsigned i=0;i<list->GetCount();++i)
                if(rejected[i]) report+=keys[i]+"\t"+list->GetString(i)+"\n";
            wxFFile file(save.GetPath(),"wb");
            if(!file.IsOpened()||!file.Write(report,wxConvUTF8)||!file.Close())
                wxMessageBox("Export failed.", "Spellcross",wxOK|wxICON_ERROR,&dialog);
        });
        dialog.SetSizerAndFit(root);
        dialog.CentreOnParent();
        dialog.ShowModal();
        remember();
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

        auto* enhanced = new wxCheckBox(&dialog, wxID_ANY,
            spellcross_ui_locale::Tr("screen.advanced_rendering", "Advanced rendering (experimental)"));
        enhanced->SetValue(original.advanced_rendering);
        root->Add(enhanced, 0, wxLEFT | wxRIGHT | wxTOP, dialog.FromDIP(14));
        auto* materials = new wxCheckBox(&dialog, wxID_ANY,
            spellcross_ui_locale::Tr("screen.procedural_materials", "Procedural terrain materials (experimental)"));
        materials->SetValue(original.procedural_materials);
        root->Add(materials, 0, wxLEFT | wxRIGHT | wxTOP, dialog.FromDIP(8));
        auto* upscaling = new wxCheckBox(&dialog, wxID_ANY,
            spellcross_ui_locale::Tr("screen.procedural_upscaling", "Procedural sprite upscaling (experimental)"));
        upscaling->SetValue(original.procedural_upscaling);
        root->Add(upscaling, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, dialog.FromDIP(8));

        addLabel(spellcross_ui_locale::Tr("screen.tactical_filter", "Battlefield scaling filter (CPU)"));
        auto* filter = new wxChoice(&dialog, wxID_ANY);
        filter->Append(spellcross_ui_locale::Tr("screen.filter.legacy", "Existing renderer (default)"));
        std::vector<int> filterIds{spellcross_filters::Legacy};
        int selectedFilter=0;
        for(int mode=1;mode<=spellcross_filters::LastMode;++mode)
        {
            if(!spellcross_filters::Available(mode)) continue;
            if(mode==original.tactical_filter) selectedFilter=static_cast<int>(filterIds.size());
            filterIds.push_back(mode);
            filter->Append(wxString::FromUTF8(spellcross_filters::ModeNames[mode]));
        }
        filter->SetSelection(selectedFilter);
        root->Add(filter, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, dialog.FromDIP(14));
        auto* dedither = new wxCheckBox(&dialog, wxID_ANY,
            spellcross_ui_locale::Tr("screen.tactical_dedither", "Reduce palette dithering"));
        dedither->SetValue(original.tactical_dedither);
        root->Add(dedither, 0, wxLEFT | wxRIGHT | wxTOP, dialog.FromDIP(8));
        addLabel(spellcross_ui_locale::Tr("screen.tactical_sharpen", "Adaptive sharpening (%)"));
        auto* sharpen = new spellcross_strategic_ui::Slider(&dialog,
            wxID_ANY, original.tactical_sharpen, 0, 100);
        root->Add(sharpen, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, dialog.FromDIP(14));

        auto* review=new wxButton(&dialog,wxID_ANY,
            spellcross_ui_locale::Tr("screen.review.title","Filter review (debug)"));
        review->Bind(wxEVT_BUTTON,[&](wxCommandEvent&) { ReviewFilters(&dialog); });
        root->Add(review,0,wxLEFT|wxRIGHT|wxBOTTOM,dialog.FromDIP(14));

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
            next.advanced_rendering = enhanced->GetValue();
            next.procedural_materials = materials->GetValue();
            next.procedural_upscaling = upscaling->GetValue();
            const int selected=filter->GetSelection();
            next.tactical_filter = selected>=0 && selected<static_cast<int>(filterIds.size())?
                filterIds[selected]:spellcross_filters::Legacy;
            next.tactical_dedither = dedither->GetValue();
            next.tactical_sharpen = sharpen->GetValue();
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
        enhanced->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        materials->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        upscaling->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        filter->Bind(wxEVT_CHOICE, [&](wxCommandEvent&) { apply(); });
        dedither->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        sharpen->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });

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
