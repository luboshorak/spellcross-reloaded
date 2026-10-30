#include "form_mmenu.h"
#include "app_identity.h"
#include "../spell_ui_locale.h"
#include "../spell_units.h"

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/frame.h>
#include <wx/log.h>
#include <wx/slider.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

#include "LZ_spell.h"

namespace {

std::string to_lower(std::string s)
{
    for (char& ch : s)
        ch = static_cast<char>(::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool LoadFileBytes(const std::filesystem::path& path, std::vector<unsigned char>& out)
{
    out.clear();
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamsize n = f.tellg();
    f.seekg(0, std::ios::beg);
    if (n <= 0) return false;
    out.resize(static_cast<size_t>(n));
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), n));
}

std::filesystem::path FindFileCaseInsensitive(const std::filesystem::path& dir, const std::string& wanted)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return {};
    const std::string w = to_lower(wanted);
    for (const auto& de : fs::directory_iterator(dir, ec))
    {
        if (ec) break;
        if (!de.is_regular_file(ec)) continue;
        const std::string fn = to_lower(de.path().filename().string());
        if (fn == w) return de.path();
    }
    return {};
}

std::filesystem::path FindMenuFile(const std::filesystem::path& root, const std::string& filename)
{
    namespace fs = std::filesystem;
    fs::path temp_common = fs::current_path() / "temp" / "COMMON";

    const std::vector<std::filesystem::path> candidates = {
        root,
        root / "DATA",
        root / "COMMON",
        root / "DATA" / "COMMON",
        root / "CD",
        root / "DATA" / "CD",
        temp_common,
    };

    for (const auto& dir : candidates)
    {
        auto found = FindFileCaseInsensitive(dir, filename);
        if (!found.empty())
            return found;
    }
    return {};
}

bool ExpandPaletteTo256(const std::vector<unsigned char>& palBytes, std::array<unsigned char, 256 * 3>& pal256)
{
    pal256.fill(0);
    if (palBytes.size() < 3) return false;

    const size_t colors = palBytes.size() / 3;
    if (colors != 32 && colors != 64 && colors != 256) return false;

    unsigned char maxv = 0;
    for (size_t i = 0; i < colors * 3; ++i) maxv = std::max(maxv, palBytes[i]);
    const bool is_vga6 = (maxv <= 63);

    auto to8 = [&](unsigned char v) -> unsigned char {
        return is_vga6 ? (unsigned char)std::min(255, (int)v * 4) : v;
    };

    for (size_t i = 0; i < 256; ++i)
    {
        const size_t src = (i % colors) * 3;
        pal256[i * 3 + 0] = to8(palBytes[src + 0]);
        pal256[i * 3 + 1] = to8(palBytes[src + 1]);
        pal256[i * 3 + 2] = to8(palBytes[src + 2]);
    }
    return true;
}

struct DecodedIndexed
{
    int w = 0;
    int h = 0;
    std::vector<unsigned char> pixels;
};

bool DecodeIndexedMaybeHeader(const std::vector<unsigned char>& src, DecodedIndexed& out)
{
    out = {};
    if (src.size() < 4) return false;

    auto rd16 = [&](size_t off) -> unsigned {
        return (unsigned)src[off] | ((unsigned)src[off + 1] << 8);
    };

    unsigned w = rd16(0);
    unsigned h = rd16(2);
    if (w == 0 || h == 0 || w > 4096 || h > 4096) return false;

    const size_t need = (size_t)w * (size_t)h;
    if (src.size() < 4 + need) return false;

    out.w = (int)w;
    out.h = (int)h;
    out.pixels.assign(src.begin() + 4, src.begin() + 4 + need);
    return true;
}

bool GuessDimsFromSize(const std::vector<unsigned char>& pixels, DecodedIndexed& out, std::initializer_list<int> widths)
{
    out = {};
    for (int w : widths)
    {
        if (w <= 0) continue;
        if (pixels.size() % (size_t)w != 0) continue;
        int h = (int)(pixels.size() / (size_t)w);
        if (h <= 0 || h > 480) continue;
        out.w = w; out.h = h; out.pixels = pixels;
        return true;
    }
    return false;
}

wxBitmap MakeBitmapFromIndexed(const DecodedIndexed& d, const std::array<unsigned char, 256 * 3>& pal256, bool idx0Transparent)
{
    if (d.w <= 0 || d.h <= 0 || d.pixels.size() < (size_t)d.w * (size_t)d.h)
        return wxBitmap();

    wxImage img(d.w, d.h, true);
    if (idx0Transparent)
        img.InitAlpha();

    for (int y = 0; y < d.h; ++y)
    {
        for (int x = 0; x < d.w; ++x)
        {
            const unsigned char idx = d.pixels[(size_t)y * d.w + x];
            img.SetRGB(x, y,
                pal256[(size_t)idx * 3 + 0],
                pal256[(size_t)idx * 3 + 1],
                pal256[(size_t)idx * 3 + 2]);

            if (idx0Transparent)
                img.SetAlpha(x, y, (idx == 0) ? 0 : 255);
        }
    }
    return wxBitmap(img);
}

