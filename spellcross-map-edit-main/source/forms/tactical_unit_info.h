// Original Spellcross tactical unit information screen (1024x768).
// The complete chrome is decoded from the player's retail INFO.FS:
// IBG_768.LZ, INFOPAN.LZ, INFOLSTA.LZ and PALETA.NRM.
// No re-created textures, arbitrary wxWidgets borders or 640x480 scaling.
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
#include <cmath>
#include <cstring>
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
          m_data(data), m_hasResearch(std::move(hasResearch))
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        // Cover the entire tactical window, not just a 4:3 rectangle which
        // leaks the map/menu around a borderless modal dialog.
        if (parent)
            SetSize(parent->GetScreenRect());
        else
            SetClientSize(kW, kH);

        if (map)
        {
            for (MapUnit* unit : map->units)
            {
                if (!unit || !unit->unit || unit->hide || unit->isDead() ||
                    (unit->is_enemy && unit->is_visible != 2))
                    continue;
                if (unit == selected)
                    m_index = m_units.size();
                m_units.push_back(unit);
            }
        }
        if (m_units.empty() && selected)
        {
            m_units.push_back(selected);
            m_index = 0;
        }

        LoadChrome();
        Rebuild();
        Bind(wxEVT_PAINT, &TacticalUnitInfoDialog::OnPaint, this);
        // Act on mouse-down, not mouse-up: Windows can hand the latter to a
        // different HWND if the game had mouse capture while opening the modal.
        Bind(wxEVT_LEFT_DOWN, &TacticalUnitInfoDialog::OnClick, this);
        Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { EndModal(wxID_CANCEL); });
        Bind(wxEVT_CHAR_HOOK, &TacticalUnitInfoDialog::OnKey, this);
        Bind(wxEVT_MOUSEWHEEL, &TacticalUnitInfoDialog::OnWheel, this);
        UpdateComposite();
    }

    MapUnit* SelectedForBattle() const { return m_goTo; }

