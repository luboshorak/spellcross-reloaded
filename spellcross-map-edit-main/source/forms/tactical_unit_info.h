// Original Spellcross tactical unit information screen.
// All unit illustrations, palettes, descriptions and button artwork are loaded
// from the player's own original INFO.FS / COMMON.FS.
#pragma once

#include "../spellcross.h"
#include "../map.h"
#include "../fs_archive.h"
#include "../LZ_spell.h"
#include "../other.h"
#include "../game_dialogs.h"

#include <wx/dcbuffer.h>
#include <wx/tokenzr.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

class TacticalUnitInfoDialog final : public wxDialog
{
public:
    TacticalUnitInfoDialog(wxWindow* parent, SpellMap* map, SpellData* data,
        MapUnit* selected, std::function<bool(MapUnit*)> hasResearch)
        : wxDialog(parent, wxID_ANY, "Spellcross - Unit info",
            wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxSTAY_ON_TOP),
          m_map(map), m_data(data), m_hasResearch(std::move(hasResearch))
    {
        // A borderless, centred 4:3 game page, never an OS message box. Keep
        // the original 640x480 coordinate system while fitting fullscreen.
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinClientSize(wxSize(640, 480));
        const wxSize available = parent ? parent->GetClientSize() : wxSize(640, 480);
        const double scale = std::max(1.0, std::min(
            static_cast<double>(available.x) / 640.0,
            static_cast<double>(available.y) / 480.0));
        SetClientSize(static_cast<int>(640 * scale),
            static_cast<int>(480 * scale));
        for (MapUnit* unit : map->units)
        {
            if (!unit || !unit->unit || unit->hide || unit->isDead() ||
                (unit->is_enemy && unit->is_visible != 2))
                continue;
            if (unit == selected)
                m_index = m_units.size();
            m_units.push_back(unit);
        }
        if (m_units.empty() && selected)
        {
            m_units.push_back(selected);
            m_index = 0;
        }
        Bind(wxEVT_PAINT, &TacticalUnitInfoDialog::OnPaint, this);
        Bind(wxEVT_LEFT_UP, &TacticalUnitInfoDialog::OnClick, this);
        Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { EndModal(wxID_CANCEL); });
        Bind(wxEVT_CHAR_HOOK, &TacticalUnitInfoDialog::OnKey, this);
        Bind(wxEVT_MOUSEWHEEL, &TacticalUnitInfoDialog::OnWheel, this);
        Rebuild();
        CentreOnParent();
    }

    MapUnit* SelectedForBattle() const { return m_goTo; }