bool DecodeMainMenuPixels(const std::vector<unsigned char>& src, std::vector<unsigned char>& out, int w, int h)
{
    const size_t need = (size_t)w * (size_t)h;

    if (src.size() == need)
    {
        out = src;
        return true;
    }

    if (src.size() >= 4)
    {
        auto rd16 = [&](size_t off) -> unsigned {
            return (unsigned)src[off] | ((unsigned)src[off + 1] << 8);
        };
        unsigned tw = rd16(0);
        unsigned th = rd16(2);
        if (tw == (unsigned)w && th == (unsigned)h && src.size() >= 4 + need)
        {
            out.assign(src.begin() + 4, src.begin() + 4 + need);
            return true;
        }
    }
    return false;
}

bool LoadBytesFromFS(FSarchive* fs, const char* name, std::vector<unsigned char>& out)
{
    out.clear();
    if (!fs) return false;
    uint8_t* data = nullptr;
    int size = 0;
    if (fs->GetFile(name, &data, &size))
        return false;
    if (!data || size <= 0)
        return false;
    out.assign(data, data + size);
    return true;
}

} // namespace

FormMainMenu::FormMainMenu(wxPanel* parent,
    wxWindowID win_id,
    SpellMap* spell_map,
    SpellData* spell_data,
    std::function<void(FormMainMenuAction)> action_cb)
{
    m_spell_map = spell_map;
    m_spelldata = spell_data;
    spellcross_ui_locale::SetLanguageFromEdition(
        m_spelldata && m_spelldata->units && m_spelldata->units->IsEnglish());
    m_action_cb = std::move(action_cb);
    m_event_parent = parent;
    m_hover_index = -1;

    m_panel = wxBitmap();
    m_panel_pos = wxPoint(0, 0);
    m_panel_size = wxSize(0, 0);
    m_pal256_ok = false;
    m_pal256.fill(0);

    LoadBackground();
    LoadPanel();
    LoadNativeHover();
    BuildMenuItems(); // native TUTMNU.LZ is loaded on demand

    // Match the strategic-level window footprint and scale the original
    // 640x480 menu inside it. The frame is intentionally independent from the
    // hidden tactical MainFrame so hiding the tactical window cannot hide the
    // main menu with it.
    const wxSize windowSize(1390, 1050);
    long style = wxDEFAULT_FRAME_STYLE;
    form = new wxFrame(nullptr, win_id, "Spellcross", wxDefaultPosition, windowSize, style);
    spellcross_app::ApplyWindowIcon(form);
    form->SetMinSize(wxSize(660, 520));
    form->SetBackgroundStyle(wxBG_STYLE_PAINT);
    // Avoid native MSW composition during hover redraws: OnPaint explicitly
    // buffers the completed menu frame before presenting it.
    form->SetDoubleBuffered(false);

    // copy icon from parent top-level window
    wxWindow* tlw = parent;
    while (tlw && tlw->GetParent()) tlw = tlw->GetParent();
    if (auto* frame_tlw = dynamic_cast<wxFrame*>(tlw))
    {
        wxIcon ico = frame_tlw->GetIcon();
        if (ico.IsOk())
            form->SetIcon(ico);
    }

    // menu bar: Options > Audio, Screen
    auto* bar = new wxMenuBar();
    auto* optMenu = new wxMenu();
    optMenu->Append(ID_MMENU_OPTIONS_AUDIO, spellcross_ui_locale::Tr("menu.home.audio", "&Audio...\tCtrl+A"));
    optMenu->Append(ID_MMENU_OPTIONS_SCREEN, spellcross_ui_locale::Tr("menu.home.screen", "&Screen...\tCtrl+S"));
    bar->Append(optMenu, spellcross_ui_locale::Tr("menu.options", "&Options"));
    auto* editorMenu = new wxMenu();
    editorMenu->Append(ID_MMENU_OPEN_EDITOR, spellcross_ui_locale::Tr("menu.home.open_editor", "Open Map Editor..."), spellcross_ui_locale::Tr("menu.home.editor_help", "Launch the standalone Spellcross map editor"));
    bar->Append(editorMenu, spellcross_ui_locale::Tr("menu.home.editor", "&Map Editor"));
    form->SetMenuBar(bar);
    form->Bind(wxEVT_MENU, &FormMainMenu::OnOptionsAudio, this, ID_MMENU_OPTIONS_AUDIO);
    form->Bind(wxEVT_MENU, &FormMainMenu::OnOptionsScreen, this, ID_MMENU_OPTIONS_SCREEN);
    form->Bind(wxEVT_MENU, &FormMainMenu::OnOpenEditor, this, ID_MMENU_OPEN_EDITOR);

    LayoutMenuItems(wxSize(640, 480));

    form->Bind(wxEVT_CLOSE_WINDOW, &FormMainMenu::OnClose, this);
    form->Bind(wxEVT_SIZE, &FormMainMenu::OnSize, this);
    form->Bind(wxEVT_PAINT, &FormMainMenu::OnPaint, this);
    form->Bind(wxEVT_MOTION, &FormMainMenu::OnMouseMove, this);
    form->Bind(wxEVT_LEAVE_WINDOW, &FormMainMenu::OnMouseLeave, this);
    form->Bind(wxEVT_LEFT_UP, &FormMainMenu::OnMouseClick, this);
    form->Bind(wxEVT_KEY_DOWN, &FormMainMenu::OnKeyDown, this);

    form->CentreOnScreen();
    form->Show();
}