private:
    // Retail screen uses 1024x768: source illustrations are 437x390 pixels,
    // and original layout bitmaps are 1024x768 / 640x480 / 433x63.
    static constexpr int kW = 1024;
    static constexpr int kH = 768;
    static constexpr int kArtW = 437;
    static constexpr int kArtH = 390;
    static constexpr int kTextX = 663;
    static constexpr int kTextY = 168;
    static constexpr int kTextW = 164;
    static constexpr int kTextRows = 36;
    static constexpr int kTextStep = 12;
    static constexpr int kDescriptionX = 236;
    static constexpr int kDescriptionY = 171;
    static constexpr int kDescriptionW = 382;
    static constexpr int kDescriptionRows = 21;
    static constexpr int kDescriptionStep = 18;

    SpellData* m_data = nullptr;
    std::function<bool(MapUnit*)> m_hasResearch;
    std::vector<MapUnit*> m_units;
    size_t m_index = 0;
    MapUnit* m_goTo = nullptr;
    wxBitmap m_background;
    wxBitmap m_panel;
    wxBitmap m_footer;
    // Composite the expensive original bitmaps and bitmap font off-screen.
    // The paint handler then performs only one blit, eliminating scroll flicker.
    wxBitmap m_composite;
    std::vector<uint8_t> m_infoPalette;
    std::vector<std::string> m_pictures;
    size_t m_pictureIndex = 0;
    wxBitmap m_picture;
    std::vector<wxString> m_statsLines;
    std::vector<wxString> m_descriptionLines;
    bool m_showDescription = false;
    int m_lineOffset = 0;

    struct Button
    {
        const char* glyph;
        const char* face;
        int x;
        int y;
        int width;
    };
    // Exact button sequence according to the retail manual: scroll up/down,
    // previous/next unit, next picture, rename, goto, info text, exit.
    // First eight keys sit on INFOLSTA; the final narrow key is IEXIT__N.
    static constexpr Button kButtons[] = {
        {"TEXTUP",   "IINFO_LN", 222, 557, 42},
        {"TEXTDOWN", "IINFO__N", 269, 557, 42},
        {"INFOPREV", "IINFO__N", 316, 557, 42},
        {"INFONEXT", "IINFO__N", 363, 557, 42},
        {"INFOPICT", "IINFO__N", 410, 557, 42},
        {"INFOPREM", "IINFO__N", 457, 557, 42},
        {"INFOGOTO", "IINFO__N", 504, 557, 42},
        {"INFOTEXT", "IINFO_RN", 551, 557, 42},
        {"INFOEXIT", "IEXIT__N", 604, 557, 22}
    };

    MapUnit* Current() const
    {
        return m_index < m_units.size() ? m_units[m_index] : nullptr;
    }

    static wxString DecodeText(const std::string& input)
    {
        return wxString(char2wstringCP895(input.c_str()));
    }

    bool ReadFile(const std::string& name, std::vector<uint8_t>& out) const
    {
        if (!m_data || !m_data->info) return false;
        uint8_t* raw = nullptr;
        int size = 0;
        if (m_data->info->GetFile(name.c_str(), &raw, &size) != 0 ||
            !raw || size <= 0)
            return false;
        out.assign(raw, raw + size);
        return true;
    }

    wxBitmap MakeInfoBitmap(const char* name, int width, int height,
        bool transparent)
    {
        if (m_infoPalette.size() < 256 * 3) return wxBitmap();
        std::vector<uint8_t> input;
        if (!ReadFile(name, input)) return wxBitmap();
        try
        {
            LZWexpand decoder(width * height + 8);
            const std::vector<uint8_t>& pixels =
                decoder.Decode(input.data(), input.data() + input.size());
            // Retail LZW files can carry one or two final flush bytes.
            if (pixels.size() < static_cast<size_t>(width * height))
                return wxBitmap();
            wxImage image(width, height, false);
            if (transparent) image.InitAlpha();
            unsigned char* rgb = image.GetData();
            unsigned char* alpha = transparent ? image.GetAlpha() : nullptr;
            for (int i = 0; i < width * height; ++i)
            {
                const size_t index = static_cast<size_t>(pixels[i]) * 3;
                rgb[i * 3] = m_infoPalette[index];
                rgb[i * 3 + 1] = m_infoPalette[index + 1];
                rgb[i * 3 + 2] = m_infoPalette[index + 2];
                if (alpha) alpha[i] = pixels[i] ? 255 : 0;
            }
            return wxBitmap(image);
        }
        catch (...) { return wxBitmap(); }
    }

    void LoadChrome()
    {
        if (!ReadFile("PALETA.NRM", m_infoPalette) ||
            m_infoPalette.size() < 256 * 3)
            return;
        m_background = MakeInfoBitmap("IBG_768.LZ", 1024, 768, false);
        m_panel = MakeInfoBitmap("INFOPAN.LZ", 640, 480, true);
        m_footer = MakeInfoBitmap("INFOLSTA.LZ", 433, 63, true);
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
            LZWexpand decoder(kArtW * kArtH + 8);
            const std::vector<uint8_t>& pixels =
                decoder.Decode(compressed.data(), compressed.data() + compressed.size());
            if (pixels.size() < static_cast<size_t>(kArtW * kArtH))
                return;
            wxImage img(kArtW, kArtH, false);
            unsigned char* rgb = img.GetData();
            for (size_t i = 0; i < static_cast<size_t>(kArtW * kArtH); ++i)
            {
                const unsigned index = pixels[i];
                const size_t offset = index < 192 ? static_cast<size_t>(index) * 3 : 0;
                rgb[3 * i] = pal[offset];
                rgb[3 * i + 1] = pal[offset + 1];
                rgb[3 * i + 2] = pal[offset + 2];
            }
            m_picture = wxBitmap(img);
        }
        catch (...) { /* Corrupt/missing optional picture does not crash the battle. */ }
    }

    int TextWidth(const wxString& value) const
    {
        if (m_data && m_data->font)
        {
            std::string encoded = wstring2stringCP895(value.ToStdWstring());
            return m_data->font->GetTextWidth(encoded);
        }
        return static_cast<int>(value.length()) * 8;
    }

    void AppendWrapped(std::vector<wxString>& output, const wxString& paragraph, int maxWidth)
    {
        if (paragraph.empty())
        {
            output.emplace_back();
            return;
        }
        wxString line;
        wxStringTokenizer words(paragraph, " \t", wxTOKEN_STRTOK);
        while (words.HasMoreTokens())
        {
            const wxString word = words.GetNextToken();
            const wxString next = line.empty() ? word : line + " " + word;
            if (!line.empty() && TextWidth(next) > maxWidth)
            {
                output.push_back(line);
                line = word;
            }
            else line = next;
        }
        if (!line.empty()) output.push_back(line);
    }

    void Rebuild()
    {
        m_pictures.clear();
        m_pictureIndex = 0;
        m_lineOffset = 0;
        m_statsLines.clear();
        m_descriptionLines.clear();
        MapUnit* unit = Current();
        if (!unit || !unit->unit) return;

        const std::string root = unit->unit->info;
        if (!root.empty()) m_pictures.push_back(root);
        std::vector<uint8_t> lst;
        if (!root.empty() && ReadFile(root + ".LST", lst))
        {
            std::istringstream input(std::string(lst.begin(), lst.end()));
            std::string name;
            while (std::getline(input, name))
            {
                if (!name.empty() && name.back() == '\r') name.pop_back();
                if (name.find_first_not_of(" \t") == std::string::npos)
                    continue;
                std::vector<uint8_t> exists;
                if (ReadFile(name + ".LZ", exists) &&
                    std::find(m_pictures.begin(), m_pictures.end(), name) == m_pictures.end())
                    m_pictures.push_back(name);
            }
        }
        LoadPicture();

        // Manual p.9: unresearched enemy units reveal no encyclopedia text.
        const bool descriptionAllowed =
            !unit->is_enemy || (m_hasResearch && m_hasResearch(unit));
        std::vector<uint8_t> description;
        if (descriptionAllowed && !root.empty() &&
            (ReadFile(root + SpellLanguageExtension(m_data->GetTextLanguage()), description) ||
             ReadFile(root + ".ENG", description)))
        {
            const wxString source = DecodeText(std::string(description.begin(), description.end()));
            wxStringTokenizer paras(source, "\n", wxTOKEN_RET_EMPTY_ALL);
            while (paras.HasMoreTokens())
            {
                wxString para = paras.GetNextToken();
                para.Replace("\r", "");
                AppendWrapped(m_descriptionLines, para, kDescriptionW);
            }
        }

        const bool czech =
            std::string(SpellLanguageExtension(m_data->GetTextLanguage())) == ".CZ";
        const auto add = [this](const wxString& value) { m_statsLines.push_back(value); };
        const int kind = unit->unit->utype & SpellUnitRec::UTYPE_TYPE_MASK;
        if (czech)
            add(kind == SpellUnitRec::UTYPE_TYPE_ARMORED ? L"Těžká jednotka" :
                kind == SpellUnitRec::UTYPE_TYPE_LIGHT ? L"Lehká jednotka" : L"Vzdušná jednotka");
        else
            add(kind == SpellUnitRec::UTYPE_TYPE_ARMORED ? L"Heavy unit" :
                kind == SpellUnitRec::UTYPE_TYPE_LIGHT ? L"Light unit" : L"Air unit");
        add(DecodeText(unit->unit->name));
        add(czech
            ? (unit->is_enemy ? L"Nepřátelská jednotka" :
                unit->spec_type == MapUnitType::ArmyUnit ? L"Jednotka stálá" : L"Jednotka dočasná")
            : (unit->is_enemy ? L"Enemy unit" :
                unit->spec_type == MapUnitType::ArmyUnit ? L"Permanent unit" : L"Temporary unit"));

        add(czech ? L"Stav:" : L"Strength:");
        add(wxString::Format(czech ? L"   Aktuální: %d" : L"   Current: %d", unit->man));
        add(wxString::Format(czech ? L"   Maximální: %d" : L"   Maximum: %d", unit->unit->cnt));
        add(wxString::Format(czech ? L"   Zranění: %d" : L"   Wounded: %d", unit->wounded));
        add(wxString::Format(czech ? L"   Mrtví: %d" : L"   Dead: %d",
            std::max(0, unit->unit->cnt - unit->man - unit->wounded)));

        add(czech ? L"Akční body:" : L"Action points:");
        add(wxString::Format(czech ? L"   Aktuální: %d" : L"   Current: %d", unit->action_points));
        add(wxString::Format(czech ? L"   Maximální: %d" : L"   Maximum: %d", unit->GetMaxAP()));
        add(wxString::Format(czech ? L"   Na útok: %d" : L"   Per attack: %d", unit->GetAPperFire()));
        add(wxString::Format(czech ? L"   Na pohyb: %d" : L"   Per move: %d", unit->GetWalkAP()));
        add(wxString::Format(czech ? L"Úroveň: %d" : L"Level: %d", unit->experience_level));
        add(wxString::Format(czech ? L"Zkušenost: %d" : L"Experience: %d", unit->experience));
        if (unit->experience_level >= 12)
            add(czech ? L"Další úroveň: -" : L"Next level: -");
        else
            add(wxString::Format(czech ? L"Další úroveň: %d" : L"Next level: %d",
                unit->unit->GetNextExperiencePts(unit->experience_level)));
        add(wxString::Format(czech ? L"Morálka: %d" : L"Morale: %d",
            static_cast<int>(unit->morale)));
        add(wxString::Format(czech ? L"Dohled: %d" : L"Sight: %d", unit->unit->sdir));
        add(wxString::Format(czech ? L"Dostřel: %d" : L"Range: %d", unit->GetFireRange()));
        add(wxString::Format(czech ? L"Zakopání: %d" : L"Entrenchment: %d", unit->dig_level));

        const auto* bonuses = unit->unit->bonuses
            ? unit->unit->bonuses->GetBonus(unit->experience_level) : nullptr;
        const auto withBonuses = [](int total, int xp, int formation, int upgrade)
        {
            wxString result = wxString::Format(L"%d", total);
            if (!xp && !formation && !upgrade) return result;
            result += L" (";
            if (xp) result += wxString::Format(L"+%d", xp);
            if (formation) result += wxString::Format(L"+%d", formation);
            if (upgrade) result += wxString::Format(L"+%d", upgrade);
            result += L")";
            return result;
        };
        const int xpDefence = bonuses ? bonuses->defence : 0;
        const int xpAttack = bonuses ? bonuses->attack : 0;
        add(wxString(czech ? L"Obrana: " : L"Defence: ") +
            withBonuses(unit->GetDefence(), xpDefence,
                unit->formation_defence_bonus, unit->upgrade_defence_bonus));
        add(czech ? L"Útok:" : L"Attack:");
        const auto attack = [&](MapUnit::TARGET_TYPE type)
        {
            const int total = unit->GetAttack(type);
            return withBonuses(total,
                total ? xpAttack : 0,
                total ? unit->formation_attack_bonus : 0,
                total ? unit->upgrade_attack_bonus : 0);
        };
        add(wxString(czech ? L"   Lehké j.: " : L"   Light: ") + attack(MapUnit::TARGET_TYPE::LIGHT));
        add(wxString(czech ? L"   Těžké j.: " : L"   Armor: ") + attack(MapUnit::TARGET_TYPE::ARMOR));
        add(wxString(czech ? L"   Vzdušné j.: " : L"   Air: ") + attack(MapUnit::TARGET_TYPE::AIR));
        add(wxString(czech ? L"   Zdi a objekty: " : L"   Objects: ") +
            attack(MapUnit::TARGET_TYPE::OBJECT));
        add(czech ? L"Velitelé:" : L"Commanders:");
        if (unit->is_commander)
            add(czech ? L"   Přítomen" : L"   Present");
    }

    // COMMON.FS .ICO/.BTN graphics are used by several UIs with different
    // palettes. The tactical INFO palette is PALETA.NRM from INFO.FS; using
    // map_pal here turns original grey buttons neon green/magenta.
    void DrawInfoGraphic(wxDC& dc, const char* name, int x, int y)
    {
        if (!m_data || m_infoPalette.size() < 256 * 3) return;
        SpellGraphicItem* resource = m_data->gres.GetResource(name);
        if (!resource) return;
        SpellGraphicItem local = *resource;
        local.pal = reinterpret_cast<uint8_t (*)[3]>(m_infoPalette.data());
        std::unique_ptr<wxBitmap> bitmap(local.Render(true));
        if (bitmap && bitmap->IsOk())
            dc.DrawBitmap(*bitmap, x, y, true);
    }

    void DrawButton(wxDC& dc, const Button& button)
    {
        DrawInfoGraphic(dc, button.face, button.x, button.y);
        // IEXIT__N is the original self-contained 22x26 exit key with its
        // three dark bars. INFOEXIT.ICO is not layered on that narrow key.
        if (button.width == 22) return;
        // In text mode the same button offers to return to the unit picture.
        const char* iconName = (m_showDescription &&
            std::strcmp(button.glyph, "INFOTEXT") == 0)
            ? "INFOPICT" : button.glyph;
        SpellGraphicItem* glyph = m_data ? m_data->gres.GetResource(iconName) : nullptr;
        if (glyph)
            DrawInfoGraphic(dc, iconName,
                button.x + glyph->x_ofs, button.y + glyph->y_ofs);
    }

    bool DrawOriginalFont(wxDC& dc, const wxString& text,
        int x, int y, const wxColour& ink)
    {
        SpellFont* font = m_data ? m_data->font : nullptr;
        if (!font || text.empty()) return false;
        std::string original = wstring2stringCP895(text.ToStdWstring());
        const int width = std::clamp(font->GetTextWidth(original) + 5, 8, 768);
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

    struct View
    {
        double scale;
        double x;
        double y;
    };
    View GetView() const
    {
        const wxSize client = GetClientSize();
        const double scale = std::max(0.01, std::min(
            static_cast<double>(client.x) / kW,
            static_cast<double>(client.y) / kH));
        return { scale, (client.x - kW * scale) * 0.5,
            (client.y - kH * scale) * 0.5 };
    }

    // Render a complete 1024x768 frame to an off-screen bitmap. Rebuild on
    // unit/picture/text changes, never during the native Windows WM_PAINT.
    void UpdateComposite()
    {
        if (!m_composite.IsOk())
            m_composite = wxBitmap(kW, kH, 24);
        if (!m_composite.IsOk()) return;
        wxMemoryDC dc;
        dc.SelectObject(m_composite);
        DrawScene(dc);
        dc.SelectObject(wxNullBitmap);
        Refresh(false);
    }

    void DrawScene(wxDC& dc)
    {
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();

        if (m_background.IsOk())
            dc.DrawBitmap(m_background, 0, 0, false);
        if (m_picture.IsOk())
            dc.DrawBitmap(m_picture, 204, 154, false);
        if (m_panel.IsOk())
            dc.DrawBitmap(m_panel, 194, 144, true);
        if (m_footer.IsOk())
            dc.DrawBitmap(m_footer, 210, 554, true);

        const MapUnit* unit = Current();
        if (unit && unit->unit)
        {
            const std::string name = unit->name.empty()
                ? std::string(unit->unit->name) : unit->name;
            if (!DrawOriginalFont(dc, DecodeText(name), 724, 153,
                wxColour(235, 66, 45)))
            {
                dc.SetFont(wxFont(wxFontInfo(11).Family(wxFONTFAMILY_TELETYPE).Bold()));
                dc.SetTextForeground(wxColour(235, 66, 45));
                dc.DrawText(DecodeText(name), 724, 153);
            }
        }

        // The right-hand column ALWAYS contains tactical statistics.
        dc.SetClippingRegion(kTextX, kTextY, kTextW, kTextRows * kTextStep);
        dc.SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE)));
        dc.SetTextForeground(wxColour(146, 194, 140));
        for (int row = 0; row < kTextRows; ++row)
        {
            const size_t at = static_cast<size_t>(row);
            if (at >= m_statsLines.size()) break;
            const int y = kTextY + row * kTextStep;
            if (!DrawOriginalFont(dc, m_statsLines[at], kTextX, y,
                wxColour(146, 194, 140)))
                dc.DrawText(m_statsLines[at], kTextX, y);
        }
        dc.DestroyClippingRegion();

        // In original Spellcross the Info/Text button replaces the main
        // illustration area with the historical dossier, NOT the right-hand
        // statistics. The picture stays beneath the green lettering, as in
        // the user's DOSBox comparison.
        if (m_showDescription)
        {
            dc.SetClippingRegion(kDescriptionX, kDescriptionY,
                kDescriptionW, kDescriptionRows * kDescriptionStep);
            dc.SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE)));
            for (int row = 0; row < kDescriptionRows; ++row)
            {
                const size_t at = static_cast<size_t>(m_lineOffset + row);
                if (at >= m_descriptionLines.size()) break;
                const int y = kDescriptionY + row * kDescriptionStep;
                if (!DrawOriginalFont(dc, m_descriptionLines[at], kDescriptionX, y,
                    wxColour(174, 197, 164)))
                {
                    dc.SetTextForeground(wxColour(174, 197, 164));
                    dc.DrawText(m_descriptionLines[at], kDescriptionX, y);
                }
            }
            dc.DestroyClippingRegion();
        }

        for (const Button& button : kButtons)
            DrawButton(dc, button);
    }

    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
        const View view = GetView();
        dc.SetDeviceOrigin(static_cast<int>(std::round(view.x)),
            static_cast<int>(std::round(view.y)));
        dc.SetUserScale(view.scale, view.scale);
        if (m_composite.IsOk())
            dc.DrawBitmap(m_composite, 0, 0, false);
    }

    void ChangeUnit(int delta)
    {
        if (m_units.size() < 2) return;
        const int count = static_cast<int>(m_units.size());
        // The dossier cycles through the actual tactical-map units, not the
        // unit-type encyclopedia. Do not reset the selected index in Rebuild.
        m_index = static_cast<size_t>(
            (static_cast<int>(m_index) + (delta > 0 ? 1 : -1) + count) % count);
        Rebuild();
        UpdateComposite();
    }

    void ScrollText(int delta)
    {
        // Only the large text pane scrolls. Statistics are a separate
        // stationary column, exactly as on the DOSBox reference.
        if (!m_showDescription) return;
        const int maxOffset = std::max(
            0, static_cast<int>(m_descriptionLines.size()) - kDescriptionRows);
        const int next = std::clamp(m_lineOffset + delta, 0, maxOffset);
        if (next == m_lineOffset) return;
        m_lineOffset = next;
        UpdateComposite();
    }

    void Rename()
    {
        MapUnit* unit = Current();
        if (!unit || unit->is_enemy || unit->spec_type == MapUnitType::SpecUnit) return;
        wxString updated = DecodeText(unit->name.empty()
            ? std::string(unit->unit->name) : unit->name);
        if (!spellcross_dialog_ui::PromptText(this, "Unit name", "Enter unit's name", updated))
            return;
        updated.Trim(true).Trim(false);
        if (updated.size() > 27) updated.Truncate(27);
        unit->name = updated.empty() ? std::string() :
            wstring2stringCP895(updated.ToStdWstring());
        UpdateComposite();
    }

    void Activate(int index)
    {
        switch (index)
        {
        case 0: ScrollText(-1); break;
        case 1: ScrollText(1); break;
        case 2: ChangeUnit(-1); break;
        case 3: ChangeUnit(1); break;
        case 4:
            if (m_pictures.size() > 1)
            {
                m_pictureIndex = (m_pictureIndex + 1) % m_pictures.size();
                LoadPicture();
                UpdateComposite();
            }
            break;
        case 5: Rename(); break;
        case 6:
            if (Current() && !Current()->is_enemy)
            {
                m_goTo = Current();
                EndModal(wxID_OK);
            }
            break;
        case 7:
            m_showDescription = !m_showDescription;
            m_lineOffset = 0;
            UpdateComposite();
            break;
        case 8: EndModal(wxID_CANCEL); break;
        }
    }

    void OnClick(wxMouseEvent& ev)
    {
        const View view = GetView();
        const int x = static_cast<int>((ev.GetX() - view.x) / view.scale);
        const int y = static_cast<int>((ev.GetY() - view.y) / view.scale);
        for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); ++i)
        {
            const Button& button = kButtons[i];
            // Hitboxes follow the visible retail keys; a few pixels of
            // padding prevent clicks on the bevel from being lost.
            if (x >= button.x - 3 && x < button.x + button.width + 3 &&
                y >= button.y - 3 && y < button.y + 29)
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
        case WXK_RETURN: case WXK_NUMPAD_ENTER: Activate(6); break;
        default: ev.Skip(); break;
        }
    }
};
