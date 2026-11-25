#include <wx/wx.h>
#include "map.h"
#include "main.h"
#include "forms/form_level.h"
#include <wx/init.h>
#include <wx/modalhook.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

struct EventDialogs : wxModalDialogHook
{
    StrategicLevelFrame* strategy = nullptr;
    int count = 0;
    bool painted = false;
    int Enter(wxDialog* dialog) override
    {
        if (dialog->GetTitle() == "Event")
        {
            Check(strategy && strategy->IsShownOnScreen(), "Event displayed over tactical window");
            Check(!strategy->m_pendingMission.valid && painted, "Event displayed before return/first map paint");
            Check(dialog->GetParent() == strategy, "Wrong event owner");
            ++count;
        }
        return wxID_OK;
    }
};

int main(int argc, char** argv)
{
    if (argc != 5) return 2;
    wxInitializer wx;
    if (!wx) return 2;
    wxDisableAsserts(); wxInitAllImageHandlers();
    namespace fs = std::filesystem;
    try
    {
        const fs::path common(argv[3]), output(argv[4]);
        fs::create_directories(output);
        fs::current_path(fs::absolute(fs::path(argv[2])).parent_path());
        SpellData data(fs::path(argv[1]).wstring(), fs::path(argv[1]).wstring(), fs::path(argv[2]).wstring());
        LevelData level; LevelLoader loader; std::string error;
        Check(loader.LoadLevelDef((common / "LEVEL_02.DEF").string(), level, &error), "Retail level fixture");
        level.source_path = (output / "LEVEL_02.DEF").string();
        fs::copy_file(common / "LEVEL_02.DEF", level.source_path, fs::copy_options::overwrite_existing);
        // Retain the retail EventText; isolate its timing from unrelated army events.
        const auto intro = level.events.front();
        level.events = {intro};
        level.events.front().add_units.push_back({1, 1, 100, "Reinforcement"});
        SpellMap map;
        MainFrame controller(&map, &data);
        EventDialogs dialogs; dialogs.Register();
        StrategicLevelFrame strategy(&controller, level, true);
        dialogs.strategy = &strategy;
        strategy.m_originalStrategicPanel->Bind(wxEVT_PAINT, [&](wxPaintEvent& event) {
            dialogs.painted = true; event.Skip();
        });
        strategy.StartFreshGameMode({});
        Check(dialogs.count == 0 && strategy.m_pendingStrategicEventTexts.size() == 1,
            "Chapter intro opened a dialog while strategic frame was hidden");
        Check(strategy.m_triggeredLevelEvents.count(intro.id) &&
            strategy.m_playerUnits.size() == level.start_units.size() + 1,
            "Deferring text delayed or lost reinforcement actions");
        strategy.Show(); strategy.Raise();
        for (int i = 0; i < 5; ++i) wxTheApp->ProcessPendingEvents();
        Check(dialogs.count == 1 && strategy.m_pendingStrategicEventTexts.empty(), "Intro not shown after map paint");

        strategy.Hide();
        LevelEvent followup = intro; followup.id = 902;
        followup.add_units.push_back({1, 1, 100, "Follow-up reinforcement"});
        strategy.m_level.events.push_back(followup);
        strategy.m_pendingMission.valid = true;
        const size_t before = strategy.m_playerUnits.size();
        strategy.ProcessLevelEvents();
        Check(dialogs.count == 1 && strategy.m_playerUnits.size() == before + 1,
            "Hidden post-battle event lost its unit or showed a dialog");
        strategy.Show();
        for (int i = 0; i < 5; ++i) wxTheApp->ProcessPendingEvents();
        Check(dialogs.count == 1, "Post-battle text shown before mission handoff completed");
        strategy.m_pendingMission.valid = false;
        strategy.RunDeferredChapterIntroEvents();
        for (int i = 0; i < 5; ++i) wxTheApp->ProcessPendingEvents();
        Check(dialogs.count == 2 && strategy.m_pendingStrategicEventTexts.empty(), "Post-battle text did not drain");
        strategy.ProcessLevelEvents(); strategy.RunDeferredChapterIntroEvents();
        for (int i = 0; i < 5; ++i) wxTheApp->ProcessPendingEvents();
        Check(dialogs.count == 2 && strategy.m_playerUnits.size() == before + 1,
            "Repeated show/processing duplicated messages or reinforcements");
        strategy.Hide();
        std::cout << "PASS: intro/post-battle text after strategic paint, hidden/handoff gating, once-only reinforcement actions\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
