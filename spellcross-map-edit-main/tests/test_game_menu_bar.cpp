// Run with a display server, e.g. xvfb-run -a ./test_game_menu_bar.
#include "../source/game_controls.h"
#include <stdexcept>
#include <fstream>

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
        model->Append(spellcross_help::BuildMenu(), "&Help");
        spellcross_menu::Install(frame, model);
        spellcross_help::BindControls(frame);
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
            if (!spellcross_menu::EnsureMenuFontLoaded())
            {
#ifdef __WXMSW__
                const HMODULE module = GetModuleHandleW(nullptr);
                const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_GAME_MENU_FONT), RT_RCDATA);
                const DWORD resourceError = GetLastError();
                const DWORD size = resource ? SizeofResource(module, resource) : 0;
                const HGLOBAL bytes = resource ? LoadResource(module, resource) : nullptr;
                void* data = bytes ? LockResource(bytes) : nullptr;
                DWORD count = 0;
                const HANDLE font = data ? AddFontMemResourceEx(data, size, nullptr, &count) : nullptr;
                throw std::runtime_error("Bundled font failed: resource=" +
                    std::to_string(resource != nullptr) + " resource_error=" +
                    std::to_string(resourceError) + " bytes=" + std::to_string(size) +
                    " registered=" + std::to_string(font != nullptr) + " faces=" +
                    std::to_string(count) + " error=" + std::to_string(GetLastError()));
#else
                throw std::runtime_error("Bundled menu font not loaded");
#endif
            }
            Check(frames[0]->GetToolBar()->GetFont().GetFaceName() == "Fixedsys Excelsior 3.01",
                "Menu did not select the bundled font");
            Check(wxWindow::FindWindowByName("Options", frames[0]) != nullptr,
                "Options button missing");
            Check(frames[0]->GetAcceleratorTable()->IsOk(), "Menu shortcuts missing");
            auto* options = spellcross_menu::Model(frames[0])->GetMenu(1);
            auto nativePopup = spellcross_menu::CloneMenu(options);
            auto* audio = nativePopup->FindItem(10001);
            Check(audio && audio->GetItemLabelText() == "Audio" && audio->IsEnabled(),
                "Native popup menu lost its accessible command label or enabled state");
            auto* fullscreen = nativePopup->FindItem(spellcross_window::FullscreenId());
            Check(fullscreen && fullscreen->IsCheckable(),
                "Native popup menu lost its fullscreen check state");
            auto* radio = nativePopup->FindItem(10002);
            if (!radio || !radio->IsRadio())
                throw std::runtime_error("Native popup menu lost its radio command role");
            radio->Check(true);
            spellcross_menu::CopyMenuChecks(nativePopup.get(), options);
            auto* selectedRadio = options->FindItem(10002);
            Check(selectedRadio && selectedRadio->IsChecked(),
                "Native popup menu did not synchronize its selected state");
            // Exercise the real menu command and close its modal controls dialog.
            Check(spellcross_menu::Model(frames[0])->FindItem(spellcross_help::ControlsId()),
                "Controls menu missing");
            bool helpOpened = false;
            frames[0]->CallAfter([&] {
                for (auto* child : frames[0]->GetChildren())
                    if (auto* dialog = dynamic_cast<spellcross_help::ControlsDialog*>(child))
                    {
                        helpOpened = dialog->IsModal() && dialog->IsShown()
                            && dialog->GetFont().GetFaceName() == frames[0]->GetToolBar()->GetFont().GetFaceName();
                        dialog->EndModal(wxID_OK);
                    }
            });
            wxCommandEvent controls(wxEVT_MENU, spellcross_help::ControlsId());
            frames[0]->GetEventHandler()->ProcessEvent(controls);
            Check(helpOpened, "Controls menu did not open the styled modal guide");
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
            std::ofstream("menu-test-failure.txt") << error.what() << '\n';
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
