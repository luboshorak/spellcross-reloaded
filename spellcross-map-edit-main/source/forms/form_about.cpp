#include "form_about.h"
#include "app_identity.h"
#include "app_version.h"
#include "../strategic_ui_theme.h"
#include "../release_version_compare.h"
#include "../release_update.h"
#include <wx/app.h>
#include <wx/msgdlg.h>

#include <wx/bitmap.h>
#include <wx/panel.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/settings.h>
#include <wx/accel.h>
#include <wx/statbmp.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/utils.h>

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <vector>

#ifdef __WXMSW__
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace
{
    constexpr int ID_CHECK_UPDATES = wxID_HIGHEST + 401;
    constexpr int ID_OPEN_PROJECT = wxID_HIGHEST + 402;
    constexpr int ID_OPEN_RELEASE = wxID_HIGHEST + 403;
    constexpr int ID_INSTALL_PATCH = wxID_HIGHEST + 404;

    using spellcross_release::CompareVersions;

#ifdef __WXMSW__

#endif

    wxStaticText* AddInfoRow(wxWindow* parent, wxFlexGridSizer* grid,
        const wxString& label, const wxString& value)
    {
        auto* key = new wxStaticText(parent, wxID_ANY, label);
        wxFont keyFont = key->GetFont();
        keyFont.SetWeight(wxFONTWEIGHT_BOLD);
        key->SetFont(keyFont);
        grid->Add(key, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, 10);

        auto* val = new wxStaticText(parent, wxID_ANY, value);
        grid->Add(val, 1, wxEXPAND | wxBOTTOM, 10);
        return val;
    }
}

