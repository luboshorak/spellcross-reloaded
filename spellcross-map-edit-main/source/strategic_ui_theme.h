#pragma once

#include "game_menu_bar.h"
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#ifdef __WXMSW__
#include <commctrl.h>
#endif

// Presentation shared by the wx strategic pages. Native button identity,
// notifications, keyboard activation and enabled state remain intact.
namespace spellcross_strategic_ui
{
    inline wxColour Heading() { return wxColour(227, 233, 24); }
    inline wxColour Inactive() { return wxColour(115, 120, 124); }
    inline wxColour Border() { return wxColour(121, 128, 115); }

    inline void PaintGrid(wxDC& dc, wxWindow* window, bool highlighted = false)
    {
        dc.SetBackground(wxBrush(highlighted ? spellcross_menu::Highlight() : spellcross_menu::Background()));
        dc.Clear();
        dc.SetPen(wxPen(wxColour(19, 43, 14)));
        const int step = window->FromDIP(12);
        const auto size = window->GetClientSize();
        for (int x = window->FromDIP(8); x < size.x; x += step)
            dc.DrawLine(x, 1, x, size.y - 1);
        for (int y = window->FromDIP(8); y < size.y; y += step)
            dc.DrawLine(1, y, size.x - 1, y);
    }

    class Panel : public wxPanel
    {
    public:
        Panel(wxWindow* parent, wxWindowID id = wxID_ANY,
              const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
              long style = wxTAB_TRAVERSAL)
            : wxPanel(parent, id, pos, size, style)
        {
            SetBackgroundColour(spellcross_menu::Background());
            SetForegroundColour(spellcross_menu::Text());
            SetBackgroundStyle(wxBG_STYLE_PAINT);
            Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
                wxAutoBufferedPaintDC dc(this);
                PaintGrid(dc, this);
            });
        }
    };

    class Button final : public wxButton
    {
        void Paint(wxDC& dc, bool pressed, bool focused)
        {
            const bool hover = GetClientRect().Contains(ScreenToClient(wxGetMousePosition()));
            const bool active = IsEnabled() && (hover || pressed || GetFont().GetWeight() == wxFONTWEIGHT_BOLD);
            PaintGrid(dc, this, active);
            dc.SetPen(wxPen(IsEnabled() ? spellcross_menu::Accent() : Inactive()));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(GetClientRect());
            if (focused)
            {
                dc.SetPen(wxPen(Heading(), 1, wxPENSTYLE_DOT));
                dc.DrawRectangle(GetClientRect().Deflate(FromDIP(3)));
            }
            dc.SetFont(GetFont());
            dc.SetTextForeground(!IsEnabled() ? Inactive() : active ? Heading() : spellcross_menu::Text());
            wxRect text = GetClientRect().Deflate(FromDIP(7), FromDIP(3));
            if (pressed) text.Offset(1, 1);
            dc.SetClippingRegion(text);
            dc.DrawLabel(wxControl::GetLabelText(GetLabel()), text, wxALIGN_CENTER);
            dc.DestroyClippingRegion();
        }

    public:
        Button(wxWindow* parent, wxWindowID id, const wxString& label,
               const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
               long style = 0)
            : wxButton(parent, id, label, pos, size, style)
        {
            SetFont(spellcross_menu::MenuFont());
            SetBackgroundColour(spellcross_menu::Background());
            SetForegroundColour(spellcross_menu::Text());
#ifdef __WXMSW__
            MakeOwnerDrawn();
#else
            Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
                wxPaintDC dc(this);
                Paint(dc, HasCapture() && wxGetMouseState().LeftIsDown(), HasFocus());
            });
#endif
            Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& event) { Refresh(); event.Skip(); });
            Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& event) { Refresh(); event.Skip(); });
            Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) { Refresh(); event.Skip(); });
            Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) { Refresh(); event.Skip(); });
        }

#ifdef __WXMSW__
        bool MSWOnDraw(WXDRAWITEMSTRUCT* item) override
        {
            const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(item);
            const auto size = GetClientSize();
            if (size.x <= 0 || size.y <= 0) return true;
            wxBitmap image(size.x, size.y);
            wxMemoryDC dc(image);
            Paint(dc, (draw->itemState & ODS_SELECTED) != 0, (draw->itemState & ODS_FOCUS) != 0);
            BitBlt(draw->hDC, 0, 0, size.x, size.y,
                reinterpret_cast<HDC>(dc.GetHDC()), 0, 0, SRCCOPY);
            return true;
        }
