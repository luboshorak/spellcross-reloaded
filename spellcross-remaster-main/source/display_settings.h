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
#include <wx/odcombo.h>
#include <wx/slider.h>
#include <wx/scrolwin.h>
#include <wx/display.h>
#include "spell_ui_locale.h"
#include "strategic_ui_theme.h"
#include "tactical_filters.h"
#include "render_perf.h"
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
        bool gpu_acceleration = false; // opt-in D3D11 compute, CPU fallback
        int tactical_filter = spellcross_filters::Legacy;
        bool tactical_dedither = false;
        int tactical_sharpen = 0;
        int lighting_intensity = 100;
        bool directional_shadows = false;
        bool original_shadows = true;
        int shadow_strength = 60;
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
        s.lighting_intensity=std::clamp(s.lighting_intensity,0,100);
        s.shadow_strength=std::clamp(s.shadow_strength,0,100);
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
            old.gpu_acceleration == s.gpu_acceleration &&
            old.tactical_filter == s.tactical_filter && old.tactical_dedither == s.tactical_dedither &&
            old.tactical_sharpen == s.tactical_sharpen &&
            old.lighting_intensity == s.lighting_intensity &&
            old.directional_shadows == s.directional_shadows && old.original_shadows == s.original_shadows &&
            old.shadow_strength == s.shadow_strength)
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

    // Screen-specific controls keep the existing strategic theme without
    // inheriting native static-box fonts or Windows checkbox/choice colours.
    class ScreenSection final : public wxPanel
    {
        wxBoxSizer* m_body;
    public:
        ScreenSection(wxWindow* parent, const wxString& title)
            : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
        {
            SetBackgroundStyle(wxBG_STYLE_PAINT);
            auto* outer = new wxBoxSizer(wxVERTICAL);
            auto* heading = new wxStaticText(this, wxID_ANY, title);
            heading->SetFont(spellcross_menu::MenuFont().Bold());
            outer->Add(heading, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
            outer->AddSpacer(FromDIP(5));
            m_body = new wxBoxSizer(wxVERTICAL);
            outer->Add(m_body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(5));
            SetSizer(outer);
            Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
                wxAutoBufferedPaintDC dc(this);
                dc.SetBackground(wxBrush(spellcross_menu::Background()));
                dc.Clear();
                dc.SetPen(wxPen(spellcross_strategic_ui::Border()));
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawRectangle(GetClientRect());
            });
        }
        wxWindow* GetStaticBox() { return this; }
        void Add(wxWindow* child, int proportion, int flags, int border)
        {
            m_body->Add(child, proportion, flags, border);
        }
        void Add(wxSizer* child, int proportion, int flags, int border)
        {
            m_body->Add(child, proportion, flags, border);
        }
    };

    class ScreenCheck final : public wxControl
    {
        bool m_value = false;
        bool m_pressed = false;
        wxString m_label;

        void Toggle()
        {
            if (!IsEnabled()) return;
            SetValue(!m_value);
            wxCommandEvent event(wxEVT_CHECKBOX, GetId());
            event.SetEventObject(this);
            event.SetInt(m_value ? 1 : 0);
            GetEventHandler()->ProcessEvent(event);
        }

        void Paint(wxPaintEvent&)
        {
            wxAutoBufferedPaintDC dc(this);
            dc.SetBackground(wxBrush(spellcross_menu::Background()));
            dc.Clear();
            dc.SetFont(GetFont());
            const bool enabled = IsEnabled();
            const auto ink = enabled ? spellcross_menu::Text() : spellcross_strategic_ui::Inactive();
            const int side = FromDIP(16);
            const int x = FromDIP(3);
            const int y = (GetClientSize().y - side) / 2;
            dc.SetPen(wxPen(ink));
            dc.SetBrush(wxBrush(m_value && enabled ? spellcross_menu::Highlight() : spellcross_menu::Background()));
            dc.DrawRectangle(x, y, side, side);
            if (m_value)
            {
                dc.SetPen(wxPen(ink, FromDIP(2)));
                dc.DrawLine(x + FromDIP(3), y + FromDIP(8), x + FromDIP(7), y + FromDIP(12));
                dc.DrawLine(x + FromDIP(7), y + FromDIP(12), x + FromDIP(13), y + FromDIP(4));
            }
            dc.SetTextForeground(ink);
            const int labelX = x + side + FromDIP(9);
            dc.DrawLabel(m_label, wxRect(labelX, 0,
                (std::max)(1, GetClientSize().x - labelX - FromDIP(3)), GetClientSize().y),
                wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
            if (HasFocus() && enabled)
            {
                dc.SetPen(wxPen(spellcross_strategic_ui::Heading(), 1, wxPENSTYLE_DOT));
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawRectangle(GetClientRect().Deflate(1));
            }
        }

    protected:
        wxSize DoGetBestSize() const override
        {
            const auto extent = GetTextExtent(m_label);
            return wxSize(extent.x + FromDIP(34),
                (std::max)(FromDIP(28), extent.y + FromDIP(8)));
        }

    public:
        ScreenCheck(wxWindow* parent, wxWindowID id, const wxString& label)
            : wxControl(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
              m_label(label)
        {
            SetLabel(label);
            SetFont(spellcross_menu::MenuFont());
            SetBackgroundStyle(wxBG_STYLE_PAINT);
            Bind(wxEVT_PAINT, &ScreenCheck::Paint, this);
            Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
                SetFocus();
                m_pressed = true;
                if (!HasCapture()) CaptureMouse();
            });
            Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
                const bool activate = m_pressed && GetClientRect().Contains(e.GetPosition());
                m_pressed = false;
                if (HasCapture()) ReleaseMouse();
                if (activate) Toggle();
            });
            Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) { m_pressed = false; });
            Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& e) {
                if (e.GetKeyCode() == WXK_SPACE) Toggle();
                else e.Skip();
            });
            Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& e) { Refresh(false); e.Skip(); });
            Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) { Refresh(false); e.Skip(); });
        }
        bool GetValue() const { return m_value; }
        void SetValue(bool value) { m_value = value; Refresh(false); }
        bool Enable(bool enable = true) override
        {
            const bool changed = wxControl::Enable(enable);
            if (changed) Refresh(false);
            return changed;
        }
    };

    class ScreenChoice final : public wxOwnerDrawnComboBox
    {
    protected:
        // The native combo paints a light background around a centred bitmap.
        // Cover the entire button area, not only the arrow's bitmap, on every
        // paint (including hover, focus and a DPI/size change).
        void DrawButton(wxDC& dc, const wxRect& rect, int) override
        {
            if (rect.IsEmpty()) return;
            const bool enabled = IsEnabled();
            const bool hover = enabled && rect.Contains(ScreenToClient(wxGetMousePosition()));
            const auto background = hover || (enabled && IsPopupShown())
                ? spellcross_menu::Highlight() : spellcross_menu::Background();
            const auto ink = enabled ? spellcross_menu::Accent() : spellcross_strategic_ui::Inactive();
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(background));
            dc.DrawRectangle(rect);
            dc.SetPen(wxPen(spellcross_strategic_ui::Border()));
            dc.DrawLine(rect.x, rect.y, rect.x, rect.y + rect.height);
            dc.SetPen(wxPen(ink, FromDIP(2)));
            const int x = rect.x + rect.width / 2;
            const int y = rect.y + rect.height / 2;
            dc.DrawLine(x - FromDIP(5), y - FromDIP(2), x, y + FromDIP(3));
            dc.DrawLine(x, y + FromDIP(3), x + FromDIP(5), y - FromDIP(2));
        }
        void OnDrawBackground(wxDC& dc, const wxRect& rect, int,
                              int flags) const override
        {
            const bool selected = (flags & wxODCB_PAINTING_SELECTED) != 0 &&
                                  (flags & wxODCB_PAINTING_CONTROL) == 0;
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(selected ? spellcross_menu::Highlight() : spellcross_menu::Background()));
            dc.DrawRectangle(rect);
        }
        void OnDrawItem(wxDC& dc, const wxRect& rect, int item, int flags) const override
        {
            if (item == wxNOT_FOUND) return;
            dc.SetFont(GetFont());
            dc.SetTextForeground((flags & wxODCB_PAINTING_SELECTED) &&
                !(flags & wxODCB_PAINTING_CONTROL) ? spellcross_strategic_ui::Heading()
                : spellcross_menu::Text());
            dc.DrawLabel(GetString(static_cast<unsigned int>(item)),
                wxRect(rect.x + FromDIP(6), rect.y,
                    (std::max)(1, rect.width - FromDIP(12)), rect.height),
                wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
        }
        wxCoord OnMeasureItem(size_t) const override
        {
            return (std::max)(FromDIP(28), GetCharHeight() + FromDIP(8));
        }
    public:
        explicit ScreenChoice(wxWindow* parent)
            : wxOwnerDrawnComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                wxDefaultSize, 0, nullptr, wxCB_READONLY | wxBORDER_SIMPLE)
        {
            SetFont(spellcross_menu::MenuFont());
            SetBackgroundColour(spellcross_menu::Background());
            SetForegroundColour(spellcross_menu::Text());
            // Explicit custom dimensions keep wxWidgets out of the native
            // full-button drawing path; DrawButton handles the entire area.
            SetButtonPosition(FromDIP(30), FromDIP(28), wxRIGHT, 0);
        }
    };

    // All three top-level windows use this very same dialog and settings.
    // Changes preview live; Cancel restores the complete original state.
    inline bool ShowScreenDialog(wxWindow* parent, const std::function<void()>& refresh)
    {
        const Settings original = Current();
        wxDialog dialog(parent, wxID_ANY,
            spellcross_ui_locale::Tr("screen.title", "Screen"),
            wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
        auto* root = new wxBoxSizer(wxVERTICAL);
        const int space = dialog.FromDIP(10);
        auto* body = new wxScrolledWindow(&dialog,wxID_ANY,wxDefaultPosition,wxDefaultSize,
            wxVSCROLL | wxHSCROLL | wxTAB_TRAVERSAL | wxBORDER_NONE);
        body->SetScrollRate(dialog.FromDIP(12),dialog.FromDIP(12));
        auto* bodySizer = new wxBoxSizer(wxVERTICAL);
        body->SetSizer(bodySizer);
        root->Add(body,1,wxEXPAND);
        auto* columns = new wxFlexGridSizer(2, space, space);
        columns->AddGrowableCol(0, 1);
        columns->AddGrowableCol(1, 1);
        bodySizer->Add(columns, 0, wxEXPAND | wxALL, space);
        const auto group = [&](const wxString& title) {
            auto* section = new ScreenSection(body, title);
            columns->Add(section, 1, wxEXPAND);
            return section;
        };
        const auto addLabel = [&](ScreenSection* section, const wxString& title) {
            auto* label = new wxStaticText(section, wxID_ANY, title);
            label->SetFont(spellcross_menu::MenuFont());
            section->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, space);
        };
        auto* display = group(spellcross_ui_locale::Tr("screen.group.display", "Display and scale"));
        auto* renderer = group(spellcross_ui_locale::Tr("screen.group.renderer", "Rendering and terrain"));
        auto* colour = group(spellcross_ui_locale::Tr("screen.group.colour", "Colour correction"));
        auto* filterGroup = group(spellcross_ui_locale::Tr("screen.group.filters", "Image filters"));
        addLabel(display,spellcross_ui_locale::Tr("screen.dpi", "Game scale (DPI)"));
        const wxString scales[] = {
            spellcross_ui_locale::Tr("screen.scale.original", "100% - original pixels"),
            spellcross_ui_locale::Tr("screen.scale.enlarged", "125% - enlarged / sharp"),
            spellcross_ui_locale::Tr("screen.scale.smooth", "125% - enlarged / smoothed")
        };
        int selectedScale = original.scale_percent == 100 ? 0 :
            (original.smooth_scaling ? 2 : 1);
        std::array<spellcross_strategic_ui::Button*, 3> scaleButtons{};
        auto* scaleRow = new wxBoxSizer(wxVERTICAL);
        for (int i = 0; i < 3; ++i)
        {
            scaleButtons[i] = new spellcross_strategic_ui::Button(display->GetStaticBox(),
                wxID_ANY, scales[i]);
            scaleRow->Add(scaleButtons[i], 0, wxBOTTOM | wxEXPAND,
                i == 2 ? 0 : dialog.FromDIP(4));
        }
        display->Add(scaleRow, 0, wxEXPAND | wxALL, space);
        addLabel(display,spellcross_ui_locale::Tr("screen.lighting_intensity","Lighting intensity (%)"));
        auto* lightIntensity = new spellcross_strategic_ui::Slider(display->GetStaticBox(),
            wxID_ANY,original.lighting_intensity,0,100);
        display->Add(lightIntensity,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));
        addLabel(display,spellcross_ui_locale::Tr("screen.shadow_strength","Shadow strength (%)"));
        auto* shadowStrength = new spellcross_strategic_ui::Slider(display->GetStaticBox(),
            wxID_ANY,original.shadow_strength,0,100);
        display->Add(shadowStrength,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));

        addLabel(colour,spellcross_ui_locale::Tr("screen.brightness", "Brightness (%)"));
        auto* brightness = new spellcross_strategic_ui::Slider(colour->GetStaticBox(),
            wxID_ANY, original.brightness, 50, 150);
        colour->Add(brightness, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, dialog.FromDIP(14));

        addLabel(colour,spellcross_ui_locale::Tr("screen.gamma", "Gamma (x100)"));
        auto* gamma = new spellcross_strategic_ui::Slider(colour->GetStaticBox(),
            wxID_ANY, static_cast<int>(std::lround(original.gamma * 100)), 50, 200);
        colour->Add(gamma, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, dialog.FromDIP(14));

        addLabel(colour,spellcross_ui_locale::Tr("screen.temperature", "Color temperature (K)"));
        auto* temperature = new spellcross_strategic_ui::Slider(colour->GetStaticBox(),
            wxID_ANY, original.temperature, 3500, 9500);
        colour->Add(temperature, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
            dialog.FromDIP(14));

        auto* gpu = new ScreenCheck(renderer->GetStaticBox(),wxID_ANY,
            spellcross_ui_locale::Tr("screen.gpu_acceleration","GPU acceleration (Direct3D 11)"));
        gpu->SetValue(original.gpu_acceleration);