FormMainMenu::~FormMainMenu()
{
    if (form)
    {
        // Unbind all handlers so no events fire on the dead object
        form->Unbind(wxEVT_CLOSE_WINDOW, &FormMainMenu::OnClose, this);
        form->Unbind(wxEVT_SIZE, &FormMainMenu::OnSize, this);
        form->Unbind(wxEVT_PAINT, &FormMainMenu::OnPaint, this);
        form->Unbind(wxEVT_MOTION, &FormMainMenu::OnMouseMove, this);
        form->Unbind(wxEVT_LEAVE_WINDOW, &FormMainMenu::OnMouseLeave, this);
        form->Unbind(wxEVT_LEFT_UP, &FormMainMenu::OnMouseClick, this);
        form->Unbind(wxEVT_KEY_DOWN, &FormMainMenu::OnKeyDown, this);
        form->Unbind(wxEVT_MENU, &FormMainMenu::OnOptionsAudio,  this, ID_MMENU_OPTIONS_AUDIO);
        form->Unbind(wxEVT_MENU, &FormMainMenu::OnOptionsScreen, this, ID_MMENU_OPTIONS_SCREEN);
        form->Unbind(wxEVT_MENU, &FormMainMenu::OnOpenEditor, this, ID_MMENU_OPEN_EDITOR);
        form->Destroy();
        form = nullptr;
    }
}

bool FormMainMenu::LoadBackground()
{
    m_background = wxBitmap();
    m_bg_size = wxSize(640, 480);
    m_pal256_ok = false;

    if (!m_spelldata)
        return false;

    std::vector<unsigned char> lzBytes;
    std::vector<unsigned char> palBytes;

    // Try loading from common_fs archive first (data is already decompressed by DELZ_ALL)
    FSarchive* cfs = m_spelldata->GetCommonFS();
    bool got_img = LoadBytesFromFS(cfs, "MAINMENU.LZ", lzBytes);
    if (!got_img)
        got_img = LoadBytesFromFS(cfs, "MAINMENU.BIN", lzBytes);
    bool got_pal = LoadBytesFromFS(cfs, "MAINMENU.PAL", palBytes);

    // Fallback: search on disk
    if (!got_img || !got_pal)
    {
        namespace fs = std::filesystem;
        fs::path root = fs::path(m_spelldata->spell_data_root);

        if (!got_img)
        {
            fs::path lzPath = FindMenuFile(root, "MAINMENU.LZ");
            fs::path rawPath = FindMenuFile(root, "MAINMENU.BIN");
            if (!lzPath.empty())
                got_img = LoadFileBytes(lzPath, lzBytes);
            else if (!rawPath.empty())
                got_img = LoadFileBytes(rawPath, lzBytes);
        }
        if (!got_pal)
        {
            fs::path palPath = FindMenuFile(root, "MAINMENU.PAL");
            if (!palPath.empty())
                got_pal = LoadFileBytes(palPath, palBytes);
        }
    }

    if (!got_img || !got_pal)
        return false;

    std::array<unsigned char, 256 * 3> pal256;
    if (!ExpandPaletteTo256(palBytes, pal256))
        return false;

    // store palette for other layers
    m_pal256 = pal256;
    m_pal256_ok = true;

    std::vector<unsigned char> pixels;
    const int width = 640, height = 480;

    if (!DecodeMainMenuPixels(lzBytes, pixels, width, height))
    {
        LZWexpand delz(1024 * 1024);
        std::vector<uint8_t> decoded = delz.Decode((uint8_t*)lzBytes.data(), (uint8_t*)lzBytes.data() + lzBytes.size());
        std::vector<unsigned char> decoded_uc(decoded.begin(), decoded.end());
        if (!DecodeMainMenuPixels(decoded_uc, pixels, width, height))
            return false;
    }

    DecodedIndexed d;
    d.w = width; d.h = height; d.pixels = std::move(pixels);
    m_background = MakeBitmapFromIndexed(d, m_pal256, false);

    if (m_background.IsOk())
    {
        m_bg_size = m_background.GetSize();
        return true;
    }
    return false;
}

