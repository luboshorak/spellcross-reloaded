#pragma once

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <wx/toolbar.h>
#include <wx/weakref.h>
#include <wx/stdpaths.h>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <vector>
#include "spell_ui_locale.h"
#ifdef __WXMSW__
#include "resource.h"
#endif

// All three game windows share a session display preference.
namespace spellcross_window
{
    inline bool& Fullscreen()
    {
        static bool enabled = false;
        return enabled;
    }

    inline int FullscreenId()
    {
        static const int id = wxWindow::NewControlId();
        return id;
    }

    inline bool& ChangingMode()
    {
        static bool changing = false;
        return changing;
    }

    inline void Apply(wxFrame* frame)
    {
        if (ChangingMode() || frame->IsBeingDeleted() || !frame->IsShown() ||
            frame->IsFullScreen() == Fullscreen())
            return;

        // MSW restores decorations and bars synchronously, dispatching window
        // events before ShowFullScreen returns. Those events must not re-enter.
        struct TransitionGuard
        {
            TransitionGuard() { ChangingMode() = true; }
            ~TransitionGuard() { ChangingMode() = false; }
        } guard;
        const bool requested = Fullscreen();
        const bool changed = frame->ShowFullScreen(requested,
            wxFULLSCREEN_NOCAPTION | wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOSTATUSBAR);
        if (!changed)
            Fullscreen() = frame->IsFullScreen();
        frame->Layout();
        frame->Refresh();
    }

    inline void RequestApply(wxFrame* frame)
    {
        if (ChangingMode())
            return;
        const wxWeakRef<wxFrame> window(frame);
        // Leave the native menu/show/focus callback before changing HWND styles.
        frame->CallAfter([window]() {
            if (window)
                Apply(window.get());
        });
    }

    inline void AddFullscreen(wxMenu* options)
    {
        options->AppendSeparator();
        options->AppendCheckItem(FullscreenId(),
            spellcross_ui_locale::Tr("menu.fullscreen", "&Fullscreen\tAlt+Enter"));
    }
}

