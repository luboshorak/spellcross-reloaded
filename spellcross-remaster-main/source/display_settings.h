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
#include <wx/simplebook.h>
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
        bool material_v2 = false;
        int material_intensity = 100;
        bool animated_water = false;
        bool water_v2 = false; // Advanced mode; preserve legacy configs
        int water_intensity = 60;
        bool procedural_upscaling = false; // edge-aware tactical sprite reconstruction
        bool gpu_acceleration = false; // opt-in D3D11 compute, CPU fallback
        int tactical_filter = spellcross_filters::Legacy;
        bool tactical_dedither = false;
        int tactical_sharpen = 0;
        int lighting_intensity = 100;
        bool directional_shadows = false;
        bool original_shadows = true;
        int shadow_strength = 60;
        int shadow_direction = 0;
        int shadow_length = 100;
        int shadow_softness = 33;
        bool contact_shadows = true;
        int atmosphere = 0;
        int atmosphere_intensity = 45;
        bool atmosphere_v2 = false;
        bool ambient_occlusion = false;
        int ambient_intensity = 55;
        int ambient_radius = 8;
        int post_bloom=0,post_contrast=0,post_vignette=0,post_grain=0;
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
        s.shadow_direction=std::clamp(s.shadow_direction,0,3);
        s.shadow_length=std::clamp(s.shadow_length,25,200);
        s.shadow_softness=std::clamp(s.shadow_softness,0,100);
        s.material_intensity=std::clamp(s.material_intensity,0,100);
        s.water_intensity=std::clamp(s.water_intensity,0,100);
        s.atmosphere=std::clamp(s.atmosphere,0,4);
        s.atmosphere_intensity=std::clamp(s.atmosphere_intensity,0,100);
        s.ambient_intensity=std::clamp(s.ambient_intensity,0,100);
        s.ambient_radius=std::clamp(s.ambient_radius,2,12);
        s.post_bloom=std::clamp(s.post_bloom,0,100);
        s.post_contrast=std::clamp(s.post_contrast,0,100);
        s.post_vignette=std::clamp(s.post_vignette,0,100);
        s.post_grain=std::clamp(s.post_grain,0,100);
        return s;
    }

    enum class GraphicsPreset { Original, Balanced, Maximum, Defaults };
    inline Settings Preset(GraphicsPreset preset,const Settings& current)
    {
        Settings s;
        if(preset==GraphicsPreset::Defaults) return Normalize(s);
        // Preserve user calibration, DPI and backend preference. Presets
        // concern visual effects; selecting one never forces GPU hardware.
        s.scale_percent=current.scale_percent;s.smooth_scaling=current.smooth_scaling;
        s.brightness=current.brightness;s.gamma=current.gamma;s.temperature=current.temperature;
        s.gpu_acceleration=current.gpu_acceleration;
        if(preset!=GraphicsPreset::Original)
        {
            const bool maximum=preset==GraphicsPreset::Maximum;
            s.advanced_rendering=true;s.lighting_intensity=maximum?100:75;
            s.procedural_materials=true;s.material_v2=maximum;
            s.material_intensity=maximum?100:55;
            s.directional_shadows=true;s.shadow_strength=maximum?65:45;
            s.shadow_softness=maximum?50:33;
            s.ambient_occlusion=maximum;s.ambient_intensity=45;
            s.animated_water=true;s.water_v2=true;s.water_intensity=maximum?85:60;
            s.tactical_filter=maximum?spellcross_filters::EdgeAdaptive:spellcross_filters::SharpBilinear;
            s.tactical_dedither=maximum;s.tactical_sharpen=maximum?20:0;
            s.post_bloom=maximum?35:0;s.post_contrast=maximum?30:10;
            s.post_vignette=maximum?20:0;
        }
        // Weather is a manual creative choice, not a graphics quality level.
        return Normalize(s);
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
            old.material_v2 == s.material_v2 && old.material_intensity == s.material_intensity &&
            old.animated_water == s.animated_water && old.water_v2 == s.water_v2 && old.water_intensity == s.water_intensity &&
            old.procedural_upscaling == s.procedural_upscaling &&
            old.gpu_acceleration == s.gpu_acceleration &&
            old.tactical_filter == s.tactical_filter && old.tactical_dedither == s.tactical_dedither &&
            old.tactical_sharpen == s.tactical_sharpen &&
            old.lighting_intensity == s.lighting_intensity &&
            old.directional_shadows == s.directional_shadows && old.original_shadows == s.original_shadows &&
            old.shadow_strength == s.shadow_strength && old.shadow_direction == s.shadow_direction &&
            old.shadow_length == s.shadow_length && old.shadow_softness == s.shadow_softness &&
            old.contact_shadows == s.contact_shadows && old.atmosphere == s.atmosphere &&
            old.atmosphere_intensity == s.atmosphere_intensity && old.atmosphere_v2 == s.atmosphere_v2 &&
            old.ambient_occlusion == s.ambient_occlusion && old.ambient_intensity == s.ambient_intensity &&
            old.ambient_radius == s.ambient_radius && old.post_bloom == s.post_bloom &&
            old.post_contrast == s.post_contrast && old.post_vignette == s.post_vignette && old.post_grain == s.post_grain)
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
        // A themed navigation column instead of the native Windows tab bar.
        // Every page owns its controls; switching never rebuilds or applies them.
        auto* content = new wxBoxSizer(wxHORIZONTAL);
        auto* navigation = new wxBoxSizer(wxVERTICAL);
        auto* book = new wxSimplebook(&dialog,wxID_ANY);
        content->Add(navigation,0,wxEXPAND|wxRIGHT,space);
        content->Add(book,1,wxEXPAND);
        root->Add(content,1,wxEXPAND|wxALL,space);
        std::array<wxScrolledWindow*,7> pages{};
        std::array<spellcross_strategic_ui::Button*,7> tabs{};
        const std::array<wxString,7> names={
            spellcross_ui_locale::Tr("screen.tab.display","Display"),
            spellcross_ui_locale::Tr("screen.tab.lighting","Lighting"),
            spellcross_ui_locale::Tr("screen.tab.terrain","Terrain"),
            spellcross_ui_locale::Tr("screen.tab.atmosphere","Atmosphere"),
            spellcross_ui_locale::Tr("screen.tab.filters","Filters"),
            spellcross_ui_locale::Tr("screen.tab.post","Postprocessing"),
            spellcross_ui_locale::Tr("screen.tab.performance","Performance")
        };
        for(int i=0;i<static_cast<int>(pages.size());++i)
        {
            tabs[i]=new spellcross_strategic_ui::Button(&dialog,wxID_ANY,names[i]);
            tabs[i]->SetMinSize(wxSize(dialog.FromDIP(145),-1));
            navigation->Add(tabs[i],0,wxEXPAND|wxBOTTOM,dialog.FromDIP(5));
            pages[i]=new wxScrolledWindow(book,wxID_ANY,wxDefaultPosition,wxDefaultSize,
                wxVSCROLL|wxHSCROLL|wxTAB_TRAVERSAL|wxBORDER_NONE);
            pages[i]->SetScrollRate(dialog.FromDIP(12),dialog.FromDIP(12));
            pages[i]->SetSizer(new wxBoxSizer(wxVERTICAL));
            book->AddPage(pages[i],names[i],i==0);
        }
        const auto group = [&](int page,const wxString& title) {
            auto* section = new ScreenSection(pages[page],title);
            pages[page]->GetSizer()->Add(section,0,wxEXPAND|wxBOTTOM,space);
            return section;
        };
        const auto addLabel = [&](ScreenSection* section, const wxString& title) {
            auto* label = new wxStaticText(section, wxID_ANY, title);
            label->SetFont(spellcross_menu::MenuFont());
            section->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, space);
        };
        auto* presets = group(0,spellcross_ui_locale::Tr("screen.group.presets","Graphics presets"));
        auto* display = group(0,spellcross_ui_locale::Tr("screen.group.display", "Display and scale"));
        auto* renderer = group(6,spellcross_ui_locale::Tr("screen.group.backend", "Rendering backend"));
        auto* colour = group(0,spellcross_ui_locale::Tr("screen.group.colour", "Colour correction"));
        auto* filterGroup = group(4,spellcross_ui_locale::Tr("screen.group.filters", "Image filters"));
        auto* terrainGroup = group(2,spellcross_ui_locale::Tr("screen.group.terrain", "Terrain and water"));
        auto* shadowGroup = group(1,spellcross_ui_locale::Tr("screen.group.lighting", "Lighting and shadows"));
        auto* ambientGroup = group(1,spellcross_ui_locale::Tr("screen.group.ambient","Ambient occlusion"));
        auto* atmosphereGroup = group(3,spellcross_ui_locale::Tr("screen.group.atmosphere","Battlefield atmosphere"));
        auto* postGroup = group(5,spellcross_ui_locale::Tr("screen.tab.post","Postprocessing"));
        const std::array<wxString,4> presetLabels={
            spellcross_ui_locale::Tr("screen.preset.original","Original graphics"),
            spellcross_ui_locale::Tr("screen.preset.balanced","Balanced"),
            spellcross_ui_locale::Tr("screen.preset.maximum","Maximum quality"),
            spellcross_ui_locale::Tr("screen.preset.defaults","Reset all defaults")
        };
        std::array<spellcross_strategic_ui::Button*,4> presetButtons{};
        auto* presetRow=new wxFlexGridSizer(2,space,space);
        presetRow->AddGrowableCol(0,1);presetRow->AddGrowableCol(1,1);
        for(int i=0;i<4;++i)
        {
            presetButtons[i]=new spellcross_strategic_ui::Button(presets,wxID_ANY,presetLabels[i]);
            presetRow->Add(presetButtons[i],1,wxEXPAND);
        }
        presets->Add(presetRow,0,wxALL|wxEXPAND,space);
        const auto postSlider=[&](const wxString& label,int value) {
            addLabel(postGroup,label);
            auto* slider=new spellcross_strategic_ui::Slider(postGroup,wxID_ANY,value,0,100);
            postGroup->Add(slider,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));
            return slider;
        };
        auto* bloom=postSlider(spellcross_ui_locale::Tr("screen.post.bloom","Highlight glow (bloom, %)"),original.post_bloom);
        auto* contrast=postSlider(spellcross_ui_locale::Tr("screen.post.contrast","Contrast curve (%)"),original.post_contrast);
        auto* vignette=postSlider(spellcross_ui_locale::Tr("screen.post.vignette","Vignette (%)"),original.post_vignette);
        auto* grain=postSlider(spellcross_ui_locale::Tr("screen.post.grain","Film grain (%)"),original.post_grain);
        auto* postHint=new wxStaticText(postGroup,wxID_ANY,
            spellcross_ui_locale::Tr("screen.post.hint","0 disables an effect. Only visible battlefield pixels are processed."));
        postHint->Wrap(dialog.FromDIP(390));
        postGroup->Add(postHint,0,wxALL|wxEXPAND,space);
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
        addLabel(shadowGroup,spellcross_ui_locale::Tr("screen.lighting_intensity","Lighting intensity (%)"));
        auto* lightIntensity = new spellcross_strategic_ui::Slider(shadowGroup->GetStaticBox(),
            wxID_ANY,original.lighting_intensity,0,100);
        shadowGroup->Add(lightIntensity,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));
        addLabel(shadowGroup,spellcross_ui_locale::Tr("screen.shadow_strength","Shadow strength (%)"));
        auto* shadowStrength = new spellcross_strategic_ui::Slider(shadowGroup,
            wxID_ANY,original.shadow_strength,0,100);
        shadowGroup->Add(shadowStrength,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));
        addLabel(shadowGroup,spellcross_ui_locale::Tr("screen.shadow_direction","Light source direction"));
        auto* shadowDirection = new ScreenChoice(shadowGroup);
        shadowDirection->Append(spellcross_ui_locale::Tr("screen.shadow.nw","Upper left"));
        shadowDirection->Append(spellcross_ui_locale::Tr("screen.shadow.ne","Upper right"));
        shadowDirection->Append(spellcross_ui_locale::Tr("screen.shadow.se","Lower right"));
        shadowDirection->Append(spellcross_ui_locale::Tr("screen.shadow.sw","Lower left"));
        shadowDirection->SetSelection(original.shadow_direction);
        shadowDirection->SetMinSize(dialog.FromDIP(wxSize(180,32)));
        shadowGroup->Add(shadowDirection,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        addLabel(shadowGroup,spellcross_ui_locale::Tr("screen.shadow_length","Shadow length (%)"));
        auto* shadowLength = new spellcross_strategic_ui::Slider(shadowGroup,wxID_ANY,original.shadow_length,25,200);
        shadowGroup->Add(shadowLength,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));
        addLabel(shadowGroup,spellcross_ui_locale::Tr("screen.shadow_softness","Shadow softness (%)"));
        auto* shadowSoftness = new spellcross_strategic_ui::Slider(shadowGroup,wxID_ANY,original.shadow_softness,0,100);
        shadowGroup->Add(shadowSoftness,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));
        auto* contactShadows = new ScreenCheck(shadowGroup,wxID_ANY,
            spellcross_ui_locale::Tr("screen.contact_shadows","Contact shadows (advanced lighting)"));
        contactShadows->SetValue(original.contact_shadows);
        shadowGroup->Add(contactShadows,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        auto* ambient = new ScreenCheck(ambientGroup,wxID_ANY,
            spellcross_ui_locale::Tr("screen.ambient.enable","Ambient occlusion (contact depth)"));
        ambient->SetValue(original.ambient_occlusion);
        ambientGroup->Add(ambient,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        addLabel(ambientGroup,spellcross_ui_locale::Tr("screen.ambient.intensity","Occlusion intensity (%)"));
        auto* ambientIntensity=new spellcross_strategic_ui::Slider(ambientGroup,wxID_ANY,original.ambient_intensity,0,100);
        ambientGroup->Add(ambientIntensity,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));
        addLabel(ambientGroup,spellcross_ui_locale::Tr("screen.ambient.radius","Occlusion radius (pixels)"));
        auto* ambientRadius=new spellcross_strategic_ui::Slider(ambientGroup,wxID_ANY,original.ambient_radius,2,12);
        ambientGroup->Add(ambientRadius,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));

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
        auto* enhanced = new ScreenCheck(shadowGroup->GetStaticBox(), wxID_ANY,
            spellcross_ui_locale::Tr("screen.advanced_rendering", "Advanced rendering (experimental)"));
        enhanced->SetValue(original.advanced_rendering);
        shadowGroup->Add(enhanced, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);
        auto* shadows = new ScreenCheck(shadowGroup->GetStaticBox(),wxID_ANY,
            spellcross_ui_locale::Tr("screen.directional_shadows","Directional shadows (daylight)"));
        shadows->SetValue(original.directional_shadows);
        shadowGroup->Add(shadows,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        auto* originalShadows = new ScreenCheck(shadowGroup->GetStaticBox(),wxID_ANY,
            spellcross_ui_locale::Tr("screen.original_shadows","Original unit shadows"));
        originalShadows->SetValue(original.original_shadows);
        shadowGroup->Add(originalShadows,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        auto* materials = new ScreenCheck(terrainGroup->GetStaticBox(), wxID_ANY,
            spellcross_ui_locale::Tr("screen.procedural_materials", "Procedural terrain materials (experimental)"));
        materials->SetValue(original.procedural_materials);
        terrainGroup->Add(materials, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);
        auto* upscaling = new ScreenCheck(filterGroup->GetStaticBox(), wxID_ANY,
            spellcross_ui_locale::Tr("screen.procedural_upscaling", "Procedural sprite upscaling (experimental)"));
        upscaling->SetValue(original.procedural_upscaling);
        filterGroup->Add(upscaling, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, space);

        // Put master switches before their dependent sliders.
        auto* shadowSizer = enhanced->GetContainingSizer();
        shadowSizer->Detach(originalShadows);
        shadowSizer->Detach(shadows);
        shadowSizer->Detach(enhanced);
        shadowSizer->Insert(0,enhanced,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        shadowSizer->Insert(1,shadows,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        shadowSizer->Insert(2,originalShadows,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);

        auto* materialV2 = new ScreenCheck(terrainGroup,wxID_ANY,
            spellcross_ui_locale::Tr("screen.material_v2","Multi-scale terrain materials V2"));
        materialV2->SetValue(original.material_v2);
        terrainGroup->Add(materialV2,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        addLabel(terrainGroup,spellcross_ui_locale::Tr("screen.material_intensity","Material intensity (%)"));
        auto* materialIntensity = new spellcross_strategic_ui::Slider(terrainGroup,wxID_ANY,original.material_intensity,0,100);
        terrainGroup->Add(materialIntensity,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));
        addLabel(terrainGroup,spellcross_ui_locale::Tr("screen.animated_water","Water rendering"));
        auto* water = new ScreenChoice(terrainGroup);
        water->Append(spellcross_ui_locale::Tr("screen.water.original","Original"));
        water->Append(spellcross_ui_locale::Tr("screen.water.enhanced","Enhanced"));
        water->Append(spellcross_ui_locale::Tr("screen.water.advanced","Advanced V2"));
        water->SetSelection(original.animated_water?(original.water_v2?2:1):0);
        terrainGroup->Add(water,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        addLabel(terrainGroup,spellcross_ui_locale::Tr("screen.water_intensity","Water effect intensity (%)"));
        auto* waterIntensity = new spellcross_strategic_ui::Slider(terrainGroup,wxID_ANY,original.water_intensity,0,100);
        terrainGroup->Add(waterIntensity,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));

        addLabel(atmosphereGroup,spellcross_ui_locale::Tr("screen.atmosphere.mode","Weather effect"));
        auto* atmosphere = new ScreenChoice(atmosphereGroup);
        for(const auto& label : std::array<wxString,5>{
            spellcross_ui_locale::Tr("screen.atmosphere.off","Off"),
            spellcross_ui_locale::Tr("screen.atmosphere.mist","Drifting mist"),
            spellcross_ui_locale::Tr("screen.atmosphere.dust","Dust haze"),
            spellcross_ui_locale::Tr("screen.atmosphere.rain","Rain"),
            spellcross_ui_locale::Tr("screen.atmosphere.snow","Snow")})
            atmosphere->Append(label);
        atmosphere->SetSelection(original.atmosphere);
        atmosphere->SetMinSize(dialog.FromDIP(wxSize(280,32)));
        atmosphereGroup->Add(atmosphere,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        addLabel(atmosphereGroup,spellcross_ui_locale::Tr("screen.atmosphere.intensity","Effect intensity (%)"));
        auto* atmosphereIntensity = new spellcross_strategic_ui::Slider(atmosphereGroup,wxID_ANY,
            original.atmosphere_intensity,0,100);
        atmosphereGroup->Add(atmosphereIntensity,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,dialog.FromDIP(14));
        auto* weatherV2=new ScreenCheck(atmosphereGroup,wxID_ANY,
            spellcross_ui_locale::Tr("screen.atmosphere.v2","Layered weather over units and objects"));
        weatherV2->SetValue(original.atmosphere_v2);
        atmosphereGroup->Add(weatherV2,0,wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND,space);
        auto* weatherHint=new wxStaticText(atmosphereGroup,wxID_ANY,
            spellcross_ui_locale::Tr("screen.atmosphere.hint","Visual only. Layered weather also covers visible units and scenery; HUD and fog remain protected."));
        weatherHint->Wrap(dialog.FromDIP(390));
        atmosphereGroup->Add(weatherHint,0,wxEXPAND|wxALL,space);

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

        auto* diagnostics = group(6,spellcross_ui_locale::Tr("screen.group.diagnostics","Diagnostics"));
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
        const int displayIndex=wxDisplay::GetFromWindow(parent?parent:&dialog);
        const auto work=wxDisplay(displayIndex==wxNOT_FOUND?0:static_cast<unsigned>(displayIndex)).GetClientArea();
        int pageWidth=dialog.FromDIP(470),pageHeight=dialog.FromDIP(410);
        for(auto* page:pages)
        {
            const auto min=page->GetSizer()->CalcMin();
            pageWidth=(std::max)(pageWidth,min.x+dialog.FromDIP(22));
            pageHeight=(std::max)(pageHeight,min.y);
            page->FitInside();
        }
        const int navWidth=navigation->CalcMin().x+space;
        const int footer=buttons->CalcMin().y+dialog.FromDIP(70);
        book->SetMinSize(wxSize((std::min)(pageWidth,(std::max)(260,work.width-navWidth-dialog.FromDIP(50))),
            (std::min)(pageHeight,(std::max)(160,work.height-footer))));
        root->Fit(&dialog);
        const auto selectTab=[&](int selected) {
            book->SetSelection(selected);
            for(int i=0;i<static_cast<int>(pages.size());++i)
            {
                auto font=tabs[i]->GetFont();
                font.SetWeight(i==selected?wxFONTWEIGHT_BOLD:wxFONTWEIGHT_NORMAL);
                tabs[i]->SetFont(font);
                tabs[i]->Refresh(false);
            }
            pages[selected]->Layout();
            pages[selected]->FitInside();
        };
        for(int i=0;i<static_cast<int>(pages.size());++i)
            tabs[i]->Bind(wxEVT_BUTTON,[&,i](wxCommandEvent&){selectTab(i);});
        selectTab(0);
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
            materialV2->Enable(materials->GetValue());
            materialIntensity->Enable(materials->GetValue());
            waterIntensity->Enable(water->GetSelection()>0);
            atmosphereIntensity->Enable(atmosphere->GetSelection()>0);
            weatherV2->Enable(atmosphere->GetSelection()>0);
            ambientIntensity->Enable(ambient->GetValue());
            ambientRadius->Enable(ambient->GetValue());
            shadowStrength->Enable(shadows->GetValue());
            shadowDirection->Enable(shadows->GetValue());
            shadowLength->Enable(shadows->GetValue());
            shadowSoftness->Enable(shadows->GetValue());
            contactShadows->Enable(enhanced->GetValue());
            lightIntensity->Enable(enhanced->GetValue());
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
            next.shadow_direction = shadowDirection->GetSelection();
            next.shadow_length = shadowLength->GetValue();
            next.shadow_softness = shadowSoftness->GetValue();
            next.contact_shadows = contactShadows->GetValue();
            next.procedural_materials = materials->GetValue();
            next.material_v2 = materialV2->GetValue();
            next.material_intensity = materialIntensity->GetValue();
            next.animated_water = water->GetSelection()>0;
            next.water_v2 = water->GetSelection()==2;
            next.water_intensity = waterIntensity->GetValue();
            next.atmosphere = atmosphere->GetSelection();
            next.atmosphere_intensity = atmosphereIntensity->GetValue();
            next.atmosphere_v2=weatherV2->GetValue();
            next.ambient_occlusion=ambient->GetValue();
            next.ambient_intensity=ambientIntensity->GetValue();
            next.ambient_radius=ambientRadius->GetValue();
            next.post_bloom=bloom->GetValue();next.post_contrast=contrast->GetValue();
            next.post_vignette=vignette->GetValue();next.post_grain=grain->GetValue();
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
        const auto loadControls=[&](const Settings& s) {
            selectedScale=s.scale_percent==100?0:s.smooth_scaling?2:1;
            brightness->SetValue(s.brightness);gamma->SetValue(static_cast<int>(std::lround(s.gamma*100)));
            temperature->SetValue(s.temperature);enhanced->SetValue(s.advanced_rendering);
            lightIntensity->SetValue(s.lighting_intensity);shadows->SetValue(s.directional_shadows);
            originalShadows->SetValue(s.original_shadows);shadowStrength->SetValue(s.shadow_strength);
            shadowDirection->SetSelection(s.shadow_direction);shadowLength->SetValue(s.shadow_length);
            shadowSoftness->SetValue(s.shadow_softness);contactShadows->SetValue(s.contact_shadows);
            materials->SetValue(s.procedural_materials);materialV2->SetValue(s.material_v2);
            materialIntensity->SetValue(s.material_intensity);water->SetSelection(s.animated_water?(s.water_v2?2:1):0);
            waterIntensity->SetValue(s.water_intensity);atmosphere->SetSelection(s.atmosphere);
            atmosphereIntensity->SetValue(s.atmosphere_intensity);weatherV2->SetValue(s.atmosphere_v2);
            ambient->SetValue(s.ambient_occlusion);ambientIntensity->SetValue(s.ambient_intensity);
            ambientRadius->SetValue(s.ambient_radius);upscaling->SetValue(s.procedural_upscaling);
            gpu->SetValue(s.gpu_acceleration);
            const auto found=std::find(filterIds.begin(),filterIds.end(),s.tactical_filter);
            filter->SetSelection(found==filterIds.end()?0:static_cast<int>(found-filterIds.begin()));
            dedither->SetValue(s.tactical_dedither);sharpen->SetValue(s.tactical_sharpen);
            bloom->SetValue(s.post_bloom);contrast->SetValue(s.post_contrast);
            vignette->SetValue(s.post_vignette);grain->SetValue(s.post_grain);
            updateSelection();apply();
        };
        for(int i=0;i<4;++i)
            presetButtons[i]->Bind(wxEVT_BUTTON,[&,i](wxCommandEvent&){
                loadControls(Preset(static_cast<GraphicsPreset>(i),Current()));
            });
        for(auto* slider:{bloom,contrast,vignette,grain})
            slider->Bind(wxEVT_SLIDER,[&](wxCommandEvent&){apply();});
        updateSelection();
        brightness->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        gamma->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        temperature->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        enhanced->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        shadows->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        originalShadows->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        lightIntensity->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        shadowStrength->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        shadowDirection->Bind(wxEVT_COMBOBOX, [&](wxCommandEvent&) { apply(); });
        shadowLength->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        shadowSoftness->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        contactShadows->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        materials->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        materialV2->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { apply(); });
        materialIntensity->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        water->Bind(wxEVT_COMBOBOX, [&](wxCommandEvent&) { apply(); });
        waterIntensity->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        atmosphere->Bind(wxEVT_COMBOBOX, [&](wxCommandEvent&) { apply(); });
        atmosphereIntensity->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { apply(); });
        weatherV2->Bind(wxEVT_CHECKBOX,[&](wxCommandEvent&){apply();});
        ambient->Bind(wxEVT_CHECKBOX,[&](wxCommandEvent&){apply();});
        ambientIntensity->Bind(wxEVT_SLIDER,[&](wxCommandEvent&){apply();});
        ambientRadius->Bind(wxEVT_SLIDER,[&](wxCommandEvent&){apply();});
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