bool FormMainMenu::LoadPanel()
{
    m_panel = wxBitmap();
    m_panel_size = wxSize(0, 0);

    if (!m_spelldata || !m_pal256_ok)
        return false;

    std::vector<unsigned char> bytes;

    // Prefer COMMON.FS itself. SpellData opens it with DELZ_ALL, therefore
    // *.LZ members returned here are already decoded pixel/index buffers.
    FSarchive* cfs = m_spelldata->GetCommonFS();
    bool got = false;
    const char* archiveNames[] = {
        "MAINM_BG.LZ", "MAINM_BG.BIN",
        "MAINMBG.LZ",  "MAINMBG.BIN",
        "MAINM-BG.LZ", "MAINM-BG.BIN"
    };
    for (const char* name : archiveNames)
    {
        if (LoadBytesFromFS(cfs, name, bytes))
        {
            got = true;
            break;
        }
    }

    // Fallback to loose/cache data. Important: temp\COMMON contains the
    // *decoded* members written by SpellData::DumpToFolder(), but preserves the
    // original .LZ filename. Do not blindly run LZW over such a file again.
    if (!got)
    {
        namespace fs = std::filesystem;
        const fs::path root = fs::path(m_spelldata->spell_data_root);
        const char* looseNames[] = {
            "MAINM_BG.LZ", "MAINM_BG.BIN",
            "MAINMBG.LZ",  "MAINMBG.BIN",
            "MAINM-BG.LZ", "MAINM-BG.BIN"
        };

        fs::path found;
        for (const char* name : looseNames)
        {
            found = FindMenuFile(root, name);
            if (!found.empty())
                break;
        }

        if (!found.empty() && LoadFileBytes(found, bytes))
        {
            got = true;

            // MAINM_BG in the known releases expands to 255x272 = 69360
            // indexed pixels. If the loose .LZ is not already that decoded
            // payload, try LZW once.
            const bool looksDecoded = (bytes.size() == (size_t)255 * 272) ||
                                      (bytes.size() == (size_t)255 * 237);
            if (!looksDecoded && to_lower(found.extension().string()) == ".lz")
            {
                LZWexpand delz(512 * 1024);
                std::vector<uint8_t> decoded = delz.Decode(
                    (uint8_t*)bytes.data(), (uint8_t*)bytes.data() + bytes.size());
                if (!decoded.empty())
                    bytes.assign(decoded.begin(), decoded.end());
                else
                    got = false;
            }
        }
    }

    if (!got || bytes.empty())
        return false;

    DecodedIndexed d;
    if (!DecodeIndexedMaybeHeader(bytes, d))
    {
        // Known Spellcross menu-panel layouts. The common/original variant is
        // 255x272; keep the older 255x237 fallback for other distributions.
        if (bytes.size() == (size_t)255 * 272)
        {
            d.w = 255; d.h = 272; d.pixels = bytes;
        }
        else if (bytes.size() == (size_t)255 * 237)
        {
            d.w = 255; d.h = 237; d.pixels = bytes;
        }
        else
        {
            if (!GuessDimsFromSize(bytes, d, {255, 272, 237, 510, 640, 512, 480, 400, 360, 320}))
                return false;
        }
    }

    m_panel = MakeBitmapFromIndexed(d, m_pal256, true);
    if (!m_panel.IsOk())
        return false;

    m_panel_size = m_panel.GetSize();

    // Position: shifted down and left from center
    const int bgw = m_bg_size.x > 0 ? m_bg_size.x : 640;
    const int bgh = m_bg_size.y > 0 ? m_bg_size.y : 480;

    int x = (bgw - m_panel_size.x) / 2 - 20;
    int y = (bgh - m_panel_size.y) / 2 + 79;

    // The English MAINMENU.LZ has an actual 255x272 black cutout at
    // logical (172,165). The earlier arbitrary +79 offset placed the
    // panel at Y=183, leaving an 18-pixel black stripe above the buttons.
    // Position the matching original artwork exactly over its opening.
    // Leave the six-slot Czech variant's established layout unchanged.
    if (m_bg_size == wxSize(640, 480) &&
        m_panel_size == wxSize(255, 272))
    {
        x = 172;
        y = 165;
    }

    x = std::max(0, x);
    y = std::max(0, y);

    m_panel_pos = wxPoint(x, y);
    return true;
}

void FormMainMenu::LoadNativeHover()
{
    if (!m_spelldata || !m_pal256_ok || !m_panel.IsOk())
        return;

    // The retail game stores each highlighted main-menu row as a 255x62
    // transparent indexed sprite. Never replace it with synthetic rectangles.
    constexpr int spriteW = 255;
    constexpr int spriteH = 62;
    constexpr size_t expected = static_cast<size_t>(spriteW) * spriteH;
    FSarchive* common = m_spelldata->GetCommonFS();

    for (size_t i = 0; i < m_native_hover.size(); ++i)
    {
        const std::string name = "MAINM_" + std::to_string(i) + ".LZ";
        std::vector<unsigned char> bytes;
        bool loaded = LoadBytesFromFS(common, name.c_str(), bytes);

        if (!loaded)
        {
            const auto loose = FindMenuFile(
                std::filesystem::path(m_spelldata->spell_data_root), name);
            if (!loose.empty())
                loaded = LoadFileBytes(loose, bytes);
        }
        if (!loaded)
            continue;

        // COMMON.FS with DELZ_ALL delivers decoded pixels already. Loose .LZ
        // copies may still be packed; tolerate a few trailing decoder bytes.
        if (bytes.size() < expected || bytes.size() > expected + 64)
        {
            LZWexpand lzw(32 * 1024);
            const auto decoded = lzw.Decode(bytes.data(), bytes.data() + bytes.size());
            if (decoded.size() < expected || decoded.size() > expected + 64)
                continue;
            bytes.assign(decoded.begin(), decoded.end());
        }

        DecodedIndexed d;
        d.w = spriteW;
        d.h = spriteH;
        d.pixels.assign(bytes.begin(), bytes.begin() + expected);
        m_native_hover[i] = MakeBitmapFromIndexed(d, m_pal256, true);
    }
}

