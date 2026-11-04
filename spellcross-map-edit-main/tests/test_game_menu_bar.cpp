// Run with a display server, e.g. xvfb-run -a ./test_game_menu_bar.
#include "../source/game_menu_bar.h"
#include <stdexcept>

class MenuTestApp : public wxApp
{
    static void Check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    static wxFrame* Screen()
    {
        auto* frame = new wxFrame(nullptr, wxID_ANY, "Spellcross menu regression",
            wxDefaultPosition, wxSize(800, 600));
        auto* model = new wxMenuBar;
        auto* options = new wxMenu;
        options->Append(10001, "Audio\tCtrl+O");
        spellcross_window::AddFullscreen(options);
        auto* submenu = new wxMenu;
        submenu->AppendRadioItem(10002, "Normal");
        submenu->AppendRadioItem(10003, "Hard");
        options->AppendSubMenu(submenu, "Difficulty");
        model->Append(options, "&Options");
        spellcross_menu::Install(frame, model);
        frame->Show();
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
            Check(frames[0]->GetAcceleratorTable()->IsOk(), "Menu shortcuts missing");
            auto* popup = new spellcross_menu::Popup(frames[0],
                spellcross_menu::Model(frames[0])->GetMenu(0), frames[0]->GetToolBar());
            wxYield();
            wxKeyEvent escape(wxEVT_KEY_DOWN);
            escape.m_keyCode = WXK_ESCAPE;
            popup->GetEventHandler()->ProcessEvent(escape);
            wxYield();
            const auto windowSize = frames[0]->GetSize();
            Toggle(frames[0]);
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