namespace spellcross_menu
{
    inline bool EnsureMenuFontLoaded()
    {
        static const bool loaded = []() {
#ifdef __WXMSW__
            // The private memory font remains available until process exit.
            const HMODULE module = GetModuleHandleW(nullptr);
            const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_GAME_MENU_FONT), RT_RCDATA);
            if (!resource)
                return false;
            const DWORD size = SizeofResource(module, resource);
            const HGLOBAL bytes = LoadResource(module, resource);
            void* data = bytes ? LockResource(bytes) : nullptr;
            if (!data || !size)
                return false;
            DWORD count = 0;
            const HANDLE font = AddFontMemResourceEx(data, size, nullptr, &count);
            return font != nullptr && count > 0;
#else
            // Non-Windows builds load the same bundled file privately.
            namespace fs = std::filesystem;
            const fs::path executable(wxStandardPaths::Get().GetExecutablePath().ToStdWstring());
            std::error_code ec;
            const fs::path cwd = fs::current_path(ec);
            const fs::path candidates[] = {
                executable.parent_path() / "data" / "font.ttf",
                cwd / "data" / "font.ttf",
                cwd / "spellcross-map-edit-main" / "data" / "font.ttf"
            };
            for (const auto& path : candidates)
            {
                ec.clear();
                if (fs::is_regular_file(path, ec) &&
                    wxFont::AddPrivateFont(wxString(path.wstring())))
                    return true;
            }
            return false;
#endif
        }();
        return loaded;
    }

    inline wxFont MenuFont()
    {
        if (EnsureMenuFontLoaded())
            return wxFont(wxFontInfo(12).FaceName("Fixedsys Excelsior 3.01"));
        return wxFont(wxFontInfo(12).Family(wxFONTFAMILY_TELETYPE));
    }

    inline wxColour Background() { return wxColour(8, 24, 7); }
    inline wxColour Highlight() { return wxColour(24, 51, 15); }
    inline wxColour Accent() { return wxColour(61, 207, 32); }
    inline wxColour Text() { return wxColour(62, 224, 25); }

    inline std::unique_ptr<wxMenu> CloneMenu(const wxMenu* source)
    {
        auto clone = std::make_unique<wxMenu>();
        for (auto* item : source->GetMenuItems())
        {
            if (item->IsSeparator())
                clone->AppendSeparator();
            else if (item->GetSubMenu())
            {
                auto submenu = CloneMenu(item->GetSubMenu());
                auto* copy = wxMenuItem::New(clone.get(), item->GetId(),
                    item->GetItemLabel(), item->GetHelp(), item->GetKind(), submenu.release());
                clone->Append(copy);
                copy->Enable(item->IsEnabled());
            }
            else
            {
                auto* copy = clone->Append(item->GetId(), item->GetItemLabel(),
                    item->GetHelp(), item->GetKind());
                copy->Enable(item->IsEnabled());
                if (item->IsCheckable())
                    copy->Check(item->IsChecked());
            }
        }
        return clone;
    }

    inline void CopyMenuChecks(wxMenu* source, wxMenu* target)
    {
        auto sourceItem = source->GetMenuItems().begin();
        auto targetItem = target->GetMenuItems().begin();
        for (; sourceItem != source->GetMenuItems().end() &&
            targetItem != target->GetMenuItems().end(); ++sourceItem, ++targetItem)
        {
            auto* sourceRow = *sourceItem;
            auto* targetRow = *targetItem;
            if (sourceRow->IsCheckable())
                targetRow->Check(sourceRow->IsChecked());
            if (sourceRow->GetSubMenu() && targetRow->GetSubMenu())
                CopyMenuChecks(sourceRow->GetSubMenu(), targetRow->GetSubMenu());
        }
    }

    // The toolbar reserves client space on every frame, including the menu
    // screen that paints directly into its frame rather than a child canvas.
    // The wxMenuBar is an owned command model, never an OS-rendered menubar.
    class Bar final : public wxToolBar
    {
        std::unique_ptr<wxMenuBar> m_model;

        void Open(size_t index, wxWindow* anchor)
        {
            if (!m_model->IsEnabledTop(index))
                return;
            auto* frame = static_cast<wxFrame*>(GetParent());
            if (auto* item = m_model->FindItem(spellcross_window::FullscreenId()))
                item->Check(frame->IsFullScreen());
            const wxPoint position = frame->ScreenToClient(
                anchor->ClientToScreen(wxPoint(0, anchor->GetSize().y)));
            auto popup = CloneMenu(m_model->GetMenu(index));
            frame->PopupMenu(popup.get(), position);
            CopyMenuChecks(popup.get(), m_model->GetMenu(index));
        }

        void AddAccelerators(wxMenu* menu, std::vector<wxAcceleratorEntry>& entries)
        {
            for (auto* item : menu->GetMenuItems())
            {
                if (item->GetSubMenu())
                    AddAccelerators(item->GetSubMenu(), entries);
                else
                {
                    std::unique_ptr<wxAcceleratorEntry> entry(item->GetAccel());
                    if (entry)
                    {
                        entry->Set(entry->GetFlags(), entry->GetKeyCode(), item->GetId());
                        entries.push_back(*entry);
                    }
                }
            }
        }

    public:
        Bar(wxFrame* frame, wxMenuBar* model)
            : wxToolBar(frame, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER | wxBORDER_NONE),
              m_model(model)
        {
            SetFont(MenuFont());
            SetBackgroundColour(Background());
            SetMargins(FromDIP(4), FromDIP(2));
            std::vector<wxAcceleratorEntry> entries;
            for (size_t index = 0; index < model->GetMenuCount(); ++index)
            {
                const auto label = model->GetMenuLabelText(index);
                wxClientDC dc(this);
                dc.SetFont(GetFont());
                auto* button = new wxControl(this, wxID_ANY, wxDefaultPosition,
                    wxSize(dc.GetTextExtent(label).x + FromDIP(32), FromDIP(32)),
                    wxBORDER_NONE | wxWANTS_CHARS);
                button->SetFont(GetFont());
                button->SetBackgroundStyle(wxBG_STYLE_PAINT);
                button->SetName(label);
                button->SetToolTip(label);
                button->Bind(wxEVT_PAINT, [this, button, label, index](wxPaintEvent&) {
                    wxAutoBufferedPaintDC paint(button);
                    const bool hover = button->GetClientRect().Contains(
                        button->ScreenToClient(wxGetMousePosition()));
                    paint.SetBackground(wxBrush(hover ? Highlight() : Background()));
                    paint.Clear();
                    paint.SetFont(GetFont());
                    paint.SetTextForeground(m_model->IsEnabledTop(index)
                        ? (hover ? wxColour(227, 233, 24) : Accent()) : wxColour(76, 104, 68));
                    const auto extent = paint.GetTextExtent(label);
                    const auto size = button->GetClientSize();
                    paint.DrawText(label, (size.x - extent.x) / 2, (size.y - extent.y) / 2);
                    paint.SetPen(wxPen(Accent()));
                    paint.DrawLine(0, size.y - 1, size.x, size.y - 1);
                });
                button->Bind(wxEVT_ENTER_WINDOW, [button](wxMouseEvent&) { button->Refresh(); });
                button->Bind(wxEVT_LEAVE_WINDOW, [button](wxMouseEvent&) { button->Refresh(); });
                button->Bind(wxEVT_LEFT_DOWN, [this, index, button](wxMouseEvent&) { Open(index, button); });
                button->Bind(wxEVT_KEY_DOWN, [this, index, button](wxKeyEvent& event) {
                    if (event.GetKeyCode() == WXK_SPACE || event.GetKeyCode() == WXK_RETURN ||
                        event.GetKeyCode() == WXK_DOWN)
                        Open(index, button);
                    else
                        event.Skip();
                });
                AddControl(button);
                AddAccelerators(model->GetMenu(index), entries);
                const auto rawLabel = model->GetMenuLabel(index);
                const int mnemonic = rawLabel.Find('&');
                if (mnemonic != wxNOT_FOUND && mnemonic + 1 < static_cast<int>(rawLabel.length()))
                {
                    const int id = wxWindow::NewControlId();
                    entries.emplace_back(wxACCEL_ALT,
                        wxToupper(rawLabel[mnemonic + 1]), id);
                    frame->Bind(wxEVT_MENU, [this, index, button](wxCommandEvent&) {
                        Open(index, button);
                    }, id);
                }
            }
            Realize();
            frame->SetToolBar(this);
            frame->SetAcceleratorTable(wxAcceleratorTable(
                static_cast<int>(entries.size()), entries.data()));
            frame->Bind(wxEVT_MENU, [frame](wxCommandEvent&) {
                spellcross_window::Fullscreen() = !spellcross_window::Fullscreen();
                spellcross_window::RequestApply(frame);
            }, spellcross_window::FullscreenId());
            frame->Bind(wxEVT_SHOW, [frame](wxShowEvent& event) {
                if (event.GetEventObject() == frame && event.IsShown())
                    spellcross_window::RequestApply(frame);
                event.Skip();
            });
        }

        wxMenuBar* Model() const { return m_model.get(); }
    };

    inline wxMenuBar* Model(wxFrame* frame)
    {
        auto* bar = dynamic_cast<Bar*>(frame->GetToolBar());
        return bar ? bar->Model() : nullptr;
    }

    inline void Install(wxFrame* frame, wxMenuBar* model)
    {
        new Bar(frame, model);
    }
}
