// Run with a display server, e.g. xvfb-run -a ./test_game_menu_bar.
#include "../source/game_menu_bar.h"
#include <stdexcept>

// Model synchronous show events delivered while a native transition is still
// restoring decorations. A second transition at that point corrupts MSW state.
class ReentrantFrame : public wxFrame
{
    bool m_transition = false;
public:
    ReentrantFrame()
        : wxFrame(nullptr, wxID_ANY, "Spellcross menu regression",
            wxDefaultPosition, wxSize(800, 600)) {}

    bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL) override
    {
        if (m_transition)
            throw std::runtime_error("Fullscreen transition re-entered");
        m_transition = true;
        wxShowEvent event(GetId(), true);
        event.SetEventObject(this);
        GetEventHandler()->ProcessEvent(event);
        wxYield();
        const bool result = wxFrame::ShowFullScreen(show, style);
        m_transition = false;
        return result;
    }
};

class MenuTestApp : public wxApp
{
    static void Check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    static wxFrame* Screen()
    {
        auto* frame = new ReentrantFrame;
        frame->CreateStatusBar(2);
        auto* model = new wxMenuBar;
        auto* file = new wxMenu;
        file->Append(10000, "Load");
        model->Append(file, "&File");
        auto* options = new wxMenu;
        options->Append(10001, "Audio\tCtrl+O");
        spellcross_window::AddFullscreen(options);
        auto* submenu = new wxMenu;
        submenu->AppendRadioItem(10002, "Normal");
        submenu->AppendRadioItem(10003, "Hard");
        options->AppendSubMenu(submenu, "Difficulty");
        model->Append(options, "&Options");
        spellcross_menu::Install(frame, model);
        auto* canvas = new wxPanel(frame, 10004);
        frame->Show();
        canvas->SetFocus();
        return frame;
    }

    static void Toggle(wxFrame* frame)
    {
        wxCommandEvent event(wxEVT_MENU, spellcross_window::FullscreenId());
        frame->GetEventHandler()->ProcessEvent(event);
    }

public:
    bool OnInit() override { return true; }

    int OnRun() override
    {
        wxFrame* frames[3] = {};
        try
        {
            frames[0] = Screen();
            wxYield();
            Check(spellcross_menu::EnsureMenuFontLoaded(), "Bundled menu font not loaded");
            Check(frames[0]->GetToolBar()->GetFont().GetFaceName() == "Fixedsys Excelsior 3.01",
                "Menu did not select the bundled font");
            auto* anchor = wxWindow::FindWindowByName("Options", frames[0]);
            Check(anchor != nullptr, "Options button missing");
            Check(frames[0]->GetAcceleratorTable()->IsOk(), "Menu shortcuts missing");
            auto* popup = new spellcross_menu::Popup(frames[0],
                spellcross_menu::Model(frames[0])->GetMenu(1), anchor);
            wxYield();
            Check(popup->IsShown(), "Popup dismissed while opening");
            Check(popup->GetPosition().x == anchor->ClientToScreen(wxPoint(0, 0)).x,
                "Popup not aligned with its own menu button");
            Check(popup->GetPosition().y == anchor->ClientToScreen(wxPoint(0, anchor->GetSize().y)).y,
                "Popup not positioned below its own menu button");
            Check(popup->GetFont().GetFaceName() == anchor->GetFont().GetFaceName(),
                "Popup font differs from menu font");
            wxKeyEvent escape(wxEVT_CHAR_HOOK);
            escape.m_keyCode = WXK_ESCAPE;
            popup->GetEventHandler()->ProcessEvent(escape);
            wxYield();
            Check(wxWindow::FindFocus() == frames[0]->FindWindow(10004),
                "Closing menu lost gameplay focus");
            const auto windowSize = frames[0]->GetSize();
            auto* fullscreenPopup = new spellcross_menu::Popup(frames[0],
                spellcross_menu::Model(frames[0])->GetMenu(1), anchor);
            wxYield();
            wxKeyEvent altEnter(wxEVT_CHAR_HOOK);
            altEnter.m_keyCode = WXK_RETURN;
            altEnter.SetAltDown(true);
            fullscreenPopup->GetEventHandler()->ProcessEvent(altEnter);
            wxYield();
            Check(frames[0]->IsFullScreen(), "Fullscreen command failed");
            Check(frames[0]->GetToolBar()->IsShown(), "Menu hidden in fullscreen");
            frames[0]->Hide();

            // Strategy and tactical windows created after switching inherit
            // the session preference, just like the game's independent frames.
            for (int i = 1; i < 3; ++i)
            {
                frames[i] = Screen();
                wxYield();
                Check(frames[i]->IsFullScreen(), "New screen lost fullscreen");
                Check(frames[i]->GetToolBar()->IsShown(), "New screen lost menu");
                auto* model = spellcross_menu::Model(frames[i]);
                Check(model && model->FindItem(10003), "Submenu command lost");
                model->Check(10003, true);
                Check(model->IsChecked(10003), "Radio menu state lost");
                frames[i]->Hide();
            }
            frames[2]->Show();
            Toggle(frames[2]);
            wxYield();
            Check(!frames[2]->IsFullScreen(), "Windowed command failed");
            frames[2]->Hide();
            frames[0]->Show();
            wxYield();
            Check(!frames[0]->IsFullScreen(), "Returning screen kept old mode");
            Check(frames[0]->GetSize() == windowSize, "Window size not restored");
            for (auto* frame : frames)
            {
                frame->Show();
                wxYield();
                for (int cycle = 0; cycle < 10; ++cycle)
                {
                    Toggle(frame);
                    wxYield();
                    Check(frame->IsFullScreen(), "Repeated fullscreen entry failed");
                    Toggle(frame);
                    wxYield();
                    Check(!frame->IsFullScreen(), "Repeated fullscreen exit failed");
                    Check(frame->GetToolBar()->IsShown(), "Repeated transition hid menu");
                    Check(frame->GetStatusBar()->IsShown(), "Windowed status bar not restored");
                }
                frame->Hide();
            }
        }
        catch (const std::exception& error)
        {
            wxFprintf(stderr, "FAIL: %s\n", error.what());
            for (auto* frame : frames)
                delete frame;
            return 1;
        }
        for (auto* frame : frames)
            delete frame;
        return 0;
    }
};
wxIMPLEMENT_APP(MenuTestApp);