FormAbout::FormAbout(wxWindow* parent, wxWindowID id, const wxString& title,
    const wxPoint& pos, const wxSize& size, long style)
    : wxDialog(parent, id, title, pos, size, style)
{
    spellcross_app::ApplyWindowIcon(this);
    SetMinSize(wxSize(650, 480));
    SetBackgroundColour(spellcross_menu::Background());
    SetForegroundColour(spellcross_menu::Text());

    auto* root = new wxBoxSizer(wxVERTICAL);

    // Header -----------------------------------------------------------------
    auto* header = new wxPanel(this, wxID_ANY);
    header->SetBackgroundColour(spellcross_menu::Highlight());
    auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);

    wxIcon icon;
    icon.LoadFile("IDI_ICON2", wxBITMAP_TYPE_ICO_RESOURCE);
    if (icon.IsOk())
    {
        auto* logo = new wxStaticBitmap(header, wxID_ANY, wxBitmap(icon));
        headerSizer->Add(logo, 0, wxALIGN_CENTER_VERTICAL | wxALL, 18);
    }

    auto* titleSizer = new wxBoxSizer(wxVERTICAL);
    auto* appName = new wxStaticText(header, wxID_ANY, "Spellcross Reloaded");
    wxFont titleFont = appName->GetFont();
    titleFont.SetPointSize(20);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    appName->SetFont(titleFont);
    titleSizer->Add(appName, 0, wxBOTTOM, 3);

    auto* subtitle = new wxStaticText(header, wxID_ANY,
        "Fan-made restoration of Spellcross: The Last Battle");
    wxFont subtitleFont = subtitle->GetFont();
    subtitleFont.SetPointSize(10);
    subtitle->SetFont(subtitleFont);
    titleSizer->Add(subtitle, 0, wxBOTTOM, 8);

    const wxString buildLabel = "Version " + spellcross_app::VersionLabel() +
        "   |   build " + wxString::FromUTF8(__DATE__);
    auto* version = new wxStaticText(header, wxID_ANY, buildLabel);
    version->SetForegroundColour(spellcross_strategic_ui::Inactive());
    titleSizer->Add(version, 0);

    headerSizer->Add(titleSizer, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 18);
    header->SetSizer(headerSizer);
    root->Add(header, 0, wxEXPAND);

    auto* separatorTop = new wxPanel(this, wxID_ANY, wxDefaultPosition,
        wxSize(-1, FromDIP(2)), wxBORDER_NONE);
    separatorTop->SetBackgroundColour(spellcross_menu::Accent());
    root->Add(separatorTop, 0, wxEXPAND);

    // Description -------------------------------------------------------------
    auto* content = new wxPanel(this, wxID_ANY);
    content->SetBackgroundColour(spellcross_menu::Background());
    auto* contentSizer = new wxBoxSizer(wxVERTICAL);

    auto* description = new wxStaticText(content, wxID_ANY,
        "Spellcross Reloaded rebuilds the original campaign and tactical game on top of "
        "the Spellcross Map Editor codebase, while continuing to use the original game data. "
        "The project is experimental, community-driven and still under active development.");
    description->Wrap(600);
    contentSizer->Add(description, 0, wxEXPAND | wxALL, 16);

    // Project details ----------------------------------------------------------
    auto* projectBox = new wxBoxSizer(wxVERTICAL);
    auto* projectHeading = new wxStaticText(content, wxID_ANY, "Project");
    wxFont projectFont = spellcross_menu::MenuFont();
    projectFont.SetWeight(wxFONTWEIGHT_BOLD);
    projectHeading->SetFont(projectFont);
    projectHeading->SetForegroundColour(spellcross_strategic_ui::Heading());
    projectBox->Add(projectHeading, 0, wxLEFT | wxTOP | wxBOTTOM, FromDIP(9));
    auto* grid = new wxFlexGridSizer(2, 8, 12);
    grid->AddGrowableCol(1, 1);
    AddInfoRow(content, grid, "Author", wxString::FromUTF8("Luboš Horák"));
    AddInfoRow(content, grid, "Version", spellcross_app::VersionLabel());
    AddInfoRow(content, grid, "Repository", "github.com/luboshorak/spellcross-reloaded");
    AddInfoRow(content, grid, "Based on", wxString::FromUTF8("Spellcross Map Editor by Stanislav Mašláň"));
    AddInfoRow(content, grid, "License", "MIT License");
    projectBox->Add(grid, 0, wxEXPAND | wxALL, 10);

    auto* projectButtons = new wxBoxSizer(wxHORIZONTAL);
    auto* openProject = new spellcross_strategic_ui::Button(content, ID_OPEN_PROJECT, "Open project on GitHub");
    projectButtons->Add(openProject, 0, wxRIGHT, 8);
    projectBox->Add(projectButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    contentSizer->Add(projectBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    // Update section -----------------------------------------------------------
    auto* updateBox = new wxBoxSizer(wxVERTICAL);
    auto* updateHeading = new wxStaticText(content, wxID_ANY, "Updates");
    updateHeading->SetFont(projectFont);
    updateHeading->SetForegroundColour(spellcross_strategic_ui::Heading());
    updateBox->Add(updateHeading, 0, wxLEFT | wxTOP | wxBOTTOM, FromDIP(9));
    m_updateStatus = new wxStaticText(content, wxID_ANY,
        "Not checked. Use the button below to compare this build with GitHub releases.");
    m_updateStatus->Wrap(590);
    updateBox->Add(m_updateStatus, 0, wxEXPAND | wxALL, 10);

    auto* updateButtons = new wxBoxSizer(wxHORIZONTAL);
    m_checkUpdates = new spellcross_strategic_ui::Button(content, ID_CHECK_UPDATES, "Check for updates");
    m_openRelease = new spellcross_strategic_ui::Button(content, ID_OPEN_RELEASE, "Open GitHub releases");
    m_installPatch = new spellcross_strategic_ui::Button(content, ID_INSTALL_PATCH, "Install patch");
    m_installPatch->Disable();
    updateButtons->Add(m_checkUpdates, 0, wxRIGHT, 8);
    updateButtons->Add(m_openRelease, 0, wxRIGHT, 8);
    updateButtons->Add(m_installPatch, 0);
    updateBox->Add(updateButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    contentSizer->Add(updateBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    content->SetSizer(contentSizer);
    root->Add(content, 1, wxEXPAND);

    auto* separatorBottom = new wxPanel(this, wxID_ANY, wxDefaultPosition,
        wxSize(-1, FromDIP(2)), wxBORDER_NONE);
    separatorBottom->SetBackgroundColour(spellcross_menu::Accent());
    root->Add(separatorBottom, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));
    auto* closeRow = new wxBoxSizer(wxHORIZONTAL);
    closeRow->AddStretchSpacer(1);
    auto* close = new spellcross_strategic_ui::Button(this, wxID_OK, "Close");
    close->SetDefault();
    closeRow->Add(close, 0);
    root->Add(closeRow, 0, wxEXPAND | wxALL, 12);

    SetSizerAndFit(root);
    spellcross_strategic_ui::Apply(this);
    // Preserve the distinctive large heading after applying the common font.
    wxFont appFont = spellcross_menu::MenuFont();
    appFont.SetPointSize(20);
    appFont.SetWeight(wxFONTWEIGHT_BOLD);
    appName->SetFont(appFont);
    appName->SetForegroundColour(spellcross_strategic_ui::Heading());
    header->SetBackgroundColour(spellcross_menu::Highlight());
    version->SetForegroundColour(spellcross_strategic_ui::Inactive());
    projectHeading->SetForegroundColour(spellcross_strategic_ui::Heading());
    updateHeading->SetForegroundColour(spellcross_strategic_ui::Heading());
    Layout();
    if (GetSize().GetWidth() < 680)
        SetSize(wxSize(680, GetSize().GetHeight()));
    CentreOnParent();

    Bind(wxEVT_BUTTON, &FormAbout::OnClose, this, wxID_OK);
    Bind(wxEVT_BUTTON, &FormAbout::OnCheckUpdates, this, ID_CHECK_UPDATES);
    Bind(wxEVT_BUTTON, &FormAbout::OnOpenProject, this, ID_OPEN_PROJECT);
    Bind(wxEVT_BUTTON, &FormAbout::OnOpenRelease, this, ID_OPEN_RELEASE);
    Bind(wxEVT_BUTTON, &FormAbout::OnInstallPatch, this, ID_INSTALL_PATCH);

    std::vector<wxAcceleratorEntry> entries;
    entries.emplace_back(wxACCEL_NORMAL, WXK_ESCAPE, wxID_OK);
    SetAcceleratorTable(wxAcceleratorTable(entries.size(), entries.data()));
}

void FormAbout::OnClose(wxCommandEvent&)
{
    EndModal(wxID_OK);
}

void FormAbout::OnOpenProject(wxCommandEvent&)
{
    wxLaunchDefaultBrowser(wxString::FromUTF8(SPELLCROSS_GITHUB_URL));
}

void FormAbout::OnOpenRelease(wxCommandEvent&)
{
    const wxString url = m_latestReleaseUrl.empty()
        ? wxString::FromUTF8(SPELLCROSS_GITHUB_RELEASES_URL)
        : m_latestReleaseUrl;
    wxLaunchDefaultBrowser(url);
}

void FormAbout::OnCheckUpdates(wxCommandEvent&)
{
    m_checkUpdates->Disable();
    m_latestReleaseTag.clear();
    m_installPatch->Disable();
    m_updateStatus->SetForegroundColour(spellcross_menu::Text());
    m_updateStatus->SetLabel("Checking GitHub releases...");
    m_updateStatus->GetParent()->Layout();
    wxYieldIfNeeded();

    wxBusyCursor busy;

#ifdef __WXMSW__
    std::string latestTag;
    std::string error;
    if (!spellcross_update::FetchLatestReleaseTag(latestTag, error))
    {
        m_updateStatus->SetForegroundColour(wxColour(238, 108, 92));
        m_updateStatus->SetLabel("Update check failed: " + wxString::FromUTF8(error.c_str()));
        m_checkUpdates->Enable();
        m_updateStatus->GetParent()->Layout();
        return;
    }

    const std::string releaseUrl =
        std::string(SPELLCROSS_GITHUB_RELEASES_URL) + "/tag/" + latestTag;
    m_latestReleaseUrl = wxString::FromUTF8(releaseUrl.c_str());

    bool comparable = false;
    const int cmp = CompareVersions(SPELLCROSS_RELEASE_LABEL, latestTag, comparable);
    const wxString latest = wxString::FromUTF8(latestTag.c_str());

    if (!comparable)
    {
        m_updateStatus->SetForegroundColour(spellcross_strategic_ui::Heading());
        m_updateStatus->SetLabel("Latest GitHub release: " + latest +
            ". The tag is not a standard numeric version, so it cannot be compared automatically.");
        m_openRelease->SetLabel("Open " + latest);
    }
    else if (cmp < 0)
    {
        m_updateStatus->SetForegroundColour(spellcross_menu::Accent());
        m_updateStatus->SetLabel("A newer build is available: " + latest +
            " (installed: " + spellcross_app::VersionLabel() + ").");
        m_openRelease->SetLabel("Open " + latest + " on GitHub");
        if (spellcross_update::IsPatchClientInstalled())
        {
            m_latestReleaseTag = latestTag;
            m_installPatch->Enable();
        }
        else
        {
            m_updateStatus->SetLabel(m_updateStatus->GetLabel() +
                " Patch installer is unavailable in this build; download the full release.");
        }
    }
    else if (cmp == 0)
    {
        m_updateStatus->SetForegroundColour(spellcross_menu::Accent());
        m_updateStatus->SetLabel("You are up to date. Latest GitHub release: " + latest + ".");
        m_openRelease->SetLabel("Open " + latest + " on GitHub");
    }
    else
    {
        m_updateStatus->SetForegroundColour(wxColour(115, 200, 235));
        m_updateStatus->SetLabel("This build (" + spellcross_app::VersionLabel() +
            ") is newer than the latest public GitHub release (" + latest + ").");
        m_openRelease->SetLabel("Open latest public release");
    }
#else
    m_updateStatus->SetForegroundColour(wxColour(238, 108, 92));
    m_updateStatus->SetLabel("Automatic update checking is currently available in Windows builds.");
#endif

    m_checkUpdates->Enable();
    m_updateStatus->GetParent()->Layout();
}

void FormAbout::OnInstallPatch(wxCommandEvent&)
{
#ifdef __WXMSW__
    if (m_latestReleaseTag.empty())
        return;
    wxMessageDialog confirm(this,
        "Install the available patch now?\n\nThe game will close, the updater will download and verify changed files, "
        "and Spellcross Reloaded will restart. Your saved games and imported original data are preserved.",
        "Spellcross Reloaded update", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    confirm.SetYesNoLabels("Install patch", "Not now");
    if (confirm.ShowModal() != wxID_YES)
        return;
    std::string error;
    if (!spellcross_update::StartPatchInstaller(m_latestReleaseTag, error))
    {
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Update could not start", wxOK | wxICON_ERROR, this);
        return;
    }
    EndModal(wxID_APPLY);
    if (wxTheApp)
        wxTheApp->ExitMainLoop();
#endif
}