bool FormMainMenu::LoadTutorialPanel()
{
    if (m_tutorial_panel.IsOk())
        return true;
    if (!m_spelldata || !m_pal256_ok)
        return false;

    // Retail English COMMON.FS: TUTMNU.LZ decodes to a 250 x 214 indexed
    // graphic with BASIC / FIRST CONTACT / MISSION 1 / MISSION 2 buttons.
    std::vector<unsigned char> pixels;
    bool found = LoadBytesFromFS(m_spelldata->GetCommonFS(), "TUTMNU.LZ", pixels);
    if (!found)
    {
        const auto source = FindMenuFile(std::filesystem::path(m_spelldata->spell_data_root),
                                         "TUTMNU.LZ");
        if (!source.empty())
            found = LoadFileBytes(source, pixels);
    }
    if (!found)
        return false;

    // Spellcross retail TUTMNU.LZ contains 250x214 pixels plus a few
    // trailing decoder bytes. Accept the original payload without trying to
    // decompress an already-decoded buffer a second time.
    constexpr size_t expected = 250u * 214u;
    if (pixels.size() < expected || pixels.size() > expected + 64)
    {
        LZWexpand lzw(128 * 1024);
        const auto decompressed = lzw.Decode(pixels.data(), pixels.data() + pixels.size());
        if (decompressed.size() < expected || decompressed.size() > expected + 64)
            return false;
        pixels.assign(decompressed.begin(), decompressed.end());
    }
    pixels.resize(expected);

    DecodedIndexed image;
    image.w = 250;
    image.h = 214;
    image.pixels = std::move(pixels);
    m_tutorial_panel = MakeBitmapFromIndexed(image, m_pal256, true);
    m_tutorial_panel_pos = wxPoint((640 - 250) / 2, (480 - 214) / 2);
    if (m_tutorial_panel.IsOk())
        LoadTutorialHover();
    return m_tutorial_panel.IsOk();
}

void FormMainMenu::LoadTutorialHover()
{
    if (!m_spelldata || !m_pal256_ok)
        return;

    // Native retail assets: T01.LZ / T02.LZ / T03.LZ / T04.LZ.
    // C and CH variants contain completion checkmarks; do not use those as
    // hover sprites or invent animation frames.
    constexpr int spriteW = 200;
    constexpr int spriteH = 31;
    constexpr size_t expected = static_cast<size_t>(spriteW) * spriteH;
    FSarchive* common = m_spelldata->GetCommonFS();

    for (size_t i = 0; i < m_native_tutorial_hover.size(); ++i)
    {
        const std::string name = "T0" + std::to_string(i + 1) + ".LZ";
        std::vector<unsigned char> bytes;
        bool loaded = LoadBytesFromFS(common, name.c_str(), bytes);
        if (!loaded)
        {
            const auto loose = FindMenuFile(
                std::filesystem::path(m_spelldata->spell_data_root), name);
            if (!loose.empty())
                loaded = LoadFileBytes(loose, bytes);
        }
        if (!loaded)
            continue;

        if (bytes.size() < expected || bytes.size() > expected + 64)
        {
            LZWexpand lzw(32 * 1024);
            const auto decoded = lzw.Decode(bytes.data(), bytes.data() + bytes.size());
            if (decoded.size() < expected || decoded.size() > expected + 64)
                continue;
            bytes.assign(decoded.begin(), decoded.end());
        }

        DecodedIndexed d;
        d.w = spriteW;
        d.h = spriteH;
        d.pixels.assign(bytes.begin(), bytes.begin() + expected);
        m_native_tutorial_hover[i] = MakeBitmapFromIndexed(d, m_pal256, true);
    }
}

void FormMainMenu::BuildTutorialItems()
{
    m_items.clear();
    m_items.push_back({ "BASIC", FormMainMenuAction::TutorialBasic, wxRect() });
    m_items.push_back({ "FIRST CONTACT", FormMainMenuAction::TutorialFirstContact, wxRect() });
    m_items.push_back({ "MISSION 1", FormMainMenuAction::TutorialMission1, wxRect() });
    m_items.push_back({ "MISSION 2", FormMainMenuAction::TutorialMission2, wxRect() });
}

void FormMainMenu::BuildMenuItems()
{
    m_items.clear();

    m_items.push_back({ "nová hra", FormMainMenuAction::NewGame, wxRect() });
    m_items.push_back({ "pokračovat", FormMainMenuAction::Continue, wxRect() });
    m_items.push_back({ "načíst hru", FormMainMenuAction::LoadGame, wxRect() });
    // Respect the original editions: the English retail menu has Tutorial,
    // while the Czech retail artwork has exactly six entries and NO tutorial.
    if (m_panel_size.y >= 255)
        m_items.push_back({ "tutorial", FormMainMenuAction::Tutorial, wxRect() });
    m_items.push_back({ "credits", FormMainMenuAction::Credits, wxRect() });
    m_items.push_back({ "intro", FormMainMenuAction::Intro, wxRect() });
    m_items.push_back({ "konec", FormMainMenuAction::Exit, wxRect() });
}

