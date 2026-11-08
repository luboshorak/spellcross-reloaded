#pragma once
// Shared release discovery and hand-off to the separate patch installer.
// A running Spellcross.exe is NEVER overwritten: the PowerShell child waits
// for the game process to exit, then validates and applies the delta.
#include "app_version.h"
#include "release_version_compare.h"
#include <wx/stdpaths.h>
#include <wx/string.h>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <string>
#include <vector>

#ifdef __WXMSW__
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#endif

namespace spellcross_update
{
    using spellcross_release::CompareVersions;
#ifdef __WXMSW__
    class InternetHandle
    {
    public:
        InternetHandle() = default;
        explicit InternetHandle(HINTERNET handle) : m_handle(handle) {}
        ~InternetHandle() { if (m_handle) WinHttpCloseHandle(m_handle); }
        InternetHandle(const InternetHandle&) = delete;
        InternetHandle& operator=(const InternetHandle&) = delete;
        operator HINTERNET() const { return m_handle; }
        bool IsOk() const { return m_handle != nullptr; }
    private:
        HINTERNET m_handle = nullptr;
    };

    inline bool FetchLatestReleaseTag(std::string& tag, std::string& error)
    {
        // Use the release *list*, not /releases/latest: GitHub's "latest" endpoint
        // intentionally skips prereleases, while Spellcross Reloaded public builds
        // are often published as prereleases during development.
        InternetHandle session(WinHttpOpen(
            L"SpellcrossReloaded/" SPELLCROSS_VERSION_WSTRING,
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0));
        if (!session.IsOk())
        {
            error = "Could not initialize the Windows HTTP client.";
            return false;
        }

        WinHttpSetTimeouts(session, 4000, 4000, 5000, 5000);

        InternetHandle connection(WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!connection.IsOk())
        {
            error = "Could not connect to api.github.com.";
            return false;
        }

        InternetHandle request(WinHttpOpenRequest(
            connection,
            L"GET",
            L"/repos/luboshorak/spellcross-reloaded/releases?per_page=20",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE));
        if (!request.IsOk())
        {
            error = "Could not create the GitHub update request.";
            return false;
        }

        const wchar_t* headers =
            L"Accept: application/vnd.github+json\r\n"
            L"X-GitHub-Api-Version: 2022-11-28\r\n"
            L"User-Agent: SpellcrossReloaded/" SPELLCROSS_VERSION_WSTRING L"\r\n";

        if (!WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L),
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request, nullptr))
        {
            error = "GitHub did not respond. Check your internet connection or proxy settings.";
            return false;
        }

        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (!WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX))
        {
            error = "Could not read the GitHub response status.";
            return false;
        }

        if (status != 200)
        {
            error = "GitHub update check failed (HTTP " + std::to_string(status) + ").";
            return false;
        }

        std::string body;
        for (;;)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available))
            {
                error = "Could not read the GitHub response.";
                return false;
            }
            if (available == 0)
                break;

            const size_t oldSize = body.size();
            body.resize(oldSize + available);
            DWORD read = 0;
            if (!WinHttpReadData(request, body.data() + oldSize, available, &read))
            {
                error = "Could not read the GitHub response.";
                return false;
            }
            body.resize(oldSize + read);
        }

        const std::regex tagRegex(R"REGEX("tag_name"\s*:\s*"([^"]+)")REGEX");
        std::sregex_iterator it(body.begin(), body.end(), tagRegex);
        const std::sregex_iterator end;
        if (it == end)
        {
            error = "No published GitHub release was found.";
            return false;
        }

        // GitHub returns releases in publication order, but select the highest
        // comparable numeric tag from the returned page so a later-published
        // hotfix/legacy tag cannot hide a genuinely newer build.
        std::string firstTag;
        std::string bestTag;
        for (; it != end; ++it)
        {
            const std::string candidate = (*it)[1].str();
            if (firstTag.empty())
                firstTag = candidate;

            bool candidateComparable = false;
            CompareVersions(candidate, SPELLCROSS_RELEASE_LABEL, candidateComparable);
            if (!candidateComparable)
                continue;

            if (bestTag.empty())
            {
                bestTag = candidate;
                continue;
            }

            bool tagsComparable = false;
            if (CompareVersions(bestTag, candidate, tagsComparable) < 0 && tagsComparable)
                bestTag = candidate;
        }

        tag = bestTag.empty() ? firstTag : bestTag;
        return !tag.empty();
    }

    inline bool IsPatchClientInstalled()
    {
        const std::filesystem::path folder(wxStandardPaths::Get().GetExecutablePath().ToStdWstring());
        std::error_code ec;
        return std::filesystem::is_regular_file(folder.parent_path() / L"updater" / L"Install-Patch.ps1", ec);
    }

    inline bool StartPatchInstaller(const std::string& targetTag, std::string& error)
    {
        if (!std::regex_match(targetTag, std::regex(R"(v\d+\.\d+\.\d+(?:\.\d+)?(?:-(?:RC|beta)\d+)?)")))
        {
            error = "Unsupported release tag.";
            return false;
        }
        const auto executable = std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring());
        const auto root = executable.parent_path();
        const auto script = root / L"updater" / L"Install-Patch.ps1";
        if (!IsPatchClientInstalled())
        {
            error = "The updater is not included in this installation. Download a complete build once.";
            return false;
        }
        // Paths cannot contain double quotes on Windows. Tags are restricted
        // to alphanumeric/dot/dash above. Arguments never pass through cmd.exe.
        const std::wstring args =
            L"-NoProfile -ExecutionPolicy Bypass -File \"" + script.wstring() +
            L"\" -InstallRoot \"" + root.wstring() +
            L"\" -CurrentVersion \"" + std::wstring(L"" SPELLCROSS_RELEASE_LABEL) +
            L"\" -TargetVersion \"" + std::wstring(targetTag.begin() + 1, targetTag.end()) +
            L"\" -WaitPid " + std::to_wstring(GetCurrentProcessId());
        const HINSTANCE launched = ShellExecuteW(nullptr, L"open", L"powershell.exe",
            args.c_str(), root.c_str(), SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(launched) <= 32)
        {
            error = "Could not start the Windows patch installer (PowerShell).";
            return false;
        }
        return true;
    }
#else
    inline bool FetchLatestReleaseTag(std::string&, std::string& error)
    {
        error = "Update checking requires Windows.";
        return false;
    }
    inline bool IsPatchClientInstalled() { return false; }
    inline bool StartPatchInstaller(const std::string&, std::string& error)
    {
        error = "Patching requires Windows.";
        return false;
    }
#endif
}
