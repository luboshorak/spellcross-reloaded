#include "form_unit_options.h"
#include "map.h"

#include <algorithm>
#include <wx/rawbmp.h>

FormUnitOpts::FormUnitOpts(wxPanel *parent, wxWindowID win_id, wxPoint position,
    SpellData* spelldata, int flags, std::function<void(int)> selection_cb)
{
    m_spelldata = spelldata;
    m_sel_cb = selection_cb;

    // The original Spellcross uses the I_TAB background and I_* glyphs for
    // resolving ambiguous clicks. Retain their order and graphics.
    vector<SpellGraphicItem*> glyphs;
    if (flags & SpellMap::UNIT_OPT_UPPER)
    {
        glyphs.push_back(m_spelldata->gres.wm_sel_upper);
        m_labels.push_back("Upper");
        m_actions.push_back(SpellMap::UNIT_OPT_UPPER);
    }
    if (flags & SpellMap::UNIT_OPT_LOWER)
    {
        glyphs.push_back(m_spelldata->gres.wm_sel_lower);
        m_labels.push_back("Lower");
        m_actions.push_back(SpellMap::UNIT_OPT_LOWER);
    }
    if (flags & SpellMap::UNIT_OPT_SELECT)
    {
        glyphs.push_back(m_spelldata->gres.wm_sel_select);
        m_labels.push_back("Select");
        m_actions.push_back(SpellMap::UNIT_OPT_SELECT);
    }
    if (flags & SpellMap::UNIT_OPT_ATTACK)
    {
        glyphs.push_back(m_spelldata->gres.wm_sel_attack);
        m_labels.push_back("Attack");
        m_actions.push_back(SpellMap::UNIT_OPT_ATTACK);
    }
    if (flags & SpellMap::UNIT_OPT_MOVE)
    {
        glyphs.push_back(m_spelldata->gres.wm_sel_move);
        m_labels.push_back("Move");
        m_actions.push_back(SpellMap::UNIT_OPT_MOVE);
    }

    auto* tab = m_spelldata->gres.wm_sel_tab;
    // The ordinary DOS selector is a two-choice I_TAB. In rare modded maps
    // more than two valid actions may coincide, so stack original tabs rather
    // than silently dropping an action or using an invalid array index.
    const int count = static_cast<int>(glyphs.size());
    const int rows = (count + 1) / 2;
    m_tab_height = tab->y_size;
    const int x_size = tab->x_size;
    const int y_size = rows * m_tab_height;
    m_back_buff.assign(x_size * y_size, 0x00);
    for (int row = 0; row < rows; ++row)
        tab->Render(m_back_buff.data(), m_back_buff.data() + m_back_buff.size(),
                    x_size, 0, row * m_tab_height);

    wxSize size(x_size, y_size);
    // The popup can be taller than one native tab. Keep all choices inside
    // the canvas, even when the ambiguous square is near an edge.
    const wxSize parent_size = parent->GetClientSize();
    position.x = (std::max)(0, (std::min)(position.x, parent_size.GetWidth() - size.GetWidth()));
    position.y = (std::max)(0, (std::min)(position.y, parent_size.GetHeight() - size.GetHeight()));

    form = new wxWindow(parent, win_id, position, size, wxBORDER_NONE);
    form->SetDoubleBuffered(true);
    m_hovers.assign(count, false);

    m_bmp_btn.reserve(count);
    m_bmp_btn_hover.reserve(count);
    buttons.reserve(count);
    for (int k = 0; k < count; ++k)
    {
        m_bmp_btn.push_back(glyphs[k]->Render());
        m_bmp_btn_hover.push_back(glyphs[k]->Render(false, true));
        auto* button = new wxPanel(form, ID_BTN + k,
            wxPoint(6 + (k % 2) * 28, 4 + (k / 2) * m_tab_height),
            wxSize(m_bmp_btn[k]->GetWidth(), m_bmp_btn[k]->GetHeight()));
        buttons.push_back(button);
        button->SetBackgroundStyle(wxBG_STYLE_PAINT);
        button->SetClientData(&m_labels[k]);
        button->Bind(wxEVT_PAINT, &FormUnitOpts::OnPaintButton, this, ID_BTN + k);
        button->Bind(wxEVT_LEAVE_WINDOW, &FormUnitOpts::OnButtonMouseHover, this, ID_BTN + k);
        button->Bind(wxEVT_ENTER_WINDOW, &FormUnitOpts::OnButtonMouseHover, this, ID_BTN + k);
        button->Bind(wxEVT_LEFT_DOWN, &FormUnitOpts::OnButtonClick, this, ID_BTN + k);
    }

    form->Bind(wxEVT_CLOSE_WINDOW, &FormUnitOpts::OnClose, this);
    form->Bind(wxEVT_PAINT, &FormUnitOpts::OnPaintTab, this);
    form->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_ESCAPE)
        {
            form->Close();
            return;
        }
        event.Skip();
    });
    form->SetFocus();
}

