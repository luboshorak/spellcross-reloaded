#pragma once

#include <wx/panel.h>
#include <wx/bitmap.h>
#include <wx/event.h>
#include <wx/gdicmn.h>

#include <array>

#include <functional>
#include <vector>

#include "spellcross.h"
#include "map.h"

enum class FormMainMenuAction
{
    NewGame,
    Continue,
    LoadGame,
    Tutorial,
    TutorialBasic,
    TutorialFirstContact,
    TutorialMission1,
    TutorialMission2,
    Credits,
    Intro,
    Exit,
    OpenEditor
};

class FormMainMenu
{
public:
    FormMainMenu(wxPanel* parent,
        wxWindowID win_id,
        SpellMap* spell_map,
        SpellData* spell_data,
        std::function<void(FormMainMenuAction)> action_cb);
    ~FormMainMenu();

private:
    struct MenuItem
    {
        wxString label;
        FormMainMenuAction action;
        wxRect rect;
    };

    SpellMap* m_spell_map;
    SpellData* m_spelldata;
    wxFrame* form;
    wxWindow* m_event_parent = nullptr;
    wxRect m_draw_rect;
    wxBitmap m_background;
    wxSize m_bg_size;

    wxBitmap m_panel;
    wxPoint m_panel_pos;
    // Native MAINM_0.LZ..MAINM_6.LZ (255x62) from COMMON.FS.
    std::array<wxBitmap, 7> m_native_hover;
    std::array<wxBitmap, 7> m_scaled_hover;
    // Original T01..T04 hover sprites, 200x31 pixels each (COMMON.FS).
    std::array<wxBitmap, 4> m_native_tutorial_hover;
    std::array<wxBitmap, 4> m_scaled_tutorial_hover;
    // Full, already scaled background + panel. Rebuilt only on resize or tutorial toggle.
    wxBitmap m_menu_cache;
    bool m_menu_cache_tutorial = false;
    wxBitmap m_tutorial_panel;
    wxPoint m_tutorial_panel_pos;
    bool m_tutorial_visible = false;
    wxSize m_panel_size;
    std::array<unsigned char, 256 * 3> m_pal256;
    bool m_pal256_ok;
    std::vector<MenuItem> m_items;
    int m_hover_index;
    std::function<void(FormMainMenuAction)> m_action_cb;

    bool LoadBackground();
    bool LoadPanel();
    void LoadNativeHover();
    bool LoadTutorialPanel();
    void LoadTutorialHover();
    void PlayNativeMenuSound();
    void BuildMenuItems();
    void BuildTutorialItems();
    void LayoutMenuItems(const wxSize& clientSize);
    void TriggerAction(int index);

    void OnClose(wxCloseEvent& ev);
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    void OnMouseClick(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnConsoleCommand();
    void OnOptionsAudio(wxCommandEvent& ev);
    void OnOptionsScreen(wxCommandEvent& ev);
    void OnOpenEditor(wxCommandEvent& ev);

    static constexpr int ID_MMENU_OPTIONS_AUDIO  = 9901;
    static constexpr int ID_MMENU_OPTIONS_SCREEN = 9902;
    static constexpr int ID_MMENU_OPEN_EDITOR = 9903;
};