#endif
    };

    // The native Windows header accepts text attributes but still paints a
    // white themed background. Draw its cells without changing list behaviour.
    class List final : public wxListCtrl
    {
    public:
        using wxListCtrl::wxListCtrl;

#ifdef __WXMSW__
    protected:
        bool MSWOnNotify(int id, WXLPARAM param, WXLPARAM* result) override
        {
            const auto* notification = reinterpret_cast<NMHDR*>(param);
            const auto header = ListView_GetHeader(reinterpret_cast<HWND>(GetHandle()));
            if (notification->hwndFrom == header && notification->code == NM_CUSTOMDRAW)
            {
                const auto* draw = reinterpret_cast<NMCUSTOMDRAW*>(param);
                if (draw->dwDrawStage == CDDS_PREPAINT)
                {
                    *result = CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
                    return true;
                }
                if (draw->dwDrawStage == CDDS_ITEMPREPAINT)
                {
                    const int width = draw->rc.right - draw->rc.left;
                    const int height = draw->rc.bottom - draw->rc.top;
                    if (width > 0 && height > 0)
                    {
                        wxBitmap image(width, height);
                        wxMemoryDC dc(image);
                        dc.SetBackground(wxBrush(spellcross_menu::Highlight()));
                        dc.Clear();
                        dc.SetPen(wxPen(Border()));
                        dc.DrawLine(width - 1, 0, width - 1, height);
                        dc.DrawLine(0, height - 1, width, height - 1);
                        wxListItem column;
                        column.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_FORMAT);
                        GetColumn(static_cast<int>(draw->dwItemSpec), column);
                        dc.SetFont(GetFont());
                        dc.SetTextForeground(spellcross_menu::Accent());
                        const wxRect text(FromDIP(7), 0, std::max(1, width - FromDIP(14)), height);
                        dc.SetClippingRegion(text);
                        const int align = column.GetAlign() == wxLIST_FORMAT_RIGHT ? wxALIGN_RIGHT
                            : column.GetAlign() == wxLIST_FORMAT_CENTER ? wxALIGN_CENTER_HORIZONTAL : wxALIGN_LEFT;
                        dc.DrawLabel(column.GetText(), text, align | wxALIGN_CENTER_VERTICAL);
                        BitBlt(draw->hdc, draw->rc.left, draw->rc.top, width, height,
                            reinterpret_cast<HDC>(dc.GetHDC()), 0, 0, SRCCOPY);
                    }
                    *result = CDRF_SKIPDEFAULT;
                    return true;
                }
                if (draw->dwDrawStage == CDDS_POSTPAINT)
                {
                    RECT empty;
                    ::GetClientRect(header, &empty);
                    int last = 0;
                    for (int column = 0; column < GetColumnCount(); ++column)
                    {
                        RECT cell;
                        if (Header_GetItemRect(header, column, &cell)) last = std::max(last, static_cast<int>(cell.right));
                    }
                    empty.left = last;
                    const auto colour = spellcross_menu::Background();
                    HBRUSH brush = CreateSolidBrush(RGB(colour.Red(), colour.Green(), colour.Blue()));
                    FillRect(draw->hdc, &empty, brush);
                    DeleteObject(brush);
                    *result = CDRF_DODEFAULT;
                    return true;
                }
            }
            return wxListCtrl::MSWOnNotify(id, param, result);
        }
#endif
    };

    inline void Apply(wxWindow* window)
    {
        wxFont font = spellcross_menu::MenuFont();
        font.SetWeight(window->GetFont().GetWeight());
        window->SetFont(font);
        window->SetBackgroundColour(spellcross_menu::Background());
        window->SetForegroundColour(window->GetFont().GetWeight() == wxFONTWEIGHT_BOLD
            ? Heading() : spellcross_menu::Text());
        if (auto* list = dynamic_cast<wxListCtrl*>(window))
        {
            list->SetTextColour(spellcross_menu::Text());
            list->SetHeaderAttr(wxItemAttr(spellcross_menu::Accent(),
                spellcross_menu::Background(), spellcross_menu::MenuFont()));
        }
        if (auto* text = dynamic_cast<wxTextCtrl*>(window))
            text->SetDefaultStyle(wxTextAttr(spellcross_menu::Text(), spellcross_menu::Background()));
        for (auto* child : window->GetChildren())
            Apply(child);
        if (auto* dialog = dynamic_cast<wxDialog*>(window))
            if (dialog->GetSizer()) dialog->GetSizer()->Fit(dialog);
        window->Refresh();
    }
}