void FormMainMenu::LayoutMenuItems(const wxSize& clientSize)
{
    if (m_items.empty())
        return;

    if (m_tutorial_visible && m_tutorial_panel.IsOk())
    {
        const int centers[4] = { 84, 116, 148, 179 };
        for (size_t i = 0; i < m_items.size() && i < 4; ++i)
        {
            m_items[i].rect = wxRect(m_tutorial_panel_pos.x + 60,
                                     m_tutorial_panel_pos.y + centers[i] - 14,
                                     160, 28);
        }
        return;
    }

    if (m_panel.IsOk() && m_panel_size.x > 0 && m_panel_size.y > 0)
    {
        // Slots inside MAINM_BG (255x237). Offsets relative to panel top-left.
        const int panel_x = m_panel_pos.x;
        const int panel_y = m_panel_pos.y;

        const int btn_w = 210;
        const int btn_h = 26;
        const int btn_x = panel_x + 45;

        const bool englishSevenSlots = m_panel_size.y >= 255;
        const int sixCenters[6] = { 30, 66, 100, 136, 170, 206 };
        const int sevenCenters[7] = { 30, 66, 100, 136, 170, 206, 242 };
        const size_t nativeCount = englishSevenSlots ? 7 : 6;
        for (size_t i = 0; i < m_items.size() && i < nativeCount; ++i)
        {
            const int cy = panel_y + (englishSevenSlots ? sevenCenters[i] : sixCenters[i]);
            m_items[i].rect = wxRect(btn_x, cy - btn_h / 2, btn_w, btn_h);
        }
        return;
    }

    // Fallback: center list in window
    const int w = clientSize.x;
    const int h = clientSize.y;

    const int btn_w = 235;
    const int btn_h = 30;
    const int gap = 10;

    const int x = (w - btn_w) / 2 + 4;

    const int area_top = 175;
    const int area_bottom = h - 25;
    const int area_h = std::max(1, area_bottom - area_top);

    const int total_h = (int)m_items.size() * btn_h + ((int)m_items.size() - 1) * gap;
    int y = area_top + std::max(0, (area_h - total_h) / 2);

    for (auto& it : m_items)
    {
        it.rect = wxRect(x, y, btn_w, btn_h);
        y += btn_h + gap;
    }
}

void FormMainMenu::TriggerAction(int index)
{
    if (index < 0 || index >= (int)m_items.size())
        return;

    const FormMainMenuAction action = m_items[static_cast<size_t>(index)].action;
    if (action == FormMainMenuAction::Tutorial && LoadTutorialPanel())
    {
        m_tutorial_visible = true;
        m_hover_index = -1;
        BuildTutorialItems();
        LayoutMenuItems(wxSize(640, 480));
        if (form) form->Refresh(false);
        return;
    }
    if (m_action_cb)
        m_action_cb(action);
}

void FormMainMenu::OnClose(wxCloseEvent& ev)
{
    if (ev.CanVeto())
        ev.Veto();
    if (form)
        form->DeletePendingEvents();

    // The menu is now an independent top-level frame. Forward its close event
    // to the original canvas so MainFrame can decide whether to resume a paused
    // tactical mission or terminate from the startup menu.
    if (m_event_parent)
    {
        wxCloseEvent forwarded(wxEVT_CLOSE_WINDOW, ev.GetId());
        wxPostEvent(m_event_parent, forwarded);
    }
}

void FormMainMenu::OnSize(wxSizeEvent& event)
{
    event.Skip();
    if (form)
        form->Refresh(false);
}