FormUnitOpts::~FormUnitOpts()
{
    for (auto* bmp : m_bmp_btn)
        delete bmp;
    for (auto* bmp : m_bmp_btn_hover)
        delete bmp;
}

void FormUnitOpts::OnClose(wxCloseEvent& ev)
{
    // ESC/cancel must not execute an arbitrary uninitialized action.
    wxQueueEvent(form->GetParent(), new wxCloseEvent(ev));
    form->Destroy();
}

void FormUnitOpts::OnPaintTab(wxPaintEvent& event)
{
    auto back = m_back_buff;
    uint8_t* buf = back.data();

    const int x_size = m_spelldata->gres.wm_sel_tab->x_size;
    const int y_size = static_cast<int>(back.size()) / x_size;
    if (m_hovered_action >= 0 && m_hovered_action < static_cast<int>(m_labels.size()))
        m_spelldata->font->Render(buf, buf + back.size(), x_size,
            3, 29 + (m_hovered_action / 2) * m_tab_height, 54, 13,
            info_label, 0xFF, 0xFE, SpellFont::FontShadow::RIGHT_DOWN);

    uint8_t (*pal)[3] = m_spelldata->gres.wm_sel_tab->pal;
    wxBitmap bmp(x_size, y_size, 24);
    wxNativePixelData pdata(bmp);
    wxNativePixelData::Iterator p(pdata);
    for (int y = 0; y < y_size; ++y)
    {
        uint8_t* scan = p.m_ptr;
        for (int x = 0; x < x_size; ++x)
        {
            *scan++ = pal[*buf][2];
            *scan++ = pal[*buf][1];
            *scan++ = pal[*buf][0];
            ++buf;
        }
        p.OffsetY(pdata, 1);
    }

    wxPaintDC pdc(form);
    pdc.DrawBitmap(bmp, wxPoint(0, 0));
}

void FormUnitOpts::OnPaintButton(wxPaintEvent& event)
{
    auto* pan = static_cast<wxPanel*>(event.GetEventObject());
    const int index = event.GetId() - ID_BTN;
    wxBitmap* bmp = m_hovers[index] ? m_bmp_btn_hover[index] : m_bmp_btn[index];
    wxPaintDC pdc(pan);
    pdc.DrawBitmap(*bmp, wxPoint(0, 0));
}

void FormUnitOpts::OnButtonMouseHover(wxMouseEvent& event)
{
    const int index = event.GetId() - ID_BTN;
    const bool enter = event.GetEventType() == wxEVT_ENTER_WINDOW;
    m_hovers[index] = enter;
    if (enter)
    {
        m_hovered_action = index;
        info_label = m_labels[index];
    }
    else if (m_hovered_action == index)
    {
        m_hovered_action = -1;
        info_label.clear();
    }
    form->Refresh();
}

void FormUnitOpts::OnButtonClick(wxMouseEvent& event)
{
    const int index = event.GetId() - ID_BTN;
    if (index < 0 || index >= static_cast<int>(m_actions.size()))
        return;
    m_action = m_actions[index];
    m_confirmed = true;
    form->Close();
}

void FormUnitOpts::ResultCallback()
{
    if (m_confirmed && m_sel_cb)
        m_sel_cb(m_action);
}
