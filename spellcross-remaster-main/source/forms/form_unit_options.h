#pragma once

#include "spellcross.h"
#include <wx/window.h>
#include <wx/panel.h>
#include <string>

class FormUnitOpts
{
private:
    SpellData* m_spelldata;    
    std::function<void(int)> m_sel_cb;
    vector<uint8_t> m_back_buff;
    vector<wxBitmap*> m_bmp_btn;
    vector<wxBitmap*> m_bmp_btn_hover;
    vector<string> m_labels;
    vector<int> m_actions;
    vector<int> m_hovers;
    int m_action = 0;
    bool m_confirmed = false;
    int m_hovered_action = -1;
    int m_tab_height = 0;

    wxWindow* form = nullptr;
    vector<wxPanel*> buttons;

    string info_label;

    void OnClose(wxCloseEvent& ev);
    void OnPaintTab(wxPaintEvent& event);
    void OnPaintButton(wxPaintEvent& event);
    void OnButtonMouseHover(wxMouseEvent& event);
    void OnButtonClick(wxMouseEvent& event);

    static constexpr int ID_BTN = 1000;

public:
    FormUnitOpts(wxPanel* parent,wxWindowID win_id,wxPoint position,SpellData* spelldata,int flags,std::function<void(int)> selection_cb);
    ~FormUnitOpts();
    void ResultCallback();        

};