void FormMainMenu::OnPaint(wxPaintEvent& event)
{
    wxBufferedPaintDC dc(form);
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();

    const wxSize client = form->GetClientSize();
    if (client.x <= 0 || client.y <= 0)
        return;

    // Same crisp integer-scaling rule used by the reconstructed strategic UI.
    // The logical menu surface is always the original 640x480.
    constexpr int logicalW = 640;
    constexpr int logicalH = 480;
    double scale = std::min(
        static_cast<double>(client.x) / logicalW,
        static_cast<double>(client.y) / logicalH);
    if (scale >= 1.0)
        scale = std::max(1.0, std::floor(scale));

    const int drawW = std::max(1, static_cast<int>(std::lround(logicalW * scale)));
    const int drawH = std::max(1, static_cast<int>(std::lround(logicalH * scale)));
    const int drawX = (client.x - drawW) / 2;
    const int drawY = (client.y - drawH) / 2;
    m_draw_rect = wxRect(drawX, drawY, drawW, drawH);
    LayoutMenuItems(wxSize(logicalW, logicalH));

    // Expensive scaling and alpha composition happen ONLY when the menu size
    // or tutorial mode changes, not during mouse movement across buttons.
    if (!m_menu_cache.IsOk() || m_menu_cache.GetSize() != wxSize(drawW, drawH) ||
        m_menu_cache_tutorial != m_tutorial_visible)
    {
        wxBitmap composed(drawW, drawH);
        wxMemoryDC layer;
        layer.SelectObject(composed);
        layer.SetBackground(*wxBLACK_BRUSH);
        layer.Clear();

        if (m_background.IsOk())
        {
            wxBitmap bg = m_background;
            if (drawW != logicalW || drawH != logicalH)
                bg = wxBitmap(bg.ConvertToImage().Scale(drawW, drawH, wxIMAGE_QUALITY_NEAREST));
            if (bg.IsOk())
                layer.DrawBitmap(bg, 0, 0, false);
        }

        // Preserve the original main menu behind the smaller tutorial panel.
        // TUTMNU has transparent pixels and must not replace MAINM_BG.
        const auto drawOverlay = [&](const wxBitmap& overlay, const wxPoint& pos)
        {
            if (!overlay.IsOk())
                return;
            const int x = static_cast<int>(std::lround(pos.x * scale));
            const int y = static_cast<int>(std::lround(pos.y * scale));
            const int w = std::max(1, static_cast<int>(std::lround(overlay.GetWidth() * scale)));
            const int h = std::max(1, static_cast<int>(std::lround(overlay.GetHeight() * scale)));
            wxBitmap picture = overlay;
            if (w != overlay.GetWidth() || h != overlay.GetHeight())
                picture = wxBitmap(overlay.ConvertToImage().Scale(w, h, wxIMAGE_QUALITY_NEAREST));
            if (picture.IsOk())
                layer.DrawBitmap(picture, x, y, true);
        };
        drawOverlay(m_panel, m_panel_pos);
        if (m_tutorial_visible)
            drawOverlay(m_tutorial_panel, m_tutorial_panel_pos);
        layer.SelectObject(wxNullBitmap);
        m_menu_cache = composed;
        m_menu_cache_tutorial = m_tutorial_visible;

        for (size_t i = 0; i < m_native_hover.size(); ++i)
        {
            m_scaled_hover[i] = wxBitmap();
            if (!m_native_hover[i].IsOk())
                continue;
            const int w = std::max(1, static_cast<int>(std::lround(255 * scale)));
            const int h = std::max(1, static_cast<int>(std::lround(62 * scale)));
            m_scaled_hover[i] = (w == 255 && h == 62)
                ? m_native_hover[i]
                : wxBitmap(m_native_hover[i].ConvertToImage().Scale(w, h, wxIMAGE_QUALITY_NEAREST));
        }
        for (size_t i = 0; i < m_native_tutorial_hover.size(); ++i)
        {
            m_scaled_tutorial_hover[i] = wxBitmap();
            if (!m_native_tutorial_hover[i].IsOk())
                continue;
            const int w = std::max(1, static_cast<int>(std::lround(200 * scale)));
            const int h = std::max(1, static_cast<int>(std::lround(31 * scale)));
            m_scaled_tutorial_hover[i] = (w == 200 && h == 31)
                ? m_native_tutorial_hover[i]
                : wxBitmap(m_native_tutorial_hover[i].ConvertToImage().Scale(w, h, wxIMAGE_QUALITY_NEAREST));
        }
    }

    if (m_menu_cache.IsOk())
        dc.DrawBitmap(m_menu_cache, drawX, drawY, false);

    // Pixel-perfect row offsets determined from MAINM_BG and MAINM_0..6.
    // These are source-image positions, NOT synthetic highlight rectangles.
    static constexpr int rowY[7] = { 0, 35, 70, 105, 140, 175, 210 };
    if (!m_tutorial_visible && m_panel.IsOk() &&
        m_hover_index >= 0 && m_hover_index < (int)m_items.size() &&
        m_hover_index < (int)m_scaled_hover.size() &&
        m_scaled_hover[(size_t)m_hover_index].IsOk())
    {
        const int x = drawX + static_cast<int>(std::lround(m_panel_pos.x * scale));
        const int y = drawY + static_cast<int>(std::lround(
            (m_panel_pos.y + rowY[m_hover_index]) * scale));
        dc.DrawBitmap(m_scaled_hover[(size_t)m_hover_index], x, y, true);
    }
    if (m_tutorial_visible && m_tutorial_panel.IsOk() &&
        m_hover_index >= 0 && m_hover_index < 4 &&
        m_scaled_tutorial_hover[(size_t)m_hover_index].IsOk())
    {
        // Verified against TUTMNU pixels: x=19, y=69,100,131,162.
        const int x = drawX + static_cast<int>(std::lround(
            (m_tutorial_panel_pos.x + 19) * scale));
        const int y = drawY + static_cast<int>(std::lround(
            (m_tutorial_panel_pos.y + 69 + 31 * m_hover_index) * scale));
        dc.DrawBitmap(m_scaled_tutorial_hover[(size_t)m_hover_index], x, y, true);
    }
}

void FormMainMenu::PlayNativeMenuSound()
{
    // SAMPLES.DEF specifies 1-PIP1 for both menu hover and activation.
    if (!m_spelldata || !m_spelldata->sounds ||
        !m_spelldata->sounds->GetSample("1-PIP1"))
        return;
    if (auto* sound = m_spelldata->sounds->GetSound("1-PIP1"))
        sound->PlayAsync();
}

void FormMainMenu::OnMouseMove(wxMouseEvent& event)
{
    const wxPoint p = event.GetPosition();
    int hit = -1;

    if (m_draw_rect.width > 0 && m_draw_rect.height > 0 && m_draw_rect.Contains(p))
    {
        const int lx = static_cast<int>(
            (static_cast<long long>(p.x - m_draw_rect.x) * 640) / m_draw_rect.width);
        const int ly = static_cast<int>(
            (static_cast<long long>(p.y - m_draw_rect.y) * 480) / m_draw_rect.height);
        const wxPoint logical(lx, ly);

        for (size_t i = 0; i < m_items.size(); ++i)
        {
            if (m_items[i].rect.Contains(logical))
            {
                hit = (int)i;
                break;
            }
        }
    }

    if (hit != m_hover_index)
    {
        m_hover_index = hit;
        if (hit >= 0)
            PlayNativeMenuSound();
        form->Refresh(false);
    }

    event.Skip();
}

void FormMainMenu::OnMouseLeave(wxMouseEvent& event)
{
    if (m_hover_index != -1)
    {
        m_hover_index = -1;
        form->Refresh(false);
    }
    event.Skip();
}

void FormMainMenu::OnMouseClick(wxMouseEvent& event)
{
    if (m_hover_index >= 0)
    {
        PlayNativeMenuSound();
        TriggerAction(m_hover_index);
    }
    event.Skip();
}

