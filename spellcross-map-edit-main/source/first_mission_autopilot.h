// Experimental in-game M01_01A spectator pilot. Included exactly once by
// main.cpp after IsIntroEscapeMission(). Kept out of the retail game flow.
// Uses native SpellMap combat, pathfinding, animation and mission events.
#pragma once
#include <wx/choice.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

// Modeless controls deliberately leave the battlefield visible.
void MainFrame::OpenBotControls()
{
    if (m_botControls)
    {
        m_botControls->Show(true);
        m_botControls->Raise();
        return;
    }

    m_botControls = new wxFrame(this, wxID_ANY, "First mission bot - experimental",
        wxDefaultPosition, wxSize(440, 315),
        wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT);
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(new wxStaticText(m_botControls, wxID_ANY,
        "M01_01A: rescue the palisade Commando, then extract Alexander."),
        0, wxALL, 10);

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(m_botControls, wxID_ANY, "Speed:"),
        0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    auto* speed = new wxChoice(m_botControls, wxID_ANY);
    const int rates[] = {1, 2, 5, 10};
    int selection = 0;
    for (int i = 0; i < 4; ++i)
    {
        speed->Append(wxString::Format("%dx", rates[i]));
        if (m_botSpeed == rates[i]) selection = i;
    }
    speed->SetSelection(selection);
    speed->Bind(wxEVT_CHOICE, [this, speed](wxCommandEvent&)
    {
        const int choices[] = {1, 2, 5, 10};
        const int choice = speed->GetSelection();
        if (choice >= 0 && choice < 4)
            m_botSpeed = choices[choice];
        UpdateBotStatus();
    });
    row->Add(speed, 0, wxRIGHT, 10);

    m_botStartPause = new wxButton(m_botControls, wxID_ANY, "Start");
    row->Add(m_botStartPause, 0, wxRIGHT, 6);
    auto* stop = new wxButton(m_botControls, wxID_ANY, "Stop");
    row->Add(stop);
    root->Add(row, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    m_botStatus = new wxStaticText(m_botControls, wxID_ANY, "Stopped");
    root->Add(m_botStatus, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    m_botOutput = new wxTextCtrl(m_botControls, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
    root->Add(m_botOutput, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    root->Add(new wxStaticText(m_botControls, wxID_ANY,
        "F8: controls   F9: pause/resume. Stop to take over manually."),
        0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    m_botControls->SetSizer(root);

    m_botStartPause->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        if (m_botRunning)
        {
            m_botPaused = !m_botPaused;
            BotLog(m_botPaused ? "Paused." : "Resumed.");
        }
        else
        {
            if (!spell_map || !spell_map->IsLoaded() || !spell_map->isGameMode() ||
                !IsIntroEscapeMission(spell_map) || m_deployment_active ||
                m_mission_end_flow)
            {
                wxMessageBox("Start the first campaign mission (M01_01A) first.\n"
                    "Other missions and deployment are not supported.",
                    "First mission bot", wxOK | wxICON_INFORMATION, m_botControls);
                return;
            }
            if (spell_map->IsGroupMode())
                spell_map->SetActiveGroup(0);
            m_botRunning = true;
            m_botPaused = false;
            m_botHeroId = -1;
            m_botRescueReached = false;
            m_botTurnCount = 0;
            m_botNoProgressTurns = 0;
            m_botCooldown = 0;
            m_botMessageTicks = 0;
            m_botMessageCloseRequested = false;
            m_botOutput->Clear();
            BotLog("Started. Native game simulation drives all actions.");
        }
        UpdateBotStatus();
    });
    stop->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        StopFirstMissionBot("Stopped. Manual control restored.");
    });
    m_botControls->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& ev)
    {
        StopFirstMissionBot("Spectator closed.");
        m_botControls = nullptr;
        m_botStartPause = nullptr;
        m_botStatus = nullptr;
        m_botOutput = nullptr;
        ev.Skip();
    });

    UpdateBotStatus();
    m_botControls->Show(true);
}

void MainFrame::BotLog(const wxString& message)
{
    if (!m_botOutput)
        return;
    if (m_botOutput->GetLastPosition() > 20000)
        m_botOutput->Clear();
    m_botOutput->AppendText(message + "\n");
}

void MainFrame::UpdateBotStatus()
{
    if (m_botStartPause)
        m_botStartPause->SetLabel(!m_botRunning ? "Start" :
            (m_botPaused ? "Resume" : "Pause"));
    if (m_botStatus)
    {
        const wxString state = !m_botRunning ? "Stopped" :
            (m_botPaused ? "Paused" : "Watching AI");
        m_botStatus->SetLabel(state +
            wxString::Format(" | %dx | Turn %d / 120", m_botSpeed, m_botTurnCount));
    }
}

void MainFrame::StopFirstMissionBot(const wxString& reason)
{
    if (m_botRunning)
        BotLog(reason);
    m_botRunning = false;
    m_botPaused = false;
    m_botCooldown = 0;
    m_botMessageTicks = 0;
    m_botMessageCloseRequested = false;
    UpdateBotStatus();
}

// First-mission event dialogs are game-script gates, not ordinary OS popups.
// Acknowledge via FormMsgBox::Close so its normal OnClose -> ResultCallback
// sequence fires, exactly as when the player clicks or presses Enter.
void MainFrame::TickFirstMissionBotMessages()
{
    if (!m_botRunning || m_botPaused || !form_message || form_video_box ||
        m_mission_end_flow || m_botMessageCloseRequested)
        return;

    // Keep the message readable in spectator mode, even at 10x simulation
    // speed (this routine runs once per 10 ms UI timer, not per game tick).
    constexpr int kReadTimeTicks = 180;
    if (++m_botMessageTicks < kReadTimeTicks)
        return;

    m_botMessageCloseRequested = true; // close event is delivered asynchronously
    const bool hasChoice = form_message->IsChoicePrompt();
    BotLog(hasChoice ? "Message: selected Yes to continue." :
        "Message: acknowledged information.");
    form_message->AcknowledgeForAutopilot(true);
}