private:
    static constexpr int kW = 640;
    static constexpr int kH = 480;
    static constexpr int kArtW = 437;
    static constexpr int kArtH = 390;
    SpellMap* m_map = nullptr;
    SpellData* m_data = nullptr;
    std::function<bool(MapUnit*)> m_hasResearch;
    std::vector<MapUnit*> m_units;
    size_t m_index = 0;
    MapUnit* m_goTo = nullptr;
    std::vector<std::string> m_pictures;
    size_t m_pictureIndex = 0;
    wxBitmap m_picture;
    std::vector<wxString> m_lines;
    int m_lineOffset = 0;

    struct Button
    {
        const char* glyph;
        int x, y;
    };
    // Coordinates in the original 640 x 480 tactical information screen.
    static constexpr Button kButtons[] = {
        {"INFOPREV", 14, 407}, {"INFONEXT", 64, 407},
        {"INFOPICT", 114, 407}, {"INFOPREM", 164, 407},
        {"INFOGOTO", 214, 407}, {"INFOEXIT", 264, 407},
        {"TEXTUP", 490, 427}, {"TEXTDOWN", 547, 427}
    };

    MapUnit* Current() const
    {
        return m_index < m_units.size() ? m_units[m_index] : nullptr;
    }

    static wxString DecodeText(const std::string& s)
    {
        return wxString(char2wstringCP895(s.c_str()));
    }

    bool ReadFile(const std::string& file, std::vector<uint8_t>& out) const
    {
        if (!m_data || !m_data->info) return false;
        uint8_t* src = nullptr;
        int length = 0;
        if (m_data->info->GetFile(file.c_str(), &src, &length) != 0 ||
            !src || length <= 0)
            return false;
        out.assign(src, src + length);
        return true;
    }

    void LoadPicture()
    {
        m_picture = wxBitmap();
        if (m_pictures.empty()) return;
        const std::string& stem = m_pictures[m_pictureIndex];
        std::vector<uint8_t> compressed, pal;
        if (!ReadFile(stem + ".LZ", compressed) ||
            !ReadFile(stem + ".PAL", pal) || pal.size() < 192 * 3)
            return;
        try
        {
            LZWexpand decoder(kArtW * kArtH);
            std::vector<uint8_t> pixels =
                decoder.Decode(compressed.data(), compressed.data() + compressed.size());
            if (pixels.size() != static_cast<size_t>(kArtW * kArtH))
                return;
            wxImage img(kArtW, kArtH, false);
            unsigned char* rgb = img.GetData();
            for (size_t i = 0; i < pixels.size(); ++i)
            {
                const unsigned idx = pixels[i];
                const size_t off = idx < 192 ? idx * 3 : 0;
                rgb[3 * i] = pal[off];
                rgb[3 * i + 1] = pal[off + 1];
                rgb[3 * i + 2] = pal[off + 2];
            }
            m_picture = wxBitmap(img);
        }
        catch (...) { /* Missing/damaged art must not crash the tactical map. */ }
    }

    void AppendWrapped(const wxString& paragraph)
    {
        if (paragraph.empty())
        {
            m_lines.emplace_back();
            return;
        }
        wxClientDC measure(this);
        measure.SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE)));
        wxString line;
        wxStringTokenizer words(paragraph, " \t", wxTOKEN_STRTOK);
        while (words.HasMoreTokens())
        {
            const wxString next = words.GetNextToken();
            const wxString candidate = line.empty() ? next : line + " " + next;
            if (!line.empty() && measure.GetTextExtent(candidate).GetWidth() > 180)
            {
                m_lines.push_back(line);
                line = next;
            }
            else line = candidate;
        }
        if (!line.empty()) m_lines.push_back(line);
    }

    void AppendText(const wxString& text)
    {
        // Preserve the original Czech/English CP895 text after conversion to
        // Unicode; routing it through the Windows ANSI codepage loses accents.
        wxStringTokenizer paragraphs(text, "\n", wxTOKEN_RET_EMPTY_ALL);
        while (paragraphs.HasMoreTokens())
        {
            wxString line = paragraphs.GetNextToken();
            line.Replace("\r", "");
            AppendWrapped(line);
        }
    }

    void Rebuild()
    {
        m_pictures.clear();
        m_pictureIndex = 0;
        m_lineOffset = 0;
        m_lines.clear();
        MapUnit* u = Current();
        if (!u || !u->unit) return;

        const std::string root = u->unit->info;
        // The first image is named in JEDNOTKY.DEF; additional views are
        // enumerated by the original INFO.FS <stem>.LST, not invented.
        if (!root.empty()) m_pictures.push_back(root);
        std::vector<uint8_t> lst;
        if (ReadFile(root + ".LST", lst))
        {
            std::istringstream names(std::string(lst.begin(), lst.end()));
            std::string name;
            while (std::getline(names, name))
            {
                if (!name.empty() && name.back() == '\r') name.pop_back();
                if (name.empty() || name.find_first_not_of(" \t") == std::string::npos)
                    continue;
                std::vector<uint8_t> dummy;
                if (ReadFile(name + ".LZ", dummy) &&
                    std::find(m_pictures.begin(), m_pictures.end(), name) == m_pictures.end())
                    m_pictures.push_back(name);
            }
        }
        LoadPicture();

        std::vector<uint8_t> description;
        const bool known = !u->is_enemy || (m_hasResearch && m_hasResearch(u));
        if (!known)
            AppendWrapped("Unknown hostile unit. Research required.");
        else if (ReadFile(root + SpellLanguageExtension(m_data->GetTextLanguage()), description) ||
                 ReadFile(root + ".ENG", description))
        {
            // Original unit description, retained in the source edition's
            // CP895 encoding rather than replacing it with generated prose.
            AppendText(DecodeText(std::string(description.begin(), description.end())));
        }
        else
            AppendWrapped("No original unit description available.");

        // The long form includes live tactical data; never show enemy combat
        // statistics which the original research system has not revealed.
        if (!known) return;
        m_lines.emplace_back();
        AppendWrapped("---- Combat information ----");
        AppendWrapped(wxString::Format("Strength %d / %d", u->man, u->unit->cnt));
        AppendWrapped(wxString::Format("Wounded %d  Dead %d", u->wounded,
            std::max(0, u->unit->cnt - u->man - u->wounded)));
        AppendWrapped(wxString::Format("Morale %d%%", static_cast<int>(u->morale)));
        AppendWrapped(wxString::Format("AP %d / %d  Fire %d  Walk %d",
            u->action_points, u->GetMaxAP(), u->GetAPperFire(), u->GetWalkAP()));
        AppendWrapped(wxString::Format("Experience lvl %d  XP %d",
            u->experience_level, u->experience));
        AppendWrapped(wxString::Format("Next level %d",
            u->unit->GetNextExperiencePts(u->experience_level)));
        AppendWrapped(wxString::Format("Sight %d  Range %d",
            u->unit->sdir, u->GetFireRange()));
        AppendWrapped(wxString::Format("Fortify %d  Defence %d",
            u->dig_level, u->GetDefence()));
        AppendWrapped(wxString::Format("Attacks light/heavy/air %d/%d/%d",
            u->GetAttack(MapUnit::TARGET_TYPE::LIGHT),
            u->GetAttack(MapUnit::TARGET_TYPE::ARMOR),
            u->GetAttack(MapUnit::TARGET_TYPE::AIR)));
        AppendWrapped(wxString::Format("Attack objects %d  Shots %d",
            u->unit->attack_objects, u->GetMaxFireCount()));
        if (u->formation_id > 0)
            AppendWrapped(wxString::Format("Formation %d", u->formation_id));
        if (u->is_commander)
            AppendWrapped("Commander present");
    }

    static void DrawNative(wxDC& dc, SpellGraphicItem* resource, int x, int y)
    {
        if (!resource) return;
        std::unique_ptr<wxBitmap> bitmap(resource->Render(true));
        if (bitmap && bitmap->IsOk()) dc.DrawBitmap(*bitmap, x, y, true);
    }

    // The retail tactical archives supply 76x10, 10x69 and 10x10 stone
    // border pieces. Tile those native pixel graphics instead of painting a
    // custom approximated frame around the unit dossier.
    void DrawOriginalFrame(wxDC& dc, const wxRect& rect)
    {
        auto* hres = m_data->gres.wm_frame_horz;
        auto* vres = m_data->gres.wm_frame_vert;
        auto* corner = m_data->gres.wm_frame_corner;
        if (!hres || !vres || !corner) return;
        std::unique_ptr<wxBitmap> h(hres->Render(true));
        std::unique_ptr<wxBitmap> v(vres->Render(true));
        if (!h || !v || !h->IsOk() || !v->IsOk()) return;

        const int inset = 10;
        const int width = rect.width - 2 * inset;
        const int height = rect.height - 2 * inset;
        for (int pos = 0; pos < width; pos += h->GetWidth())
        {
            const int copy = std::min(h->GetWidth(), width - pos);
            const wxBitmap tile = h->GetSubBitmap(
                wxRect(0, 0, copy, h->GetHeight()));
            dc.DrawBitmap(tile, rect.x + inset + pos, rect.y, true);
            dc.DrawBitmap(tile, rect.x + inset + pos,
                rect.y + rect.height - h->GetHeight(), true);
        }
        for (int pos = 0; pos < height; pos += v->GetHeight())
        {
            const int copy = std::min(v->GetHeight(), height - pos);
            const wxBitmap tile = v->GetSubBitmap(
                wxRect(0, 0, v->GetWidth(), copy));
            dc.DrawBitmap(tile, rect.x, rect.y + inset + pos, true);
            dc.DrawBitmap(tile,
                rect.x + rect.width - v->GetWidth(), rect.y + inset + pos, true);
        }
        DrawNative(dc, corner, rect.x, rect.y);
        DrawNative(dc, corner, rect.x + rect.width - inset, rect.y);
        DrawNative(dc, corner, rect.x, rect.y + rect.height - inset);
        DrawNative(dc, corner, rect.x + rect.width - inset,
            rect.y + rect.height - inset);
    }

    void DrawButton(wxDC& dc, const Button& b)
    {
        DrawNative(dc, m_data->gres.GetResource("IINFO__N"), b.x, b.y);
        SpellGraphicItem* icon = m_data->gres.GetResource(b.glyph);
        if (icon)
            DrawNative(dc, icon, b.x + (42 - icon->x_size) / 2,
                b.y + (26 - icon->y_size) / 2);
    }

    bool DrawOriginalFont(wxDC& dc, const wxString& text,
        int x, int y, const wxColour& ink)
    {
        SpellFont* font = m_data ? m_data->font : nullptr;
        if (!font || text.empty()) return false;
        const std::string original = wstring2stringCP895(text.ToStdWstring());
        const int width = std::clamp(font->GetTextWidth(original) + 5, 8, 188);
        const int height = std::max(font->GetHeight() + 4, 16);
        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height, 0);
        font->Render(pixels.data(), pixels.data() + pixels.size(), width,
            1, 1, original, 7, -1, SpellFont::NONE);
        wxImage img(width, height);
        img.InitAlpha();
        unsigned char* rgb = img.GetData();
        unsigned char* alpha = img.GetAlpha();
        for (size_t i = 0; i < pixels.size(); ++i)
        {
            rgb[3 * i] = ink.Red();
            rgb[3 * i + 1] = ink.Green();
            rgb[3 * i + 2] = ink.Blue();
            alpha[i] = pixels[i] == 7 ? 255 : 0;
        }
        dc.DrawBitmap(wxBitmap(img), x, y, true);
        return true;
    }

    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxSize client = GetClientSize();
        dc.SetUserScale(static_cast<double>(client.x) / kW,
            static_cast<double>(client.y) / kH);
        dc.SetBackground(wxBrush(wxColour(7, 14, 12)));
        dc.Clear();

        if (m_picture.IsOk()) dc.DrawBitmap(m_picture, 0, 0, false);
        DrawOriginalFrame(dc, wxRect(kArtW, 0, kW - kArtW, kH));
        DrawOriginalFrame(dc, wxRect(0, kArtH, kArtW, 66));
        dc.SetPen(wxPen(wxColour(47, 64, 44)));
        dc.DrawLine(444, 31, 633, 31);

        const MapUnit* u = Current();
        if (u && u->unit)
        {
            const std::string name = u->name.empty() ? std::string(u->unit->name) : u->name;
            if (!DrawOriginalFont(dc, DecodeText(name), 444, 9,
                wxColour(234, 89, 64)))
            {
                dc.SetFont(wxFont(wxFontInfo(11).Family(wxFONTFAMILY_TELETYPE).Bold()));
                dc.SetTextForeground(wxColour(234, 89, 64));
                dc.DrawText(DecodeText(name), 444, 9);
            }
        }

        dc.SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE)));
        dc.SetTextForeground(wxColour(151, 197, 135));
        constexpr int kRows = 22;
        for (int i = 0; i < kRows; ++i)
        {
            const size_t idx = static_cast<size_t>(m_lineOffset + i);
            if (idx >= m_lines.size()) break;
            if (!DrawOriginalFont(dc, m_lines[idx], 445, 38 + i * 17,
                wxColour(151, 197, 135)))
                dc.DrawText(m_lines[idx], 445, 38 + i * 17);
        }
        for (const auto& button : kButtons) DrawButton(dc, button);
        dc.SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE)));
        dc.SetTextForeground(wxColour(153, 164, 126));
        dc.DrawText(wxString::Format("%zu/%zu", m_pictureIndex + 1,
            std::max<size_t>(1, m_pictures.size())), 362, 415);
        dc.DrawText("Esc / RMB", 309, 449);
    }

    void ChangeUnit(int delta)
    {
        if (m_units.empty()) return;
        const int count = static_cast<int>(m_units.size());
        m_index = static_cast<size_t>((static_cast<int>(m_index) + delta + count) % count);
        Rebuild();
        Refresh(false);
    }

    void ScrollText(int delta)
    {
        const int maxOffset = std::max(0, static_cast<int>(m_lines.size()) - 22);
        m_lineOffset = std::clamp(m_lineOffset + delta, 0, maxOffset);
        Refresh(false);
    }

    void Rename()
    {
        MapUnit* u = Current();
        if (!u || u->is_enemy || u->spec_type == MapUnitType::SpecUnit) return;
        wxString updated = DecodeText(u->name.empty()
            ? std::string(u->unit->name) : u->name);
        if (!spellcross_dialog_ui::PromptText(this, "Unit name", "Enter unit's name", updated))
            return;
        updated.Trim(true).Trim(false);
        if (updated.size() > 27) updated.Truncate(27);
        // An empty name restores the unit type's original name.
        u->name = updated.empty() ? std::string() :
            wstring2stringCP895(updated.ToStdWstring());
        Refresh(false);
    }

    void Activate(int index)
    {
        switch (index)
        {
        case 0: ChangeUnit(-1); break;
        case 1: ChangeUnit(1); break;
        case 2:
            if (m_pictures.size() > 1)
            {
                m_pictureIndex = (m_pictureIndex + 1) % m_pictures.size();
                LoadPicture();
                Refresh(false);
            }
            break;
        case 3: Rename(); break;
        case 4:
            if (Current() && !Current()->is_enemy)
            {
                m_goTo = Current();
                EndModal(wxID_OK);
            }
            break;
        case 5: EndModal(wxID_CANCEL); break;
        case 6: ScrollText(-1); break;
        case 7: ScrollText(1); break;
        }
    }

    void OnClick(wxMouseEvent& ev)
    {
        const wxSize client = GetClientSize();
        if (client.x <= 0 || client.y <= 0) return;
        const int x = ev.GetX() * kW / client.x;
        const int y = ev.GetY() * kH / client.y;
        for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); ++i)
        {
            const Button& button = kButtons[i];
            if (x >= button.x && x < button.x + 42 &&
                y >= button.y && y < button.y + 26)
            {
                Activate(static_cast<int>(i));
                return;
            }
        }
    }

    void OnWheel(wxMouseEvent& ev)
    {
        ScrollText(ev.GetWheelRotation() < 0 ? 3 : -3);
    }

    void OnKey(wxKeyEvent& ev)
    {
        switch (ev.GetKeyCode())
        {
        case WXK_ESCAPE: EndModal(wxID_CANCEL); break;
        case WXK_UP: ScrollText(-1); break;
        case WXK_DOWN: ScrollText(1); break;
        case WXK_PAGEUP: ScrollText(-12); break;
        case WXK_PAGEDOWN: ScrollText(12); break;
        case WXK_LEFT: ChangeUnit(-1); break;
        case WXK_RIGHT: ChangeUnit(1); break;
        case WXK_RETURN: case WXK_NUMPAD_ENTER: Activate(4); break;
        default: ev.Skip(); break;
        }
    }
};