void FormMainMenu::OnKeyDown(wxKeyEvent& event)
{
    if (event.GetKeyCode() == WXK_ESCAPE)
    {
        if (m_tutorial_visible)
        {
            m_tutorial_visible = false;
            m_hover_index = -1;
            BuildMenuItems();
            LayoutMenuItems(wxSize(640, 480));
            if (form) form->Refresh(false);
        }
        else if (form)
            form->Close();
        return;
    }
    // ~ key (backtick/tilde) opens console
    int kc = event.GetKeyCode();
    if (kc == '`' || kc == '~' || event.GetRawKeyCode() == 0xC0)
    {
        OnConsoleCommand();
        return;
    }
    event.Skip();
}

void FormMainMenu::OnConsoleCommand()
{
    wxTextEntryDialog dlg(form, "Enter command:", "Console", "");
    if (dlg.ShowModal() != wxID_OK)
        return;

    std::string cmd = dlg.GetValue().ToStdString();
    // trim whitespace
    while (!cmd.empty() && cmd.front() == ' ') cmd.erase(cmd.begin());
    while (!cmd.empty() && cmd.back() == ' ') cmd.pop_back();
    // to upper
    for (char& c : cmd) c = (char)std::toupper((unsigned char)c);

    if (!cmd.empty())
    {
        wxMessageBox("Unknown command: " + dlg.GetValue(), "Console", wxOK | wxICON_WARNING, form);
    }
}

void FormMainMenu::OnOptionsAudio(wxCommandEvent& ev)
{
    if (!m_spelldata || !m_spelldata->sounds || !m_spelldata->sounds->channels || !m_spelldata->midi)
        return;

    const double oldSfx = m_spelldata->sounds->channels->GetVolume();
    const double oldMusic = m_spelldata->midi->GetVolume();

    wxDialog dlg(form, wxID_ANY, "Audio", wxDefaultPosition, wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE);

    auto* sizerTop = new wxBoxSizer(wxVERTICAL);

    auto* lblMusic = new wxStaticText(&dlg, wxID_ANY, "Music volume");
    auto* sldMusic = new wxSlider(&dlg, wxID_ANY,
        (int)std::lround(oldMusic * 100.0), 0, 100,
        wxDefaultPosition, wxSize(300, -1),
        wxSL_HORIZONTAL | wxSL_VALUE_LABEL);

    auto* lblSfx = new wxStaticText(&dlg, wxID_ANY, "Sound volume");
    auto* sldSfx = new wxSlider(&dlg, wxID_ANY,
        (int)std::lround(oldSfx * 100.0), 0, 100,
        wxDefaultPosition, wxSize(300, -1),
        wxSL_HORIZONTAL | wxSL_VALUE_LABEL);

    sizerTop->Add(lblMusic, 0, wxALL, 8);
    sizerTop->Add(sldMusic, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
    sizerTop->Add(lblSfx, 0, wxALL, 8);
    sizerTop->Add(sldSfx, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

    auto* btns = dlg.CreateButtonSizer(wxOK | wxCANCEL);
    sizerTop->Add(btns, 0, wxALL | wxEXPAND, 8);
    dlg.SetSizerAndFit(sizerTop);

    auto applyAudio = [&]() {
        m_spelldata->midi->SetVolume(sldMusic->GetValue() / 100.0);
        m_spelldata->sounds->channels->SetVolume(sldSfx->GetValue() / 100.0);
    };

    sldMusic->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { applyAudio(); });
    sldSfx->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { applyAudio(); });

    if (dlg.ShowModal() == wxID_OK)
        applyAudio();
    else
    {
        m_spelldata->midi->SetVolume(oldMusic);
        m_spelldata->sounds->channels->SetVolume(oldSfx);
    }
}

void FormMainMenu::OnOptionsScreen(wxCommandEvent& ev)
{
    const double oldGamma = m_spell_map ? m_spell_map->GetGamma() : 1.3;

    wxDialog dlg(form, wxID_ANY, "Screen", wxDefaultPosition, wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE);

    auto* sizerTop = new wxBoxSizer(wxVERTICAL);

    auto* lblBrightness = new wxStaticText(&dlg, wxID_ANY, "Brightness");
    auto* sldBrightness = new wxSlider(&dlg, wxID_ANY,
        (int)std::lround(oldGamma * 1000.0), 500, 2000,
        wxDefaultPosition, wxSize(300, -1),
        wxSL_HORIZONTAL | wxSL_VALUE_LABEL);

    sizerTop->Add(lblBrightness, 0, wxALL, 8);
    sizerTop->Add(sldBrightness, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

    auto* btns = dlg.CreateButtonSizer(wxOK | wxCANCEL);
    sizerTop->Add(btns, 0, wxALL | wxEXPAND, 8);
    dlg.SetSizerAndFit(sizerTop);

    auto applyScreen = [&]() {
        if (m_spell_map)
            m_spell_map->SetGamma(sldBrightness->GetValue() * 0.001);
    };

    sldBrightness->Bind(wxEVT_SLIDER, [&](wxCommandEvent&) { applyScreen(); });

    if (dlg.ShowModal() == wxID_OK)
        applyScreen();
    else
    {
        if (m_spell_map)
            m_spell_map->SetGamma(oldGamma);
    }
}

void FormMainMenu::OnOpenEditor(wxCommandEvent&)
{
    // Keep the main menu visible; this is a separate standalone application.
    if (m_action_cb)
        m_action_cb(FormMainMenuAction::OpenEditor);
}