// Simplest useful test AI: one persistent Alliance hero, real enemy turns,
// actual fog-of-war attack checks, and A* route toward a native escape square.
// This is intentionally *not* a solver for all mission scripting.
void MainFrame::StepFirstMissionBot()
{
    if (!m_botRunning || m_botPaused || !spell_map ||
        !spell_map->IsLoaded() || m_mission_end_flow)
        return;

    if (!IsIntroEscapeMission(spell_map) || !spell_map->isGameMode())
    {
        StopFirstMissionBot("Mission finished or changed.");
        return;
    }
    if (m_deployment_active || spell_map->IsEnemyTurnRunning() ||
        spell_map->m_mission_end_req.pending || CheckMessageState())
        return;
    if (m_botCooldown > 0)
    {
        --m_botCooldown;
        return;
    }
    if (m_botTurnCount >= 120)
    {
        StopFirstMissionBot("Stopped: 120-turn safety limit.");
        return;
    }

    // Wait for the regular movement/attack state machine to finish.
    for (MapUnit* unit : spell_map->units)
        if (unit && (unit->isMoving() || unit->isAttacker() ||
            unit->action_state != MapUnit::ACTION_STATE::IDLE))
            return;

    if (m_botHeroId < 0)
    {
        MapUnit* fallback = nullptr;
        for (MapUnit* unit : spell_map->units)
        {
            if (!unit || !unit->unit || unit->is_enemy || unit->isDead())
                continue;
            if (!fallback) fallback = unit;
            if (unit->spec_type == MapUnitType::MissionUnit)
            {
                fallback = unit;
                break;
            }
        }
        if (!fallback)
        {
            StopFirstMissionBot("No living Alliance unit found.");
            return;
        }
        m_botHeroId = fallback->id;
        BotLog(wxString::Format("Following Alliance unit #%d.", m_botHeroId));
    }

    MapUnit* hero = spell_map->GetUnit(m_botHeroId);
    if (!hero || hero->isDead())
    {
        StopFirstMissionBot("The controlled unit was lost.");
        return;
    }
    if (!hero->isActive() || hero->panic_turns > 0 ||
        hero->paralyze_turns > 0 || hero->freeze_turns > 0)
    {
        // Status effects and newly spawned inactive companies must never
        // fire or move. Advance a real turn instead of busy-looping forever.
        ++m_botTurnCount;
        m_botCooldown = 12;
        spell_map->OnHUDswitchEndTurn();
        UpdateBotStatus();
        return;
    }

    if (spell_map->GetSelectedUnit() != hero)
    {
        spell_map->SelectUnit(hero, true);
        m_botCooldown = 12; // async visibility/attack map refresh
        return;
    }

    MapUnit* bestTarget = nullptr;
    double bestDistance = 1e30;
    if (hero->GetFireCount() > 0)
        for (MapUnit* enemy : spell_map->units)
        {
            if (!enemy || !enemy->unit || !enemy->is_enemy || enemy->isDead() ||
                !enemy->coor.IsSelected() ||
                !spell_map->TileIsVisible(enemy->coor.x, enemy->coor.y) ||
                !spell_map->CanUnitAttack(enemy))
                continue;
            const double distance = hero->coor.Distance(enemy->coor);
            if (distance < bestDistance)
            {
                bestTarget = enemy;
                bestDistance = distance;
            }
        }

    if (bestTarget)
    {
        const MapXY enemyPos = bestTarget->coor;
        const int targetId = bestTarget->id;
        const int layer = bestTarget->unit->isAir() ? 1 : 0;
        if (spell_map->Attack(enemyPos, layer) == 0)
        {
            BotLog(wxString::Format("Attack on enemy #%d.", targetId));
            m_botCooldown = 12;
            m_botNoProgressTurns = 0;
            return;
        }
    }

    // Escape/target markers are loaded from the original mission, not guessed.
    const auto& goals = !spell_map->escape.empty() ?
        spell_map->escape : spell_map->target;
    if (goals.empty())
    {
        StopFirstMissionBot("No escape/target squares in mission data.");
        return;
    }
    const MapXY goal = *std::min_element(goals.begin(), goals.end(),
        [hero](const MapXY& a, const MapXY& b)
        { return hero->coor.Distance(a) < hero->coor.Distance(b); });

    if (spell_map->AutopilotMoveToward(hero, goal))
    {
        spell_map->ScrollToUnit(hero);
        m_botNoProgressTurns = 0;
        m_botCooldown = 12;
        BotLog(wxString::Format("Moving toward exit (%d, %d).", goal.x, goal.y));
        if (canvas) canvas->Refresh();
        return;
    }

    // Reserve leftover AP for defensive fire; do not cheat movement/combat.
    ++m_botTurnCount;
    ++m_botNoProgressTurns;
    BotLog(wxString::Format("End of turn %d.", m_botTurnCount));
    if (m_botNoProgressTurns >= 12)
    {
        StopFirstMissionBot("No progress for 12 turns; inspect map manually.");
        return;
    }
    spell_map->OnHUDswitchEndTurn();
    m_botCooldown = 12;
    UpdateBotStatus();
}
