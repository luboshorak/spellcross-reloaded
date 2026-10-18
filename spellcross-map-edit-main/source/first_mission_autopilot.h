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
            m_botSupportTried.clear();
            m_botSquadMoved.clear();
            m_botHeroMovedThisTurn = false;
            m_botContactWaitTurns = 0;
            m_botCurrentStance = 0;
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
        const char* stance = m_botCurrentStance == 1 ? " | HOLD" :
            (m_botCurrentStance == 2 ? " | SCOUT" : " | ADVANCE");
        m_botStatus->SetLabel(state +
            wxString::Format(" | %dx | Turn %d / 120", m_botSpeed, m_botTurnCount) + stance);
    }
}

void MainFrame::StopFirstMissionBot(const wxString& reason)
{
    if (m_botRunning)
        BotLog(reason);
    m_botRunning = false;
    m_botPaused = false;
    m_botCooldown = 0;
    m_botSupportTried.clear();
    m_botSquadMoved.clear();
    m_botHeroMovedThisTurn = false;
    m_botContactWaitTurns = 0;
    m_botCurrentStance = 0;
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

// First-mission spectator pilot: escort/rescue the permanent Commando before
// extracting the mission commander. This is a mission-aware heuristic rather
// than a full tactical solver; all mechanics still use the original engine.
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
        int bestRank = -1;
        for (MapUnit* unit : spell_map->units)
        {
            if (!unit || !unit->unit || unit->is_enemy || unit->isDead())
                continue;
            int rank = 0;
            if (unit->spec_type == MapUnitType::MissionUnit) rank = 1;
            if (unit->is_commander) rank = 2;
            if (unit->spec_type == MapUnitType::SpecUnit) rank = 3;
            if (spell_map->events)
                for (auto* ev : spell_map->events->GetEvents())
                    if (ev && ev->is_objective &&
                        ev->evt_type == SpellMapEventRec::EVT_TRANSPORT_UNIT &&
                        ev->trig_unit == unit)
                        rank = 4;
            if (rank > bestRank) { fallback = unit; bestRank = rank; }
        }
        if (!fallback)
        {
            StopFirstMissionBot("No living Alliance unit found.");
            return;
        }
        m_botHeroId = fallback->id;
        BotLog(wxString::Format("Following mission leader #%d.", m_botHeroId));
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
        m_botSupportTried.clear();
        m_botSquadMoved.clear();
        m_botHeroMovedThisTurn = false;
        m_botCooldown = 12;
        spell_map->OnHUDswitchEndTurn();
        UpdateBotStatus();
        return;
    }

    // Locate rescue via the *real* mission event records. A delayed SeePlace
    // or SeeUnit event can hold an unspawned ArmyUnit (permanent Commando).
    // An already active ArmyUnit is a rescue candidate as well.
    MapXY rescuePosition;
    bool rescueEventPending = false;
    MapUnit* rescueUnit = nullptr;
    if (!m_botRescueReached)
    {
        if (spell_map->events)
            for (auto* ev : spell_map->events->GetEvents())
            {
                if (!ev) continue;
                for (const auto& spawn : ev->units)
                {
                    auto* t = spawn.unit;
                    if (!t || !t->unit || t->is_enemy || spawn.is_placed ||
                        t->spec_type != MapUnitType::ArmyUnit)
                        continue;
                    const MapXY location = ev->hasPosition() ? ev->position : t->coor;
                    if (!location.IsSelected()) continue;
                    if (!rescueEventPending ||
                        hero->coor.Distance(location) < hero->coor.Distance(rescuePosition))
                    {
                        rescueEventPending = true;
                        rescuePosition = location;
                    }
                }
            }
        for (MapUnit* other : spell_map->units)
            if (other && other->unit && !other->is_enemy && !other->isDead() &&
                other->spec_type == MapUnitType::ArmyUnit &&
                (!rescueUnit || hero->coor.Distance(other->coor) <
                    hero->coor.Distance(rescueUnit->coor)))
                rescueUnit = other;

        // Approaching the living Commando represents actually making contact,
        // not simply exploiting its known coordinates from the DEF file.
        if (!rescueEventPending && rescueUnit)
        {
            rescuePosition = rescueUnit->coor;
            if (hero->coor.Distance(rescuePosition) <= 3.0)
            {
                m_botRescueReached = true;
                BotLog(wxString::Format("Rescued Commando #%d: now extracting.",
                    rescueUnit->id));
            }
        }
    }

    // The bot may see only real player-visible enemies. Estimate the contact
    // zone from currently visible weapon ranges, not hidden map enemy records.
    std::vector<MapUnit*> visibleHostiles;
    double nearestHostile = 1e30;
    for (MapUnit* e : spell_map->units)
        if (e && e->unit && e->is_enemy && !e->isDead() &&
            e->is_visible >= 2 && e->coor.IsSelected() &&
            spell_map->TileIsVisible(e->coor.x, e->coor.y))
        {
            visibleHostiles.push_back(e);
            nearestHostile = (std::min)(nearestHostile,
                hero->coor.Distance(e->coor));
        }
    const bool combatContact = nearestHostile <= 9.0;
    const bool closeDanger = nearestHostile <= 3.5;

    // Let every nearby friendly squad provide supporting fire, not only the
    // mission leader. Keep selection stable until its native fire animation ends.
    auto supportTried = [this](int id) {
        return std::find(m_botSupportTried.begin(), m_botSupportTried.end(), id)
            != m_botSupportTried.end();
    };
    MapUnit* selected = spell_map->GetSelectedUnit();
    if (selected != hero)
    {
        if (selected && selected->unit && !selected->is_enemy &&
            !selected->isDead() && !supportTried(selected->id))
        {
            if (selected->isActive() && selected->GetFireCount() > 0)
                for (MapUnit* e : spell_map->units)
                {
                    if (!e || !e->unit || !e->is_enemy || e->isDead() ||
                        e->is_visible < 2 || !e->coor.IsSelected()) continue;
                    if (selected->coor.Distance(e->coor) >
                        (double)selected->GetFireRange()) continue;
                    if (!spell_map->CanUnitAttack(e)) continue;
                    const int layer = e->unit->isAir() ? 1 : 0;
                    if (spell_map->Attack(e->coor, layer) == 0)
                    {
                        BotLog(wxString::Format("Support unit #%d fired at #%d.",
                            selected->id, e->id));
                        m_botCooldown = 12;
                        m_botNoProgressTurns = 0;
                        return;
                    }
                }
            m_botSupportTried.push_back(selected->id);
        }
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
                enemy->is_visible < 2 || !enemy->coor.IsSelected() ||
                !spell_map->TileIsVisible(enemy->coor.x, enemy->coor.y) ||
                hero->coor.Distance(enemy->coor) > 4.0 ||
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
            BotLog(wxString::Format("Defensive fire at threat #%d.", targetId));
            m_botCooldown = 12;
            m_botNoProgressTurns = 0;
            return;
        }
    }

    // After the commander has handled nearby threats, use the other Alliance
    // squads for suppressive fire wherever their current weapon range allows.
    for (MapUnit* ally : spell_map->units)
    {
        if (!ally || ally == hero || !ally->unit || ally->is_enemy ||
            ally->isDead() || !ally->isActive() || supportTried(ally->id) ||
            ally->panic_turns > 0 || ally->paralyze_turns > 0 ||
            ally->freeze_turns > 0 || ally->GetFireCount() <= 0)
            continue;
        bool inRange = false;
        for (MapUnit* e : spell_map->units)
            if (e && e->unit && e->is_enemy && !e->isDead() &&
                e->is_visible >= 2 && e->coor.IsSelected() &&
                ally->coor.Distance(e->coor) <= (double)ally->GetFireRange() &&
                ally->unit->canAttack(e->unit))
            {
                inRange = true;
                break;
            }
        if (!inRange) continue;
        spell_map->SelectUnit(ally, true);
        m_botCooldown = 12;
        return;
    }

    // Support companies stay in their defensive positions while hostiles
    // approach. Reposition only in relative safety: roaming supporters used
    // to expose themselves to orcs before Alexander even moved.
    // Do not teleport or alter AP; all movement remains native.
    if (!combatContact)
    for (MapUnit* ally : spell_map->units)
    {
        if (!ally || ally == hero || !ally->unit || ally->is_enemy ||
            ally->isDead() || !ally->isActive() || ally->panic_turns > 0 ||
            ally->paralyze_turns > 0 || ally->freeze_turns > 0 ||
            ally->action_points < ally->GetWalkAP() ||
            std::find(m_botSquadMoved.begin(), m_botSquadMoved.end(), ally->id)
                != m_botSquadMoved.end())
            continue;

        // Units ahead of Alexander do not need to retreat just to bunch up.
        const double separation = ally->coor.Distance(hero->coor);
        if (separation <= 3.5)
            continue;

        // Rejoin the main group. A* treats the commander's occupied square as
        // an obstacle and selects the nearest legal reachable approach square.
        const int reserve = (std::max)(0, ally->GetMaxAP() / 3);
        m_botSquadMoved.push_back(ally->id);
        if (spell_map->AutopilotMoveToward(ally, hero->coor, reserve) ||
            spell_map->AutopilotMoveToward(ally, hero->coor, 0))
        {
            BotLog(wxString::Format("Squad #%d moving to support Alexander.",
                ally->id));
            spell_map->ScrollToUnit(ally);
            m_botCooldown = 12;
            if (canvas) canvas->Refresh();
            return;
        }
    }

    // Rescue has precedence over the exit; no hard-coded map coordinates.
    bool headingToRescue = !m_botRescueReached && rescuePosition.IsSelected();
    MapXY goal;
    if (headingToRescue)
        goal = rescuePosition;
    else
    {
        const auto& exits = !spell_map->escape.empty() ?
            spell_map->escape : spell_map->target;
        if (exits.empty())
        {
            StopFirstMissionBot("No extraction squares in mission data.");
            return;
        }
        goal = *std::min_element(exits.begin(), exits.end(),
            [hero](const MapXY& a, const MapXY& b)
            { return hero->coor.Distance(a) < hero->coor.Distance(b); });
    }

    // A tactical round is not a race to spend every AP. Finish a phase while
    // friendly companies can still react to newly approaching enemies.
    auto finishPhase = [this](const wxString& reason, bool intentionalHold)
    {
        ++m_botTurnCount;
        m_botSupportTried.clear();
        m_botSquadMoved.clear();
        m_botHeroMovedThisTurn = false;
        if (intentionalHold)
            ++m_botNoProgressTurns;
        else
            m_botNoProgressTurns = 0;
        BotLog(wxString::Format("Turn %d: ", m_botTurnCount) + reason);
        if (m_botNoProgressTurns >= 12)
        {
            StopFirstMissionBot("No tactical progress for 12 turns.");
            return;
        }
        spell_map->OnHUDswitchEndTurn();
        m_botCooldown = 12;
        UpdateBotStatus();
    };

    // Once the commander has moved this turn, defend the new position. The
    // previous implementation immediately issued another A* order and spent
    // every remaining AP, often walking straight into a waiting ork formation.
    if (m_botHeroMovedThisTurn)
    {
        finishPhase("holding remaining AP for defensive fire.", false);
        return;
    }

    // Estimate exposure using visible enemy weapon ranges and short potential
    // approach moves. The extra radius is a conservative risk margin, NOT an
    // attempt to reveal fog-of-war or precompute hidden enemy AI actions.
    auto exposureAt = [&](MapXY p) -> double
    {
        double danger = 0.0;
        for (MapUnit* enemy : visibleHostiles)
        {
            const double distance = p.Distance(enemy->coor);
            const double range = (double)spell_map->GetEffectiveFireRange(enemy, p);
            const double dangerRadius = (std::max)(2.0, range + 2.5);
            if (distance < dangerRadius)
                danger += (dangerRadius - distance) * 13.0;
            if (distance <= 2.0)
                danger += 35.0;
        }
        return danger;
    };

    const double currentExposure = exposureAt(hero->coor);
    if (!combatContact)
    {
        m_botCurrentStance = 0;
        m_botContactWaitTurns = 0;
    }
    else
    {
        // In contact, do not automatically charge just because a path exists.
        // Wait twice at most for the enemy to approach our reserved-fire line.
        // The cap prevents an immobile defensive enemy from freezing the bot.
        if (!closeDanger && m_botContactWaitTurns < 2)
        {
            ++m_botContactWaitTurns;
            m_botCurrentStance = 1;
            finishPhase("ambush: let the orcs advance into our fire.", true);
            return;
        }
        m_botCurrentStance = 2;
    }

    // Choose a *short* low-risk probe on contact. Evaluate several local
    // endpoints and favor risk reduction over progress when severely threatened.
    // A* remains responsible for terrain, AP, occupied tiles and animation.
    MapXY movementGoal = goal;
    int maxSteps = combatContact ? 2 : 5;
    if (combatContact)
    {
        MapXY best;
        double bestScore = -1e30;
        const int offsets[][2] = {
            {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1},
            {2,0},{-2,0},{0,2},{0,-2}
        };
        for (const auto& delta : offsets)
        {
            const MapXY p(hero->coor.x + delta[0], hero->coor.y + delta[1]);
            if (p.x < 0 || p.y < 0 ||
                p.x >= spell_map->x_size || p.y >= spell_map->y_size ||
                !spell_map->TileIsVisible(p.x, p.y))
                continue;

            const double progress = hero->coor.Distance(goal) - p.Distance(goal);
            const double exposure = exposureAt(p);
            // A wounded/isolated hero must sometimes retreat rather than
            // maximize forward progress. Avoid positions appreciably riskier.
            if (exposure > currentExposure + (closeDanger ? 0.0 : 5.0))
                continue;
            if (progress <= 0.0 && exposure >= currentExposure - 4.0)
                continue;
            const double score = progress * (closeDanger ? 2.0 : 8.0) -
                (exposure - currentExposure) * (closeDanger ? 4.0 : 2.0);
            if (score > bestScore)
            {
                bestScore = score;
                best = p;
            }
        }
        if (!best.IsSelected())
        {
            ++m_botContactWaitTurns;
            m_botCurrentStance = 1;
            finishPhase("defending: no safe approach visible.", true);
            return;
        }
        movementGoal = best;
    }

    const int reserveAP = (std::max)(0, hero->GetMaxAP() / 3);
    if (spell_map->AutopilotMoveToward(hero, movementGoal, reserveAP, maxSteps))
    {
        spell_map->ScrollToUnit(hero);
        m_botHeroMovedThisTurn = true;
        m_botContactWaitTurns = 0;
        m_botNoProgressTurns = 0;
        m_botCooldown = 12;
        BotLog(wxString::Format("%s toward (%d, %d), %d steps max.",
            combatContact ? "Cautious probe" :
                (headingToRescue ? "Rescue advance" : "Extraction advance"),
            movementGoal.x, movementGoal.y, maxSteps));
        if (canvas) canvas->Refresh();
        UpdateBotStatus();
        return;
    }

    // A short safe probe may be blocked by terrain or another friendly unit.
    // Only in a quiet sector may the bot try a direct unrestricted path.
    if (!combatContact &&
        spell_map->AutopilotMoveToward(hero, goal, reserveAP, 5))
    {
        spell_map->ScrollToUnit(hero);
        m_botHeroMovedThisTurn = true;
        m_botNoProgressTurns = 0;
        m_botCooldown = 12;
        BotLog("Advancing toward objective through the free route.");
        if (canvas) canvas->Refresh();
        return;
    }

    m_botCurrentStance = combatContact ? 1 : 0;
    finishPhase(combatContact ? "holding defensively; route blocked." :
        "waiting for the next available move.", true);
}