#ifndef _WIN32
        gpu->Disable();
#endif
        renderer->Add(gpu, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);
        auto* enhanced = new ScreenCheck(renderer->GetStaticBox(), wxID_ANY,
            spellcross_ui_locale::Tr("screen.advanced_rendering", "Advanced rendering (experimental)"));
        enhanced->SetValue(original.advanced_rendering);
        renderer->Add(enhanced, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);
        auto* shadows = new ScreenCheck(renderer->GetStaticBox(),wxID_ANY,
            spellcross_ui_locale::Tr("screen.directional_shadows","Directional shadows (daylight)"));
        shadows->SetValue(original.directional_shadows);
        renderer->Add(shadows,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        auto* originalShadows = new ScreenCheck(renderer->GetStaticBox(),wxID_ANY,
            spellcross_ui_locale::Tr("screen.original_shadows","Original unit shadows"));
        originalShadows->SetValue(original.original_shadows);
        renderer->Add(originalShadows,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        auto* materials = new ScreenCheck(renderer->GetStaticBox(), wxID_ANY,
            spellcross_ui_locale::Tr("screen.procedural_materials", "Procedural terrain materials (experimental)"));
        materials->SetValue(original.procedural_materials);
        renderer->Add(materials, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);
        auto* upscaling = new ScreenCheck(renderer->GetStaticBox(), wxID_ANY,
            spellcross_ui_locale::Tr("screen.procedural_upscaling", "Procedural sprite upscaling (experimental)"));
        upscaling->SetValue(original.procedural_upscaling);
        renderer->Add(upscaling, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);

        addLabel(filterGroup,spellcross_ui_locale::Tr("screen.tactical_filter", "Battlefield scaling filter"));
        auto* filter = new ScreenChoice(filterGroup->GetStaticBox());
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
        int filterWidth = dialog.FromDIP(280);
        for (unsigned int i = 0; i < filter->GetCount(); ++i)
            filterWidth = (std::max)(filterWidth, filter->GetTextExtent(filter->GetString(i)).x + dialog.FromDIP(42));
        filter->SetMinSize(wxSize(filterWidth, dialog.FromDIP(32)));
        filter->SetSelection(selectedFilter);
        auto* filterHint=new wxStaticText(filterGroup->GetStaticBox(),wxID_ANY,
            spellcross_ui_locale::Tr("screen.filter.hint","Selected filter replaces sprite reconstruction."));
        filterHint->SetFont(spellcross_menu::MenuFont());
        filterHint->Wrap(filterWidth);
        filterGroup->Add(filter, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);
        filterGroup->Add(filterHint,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        auto* dedither = new ScreenCheck(filterGroup->GetStaticBox(), wxID_ANY,
            spellcross_ui_locale::Tr("screen.tactical_dedither", "Reduce palette dithering"));
        dedither->SetValue(original.tactical_dedither);
        filterGroup->Add(dedither, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);
        addLabel(filterGroup,spellcross_ui_locale::Tr("screen.tactical_sharpen", "Adaptive sharpening (%)"));
        auto* sharpen = new spellcross_strategic_ui::Slider(filterGroup->GetStaticBox(),
            wxID_ANY, original.tactical_sharpen, 0, 100);
        filterGroup->Add(sharpen, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, dialog.FromDIP(14));

        auto* diagnostics = new ScreenSection(body,
            spellcross_ui_locale::Tr("screen.group.diagnostics","Diagnostics"));
        bodySizer->Add(diagnostics,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,space);
        const bool originalProfiler=spellcross_perf::Current().enabled;
        auto* profiler=new ScreenCheck(diagnostics->GetStaticBox(),wxID_ANY,
            spellcross_ui_locale::Tr("screen.perf.toggle","Performance diagnostics + debug log (Ctrl+Shift+F10)"));
        profiler->SetValue(originalProfiler);
        profiler->Bind(wxEVT_CHECKBOX,[&](wxCommandEvent&) {
            auto& perf=spellcross_perf::Current();
            if(perf.enabled!=profiler->GetValue()) perf.Toggle();
            if(refresh) refresh();
        });
        diagnostics->Add(profiler, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);

        auto* buttons = new wxBoxSizer(wxHORIZONTAL);
        buttons->AddStretchSpacer();
        auto* ok = new spellcross_strategic_ui::Button(&dialog, wxID_OK,
            spellcross_ui_locale::Tr("dialog.ok", "OK"));
        auto* cancel = new spellcross_strategic_ui::Button(&dialog, wxID_CANCEL,
            spellcross_ui_locale::Tr("dialog.cancel", "Cancel"));
        buttons->Add(ok, 0, wxRIGHT, dialog.FromDIP(8));
        buttons->Add(cancel, 0);
        root->Add(buttons, 0, wxEXPAND | wxALL, dialog.FromDIP(14));
        dialog.SetSizer(root);
        spellcross_strategic_ui::Apply(&dialog);
        // Measure with the final themed font and reserve equal column widths
        // so longer localized labels never compete with the adjacent column.
        int columnWidth = dialog.FromDIP(400);
        for (auto* section : {display, renderer, colour, filterGroup})
            columnWidth = (std::max)(columnWidth, section->GetSizer()->CalcMin().x);
        for (auto* section : {display, renderer, colour, filterGroup})
            section->SetMinSize(wxSize(columnWidth, -1));
        const auto contentMin=bodySizer->CalcMin();
        const int displayIndex=wxDisplay::GetFromWindow(parent?parent:&dialog);
        const auto work=wxDisplay(displayIndex==wxNOT_FOUND?0:static_cast<unsigned>(displayIndex)).GetClientArea();
        const int footer=buttons->CalcMin().y+dialog.FromDIP(28);
        body->SetMinSize(wxSize((std::min)(contentMin.x+dialog.FromDIP(24),(std::max)(320,work.width-dialog.FromDIP(50))),
            (std::min)(contentMin.y,(std::max)(150,work.height-footer-dialog.FromDIP(70)))));
        root->Fit(&dialog);
        body->FitInside();
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
        const auto updateFilterState = [&]() {
            upscaling->Enable(filter->GetSelection() == 0);
        };
        updateFilterState();
        const auto apply = [&]() {
            updateFilterState();
            Settings next;
            next.scale_percent = selectedScale == 0 ? 100 : 125;
            next.smooth_scaling = selectedScale == 2;
            next.brightness = brightness->GetValue();
            next.gamma = gamma->GetValue() / 100.0;
            next.temperature = temperature->GetValue();
            next.advanced_rendering = enhanced->GetValue();
            next.lighting_intensity = lightIntensity->GetValue();
            next.directional_shadows = shadows->GetValue();
            next.original_shadows = originalShadows->GetValue();
            next.shadow_strength = shadowStrength->GetValue();
            next.procedural_materials = materials->GetValue();
            next.procedural_upscaling = upscaling->GetValue();
            next.gpu_acceleration = gpu->GetValue();
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
        shadows->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        originalShadows->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        lightIntensity->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        shadowStrength->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        materials->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        upscaling->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        gpu->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        filter->Bind(wxEVT_COMBOBOX, [&](wxCommandEvent&) { apply(); });
        dedither->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        sharpen->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });

        if (dialog.ShowModal() == wxID_OK)
        {
            apply();
            return true;
        }
        Set(original);
        if(spellcross_perf::Current().enabled!=originalProfiler) spellcross_perf::Current().Toggle();
        if (refresh) refresh();
        return false;
    }

}
