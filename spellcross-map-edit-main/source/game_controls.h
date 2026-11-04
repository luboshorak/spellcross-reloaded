#pragma once

#include "game_menu_bar.h"
#include <wx/display.h>
#include <wx/scrolwin.h>
#include <wx/stattext.h>

namespace spellcross_help
{
    inline int ControlsId()
    {
        static const int id = wxWindow::NewControlId();
        return id;
    }

    class ControlsDialog final : public wxDialog
    {
    public:
        explicit ControlsDialog(wxWindow* owner)
            : wxDialog(owner, wxID_ANY,
                spellcross_ui_locale::Tr("help.title", "Spellcross Reloaded controls"),
                wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        {
            SetBackgroundColour(spellcross_menu::Background());
            SetForegroundColour(spellcross_menu::Text());
            SetFont(spellcross_menu::MenuFont());
            const int displayIndex = wxDisplay::GetFromWindow(owner);
            const wxRect area = wxDisplay(displayIndex == wxNOT_FOUND ? 0 : displayIndex).GetClientArea();
            const int width = std::min(FromDIP(820), area.width - FromDIP(40));
            const int height = std::min(FromDIP(650), area.height - FromDIP(40));
            const int keyWidth = FromDIP(200);
            const int textWidth = std::max(FromDIP(120), width - keyWidth - FromDIP(70));

            auto* layout = new wxBoxSizer(wxVERTICAL);
            auto* scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition,
                wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
            scroll->SetBackgroundColour(spellcross_menu::Background());
            scroll->SetScrollRate(0, FromDIP(16));
            auto* sections = new wxBoxSizer(wxVERTICAL);

            const auto heading = [&](const char* key, const char* fallback) {
                auto* label = new wxStaticText(scroll, wxID_ANY, spellcross_ui_locale::Tr(key, fallback));
                label->SetForegroundColour(wxColour(227, 233, 24));
                sections->Add(label, 0, wxTOP | wxBOTTOM, FromDIP(12));
            };
            const auto row = [&](const char* key, const char* keyFallback,
                                 const char* description, const char* descriptionFallback) {
                auto* line = new wxBoxSizer(wxHORIZONTAL);
                auto* shortcut = new wxStaticText(scroll, wxID_ANY,
                    spellcross_ui_locale::Tr(key, keyFallback), wxDefaultPosition,
                    wxSize(keyWidth, -1));
                shortcut->SetForegroundColour(spellcross_menu::Accent());
                shortcut->Wrap(keyWidth - FromDIP(8));
                auto* text = new wxStaticText(scroll, wxID_ANY,
                    spellcross_ui_locale::Tr(description, descriptionFallback));
                text->SetForegroundColour(spellcross_menu::Text());
                text->Wrap(textWidth);
                line->Add(shortcut, 0, wxRIGHT, FromDIP(12));
                line->Add(text, 1);
                sections->Add(line, 0, wxEXPAND | wxBOTTOM, FromDIP(12));
            };

            heading("help.general", "Menus and display");
            row("help.key.fullscreen", "Alt+Enter", "help.action.fullscreen",
                "Switch between fullscreen and windowed mode on all three game screens.");
            row("help.key.menu", "Mouse / arrows", "help.action.menu",
                "Click a menu item to open it. Use arrows and Enter to choose, Escape to close.");

            heading("help.mouse", "Tactical map — mouse");
            row("help.key.left", "Left button", "help.action.left",
                "Select a unit, move or attack according to the clicked location. Choose an action if several are available.");
            row("help.key.drag", "Hold right button", "help.action.drag",
                "Drag the mouse to scroll the battlefield.");
            row("help.key.wheel", "Mouse wheel", "help.action.wheel",
                "Zoom the map around the cursor. Up zooms in, down zooms out (50–300%).");
            row("help.key.info", "Info + left click", "help.action.info",
                "Click the HUD Info button, then a visible unit to inspect its statistics. Escape cancels inspection.");

            heading("help.keys", "Tactical map — keyboard");
            row("help.key.space", "Space", "help.action.space",
                "Cycle the normal, movement-range and attack-range views.");
            row("help.key.ranges", "Hold M / A", "help.action.ranges",
                "Temporarily show movement range (M) or attack range (A). Release the key to restore the previous view.");
            row("help.key.escape", "Escape", "help.action.escape",
                "Close an open tactical overlay or cancel the active group. Otherwise open the main menu.");
            row("help.key.enter", "Enter", "help.action.enter",
                "Confirm initial deployment once all companies have been placed.");
            row("help.key.tactical_save", "Ctrl+Shift+S / Ctrl+Shift+L", "help.action.tactical_save",
                "Save (S) or load (L) a tactical game.");

            heading("help.groups", "Tactical map — unit groups");
            row("help.key.groups", "1–8", "help.action.groups",
                "Activate a group. Press the same number again to deselect it.");
            row("help.key.group_mouse", "Left / Shift+left", "help.action.group_mouse",
                "With a group active, click a friendly unit to add or remove it. Shift selects the air unit on a stacked tile.");
            row("help.key.all", "Alt+A", "help.action.all",
                "Add all living allied units to the active group.");

            heading("help.strategic", "Strategic map");
            row("help.key.strategic_mouse", "Left button", "help.action.strategic_mouse",
                "Select territories and use the on-screen buttons for missions, research, purchases and unit management.");
            row("help.key.strategic_save", "Ctrl+S / Ctrl+L", "help.action.strategic_save",
                "Save or load a strategic game. On the opening screen, Continue resumes the last automatic exit save.");

            scroll->SetSizer(sections);
            layout->Add(scroll, 1, wxEXPAND | wxALL, FromDIP(16));
            auto* close = new wxButton(this, wxID_OK, spellcross_ui_locale::Tr("help.close", "Close"));
            close->SetBackgroundColour(spellcross_menu::Highlight());
            close->SetForegroundColour(spellcross_menu::Text());
            close->SetDefault();
            layout->Add(close, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(16));
            SetSizer(layout);
            SetSize(width, height);
            SetMinSize(wxSize(width, std::min(height, FromDIP(320))));
            SetEscapeId(wxID_OK);
            CentreOnParent();
        }
    };

    inline wxMenu* BuildMenu()
    {
        auto* help = new wxMenu;
        help->Append(ControlsId(), spellcross_ui_locale::Tr("menu.controls", "&Controls"));
        help->AppendSeparator();
        help->Append(wxID_ABOUT, spellcross_ui_locale::Tr("menu.about", "&About"));
        return help;
    }

    inline void BindControls(wxFrame* owner)
    {
        owner->Bind(wxEVT_MENU, [owner](wxCommandEvent&) {
            ControlsDialog dialog(owner);
            dialog.ShowModal();
        }, ControlsId());
    }
}
