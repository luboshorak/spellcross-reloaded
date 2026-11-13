#pragma once
// Full-release updater. Never patches a running executable or touches game data.
// A confirmed GitHub SFX installer is downloaded to a temporary directory,
// then launched so the player can choose the installation folder.
#include "app_version.h"
#include "release_version_compare.h"
#include "spell_ui_locale.h"

#include <wx/progdlg.h>
#include <wx/stdpaths.h>
#include <wx/window.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_set>

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
        explicit InternetHandle(HINTERNET h = nullptr) : m_handle(h) {}
        ~InternetHandle() { if (m_handle) WinHttpCloseHandle(m_handle); }
        InternetHandle(const InternetHandle&) = delete;
        InternetHandle& operator=(const InternetHandle&) = delete;
        operator HINTERNET() const { return m_handle; }
        bool IsOk() const { return m_handle != nullptr; }
    private:
        HINTERNET m_handle;
    };

    inline bool IsSupportedReleaseTag(const std::string& tag)
    {
        static const std::regex pattern(
            R"(v\d+\.\d+\.\d+(?:\.\d+)?(?:-(?:RC|beta)\d+)?)");
        return std::regex_match(tag, pattern);
    }

    inline std::wstring WidenAscii(const std::string& value)
    {
        return std::wstring(value.begin(), value.end());
    }

    inline bool ReadResponse(HINTERNET request, std::string& body, std::string& error)
    {
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
        {
            error = "Could not read GitHub's HTTP response.";
            return false;
        }
        if (status != 200)
        {
            error = "GitHub returned HTTP " + std::to_string(status) + ".";
            return false;
        }

        // A small release JSON page should never require unbounded memory.
        constexpr size_t kMaxResponse = 8 * 1024 * 1024;
        std::array<char, 32768> buffer{};
        for (;;)
        {
            DWORD received = 0;
            if (!WinHttpReadData(request, buffer.data(),
                static_cast<DWORD>(buffer.size()), &received))
            {
                error = "Could not read the GitHub release list.";
                return false;
            }
            if (received == 0)
                break;
            if (body.size() + received > kMaxResponse)
            {
                error = "GitHub release metadata was unexpectedly large.";
                return false;
            }
            body.append(buffer.data(), received);
        }
        return true;
    }

    inline bool FetchLatestReleaseTag(std::string& tag, std::string& error)
    {
        // Consider published stable and prerelease builds, but only those
        // that actually contain the expected complete Windows SFX installer.
        tag.clear();
        InternetHandle session(WinHttpOpen(
            L"SpellcrossReloaded/" SPELLCROSS_VERSION_WSTRING,
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.IsOk())
        {
            error = "Could not initialize the Windows HTTP client.";
            return false;
        }
        WinHttpSetTimeouts(session, 4000, 4000, 5000, 5000);
        InternetHandle connection(WinHttpConnect(session, L"api.github.com",
            INTERNET_DEFAULT_HTTPS_PORT, 0));
        InternetHandle request(connection.IsOk() ? WinHttpOpenRequest(
            connection, L"GET",
            L"/repos/luboshorak/spellcross-reloaded/releases?per_page=20",
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE) : nullptr);
        if (!request.IsOk())
        {
            error = "Could not connect to api.github.com.";
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
            error = "GitHub did not respond.";
            return false;
        }

        std::string body;
        if (!ReadResponse(request, body, error))
            return false;

        const std::regex tagRegex(R"REGEX("tag_name"\s*:\s*"([^"]+)")REGEX");
        const std::regex assetRegex(R"REGEX("name"\s*:\s*"(Spellcross_Reloaded_v[^"]+\.exe)")REGEX");
        const std::sregex_iterator end;
        std::unordered_set<std::string> installers;
        for (auto it = std::sregex_iterator(body.begin(), body.end(), assetRegex);
             it != end; ++it)
            installers.insert((*it)[1].str());

        // Do not offer a tag without the actual .exe download asset.
        for (auto it = std::sregex_iterator(body.begin(), body.end(), tagRegex);
             it != end; ++it)
        {
            const std::string candidate = (*it)[1].str();
            if (!IsSupportedReleaseTag(candidate) ||
                !installers.count("Spellcross_Reloaded_" + candidate + ".exe"))
                continue;

            bool candidateComparable = false;
            CompareVersions(candidate, SPELLCROSS_RELEASE_LABEL, candidateComparable);
            if (!candidateComparable)
                continue;

            if (tag.empty())
            {
                tag = candidate;
                continue;
            }
            bool comparable = false;
            if (CompareVersions(tag, candidate, comparable) < 0 && comparable)
                tag = candidate;
        }
        if (tag.empty())
        {
            error = "No published GitHub release with a full Windows installer was found.";
            return false;
        }
        return true;
    }

    inline bool DownloadAndLaunchFullRelease(const std::string& tag,
        wxWindow* parent, std::string& error)
    {
        if (!IsSupportedReleaseTag(tag))
        {
            error = "Unsupported release tag.";
            return false;
        }

        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path downloadDir = fs::temp_directory_path(ec) /
            "SpellcrossReloadedUpdates" / tag;
        if (ec)
        {
            error = "Could not locate the download directory.";
            return false;
        }
        fs::create_directories(downloadDir, ec);
        if (ec)
        {
            error = "Could not create the download directory: " + ec.message();
            return false;
        }
        const std::wstring fileName =
            L"Spellcross_Reloaded_" + WidenAscii(tag) + L".exe";
        const fs::path installer = downloadDir / fileName;
        const fs::path temporary = downloadDir / (fileName + L".part");

        InternetHandle session(WinHttpOpen(
            L"SpellcrossReloaded/" SPELLCROSS_VERSION_WSTRING,
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.IsOk())
        {
            error = "Could not initialize the Windows HTTP client.";
            return false;
        }
        WinHttpSetTimeouts(session, 8000, 8000, 20000, 20000);
        InternetHandle connection(WinHttpConnect(session, L"github.com",
            INTERNET_DEFAULT_HTTPS_PORT, 0));
        const std::wstring path = L"/luboshorak/spellcross-reloaded/releases/download/" +
            WidenAscii(tag) + L"/" + fileName;
        InternetHandle request(connection.IsOk() ? WinHttpOpenRequest(
            connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr);
        if (!request.IsOk() || !WinHttpSendRequest(request,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request, nullptr))
        {
            error = "Could not connect to the GitHub release download.";
            return false;
        }

        DWORD status = 0, statusSize = sizeof(status);
        if (!WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &statusSize, WINHTTP_NO_HEADER_INDEX) || status != 200)
        {
            error = "The full release installer is unavailable (HTTP " +
                std::to_string(status) + ").";
            return false;
        }

        unsigned long long expected = 0;
        wchar_t lengthText[64]{};
        DWORD lengthSize = sizeof(lengthText);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
            WINHTTP_HEADER_NAME_BY_INDEX, lengthText, &lengthSize,
            WINHTTP_NO_HEADER_INDEX))
            expected = _wcstoui64(lengthText, nullptr, 10);

        // Reject an obviously invalid/oversized executable or server response.
        constexpr unsigned long long kMinInstaller = 1024 * 1024;
        constexpr unsigned long long kMaxInstaller = 2ULL * 1024 * 1024 * 1024;
        if (expected && (expected < kMinInstaller || expected > kMaxInstaller))
        {
            error = "GitHub returned an unexpected installer size.";
            return false;
        }

        fs::remove(temporary, ec);
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Could not open the download file for writing.";
            return false;
        }

        wxProgressDialog progress(
            spellcross_ui_locale::Tr("updates.download_title", "Downloading Spellcross Reloaded"),
            spellcross_ui_locale::Tr("updates.downloading", "Downloading the complete installer..."),
            100, parent, wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_AUTO_HIDE);
        std::array<char, 65536> buffer{};
        unsigned long long receivedTotal = 0;
        int lastPercent = -1;
        bool ok = true;
        for (;;)
        {
            DWORD received = 0;
            if (!WinHttpReadData(request, buffer.data(),
                static_cast<DWORD>(buffer.size()), &received))
            {
                error = "Download interrupted by a network error.";
                ok = false;
                break;
            }
            if (!received)
                break;
            receivedTotal += received;
            if (receivedTotal > kMaxInstaller)
            {
                error = "The release installer is unexpectedly large.";
                ok = false;
                break;
            }
            output.write(buffer.data(), received);
            if (!output)
            {
                error = "Could not save the downloaded installer.";
                ok = false;
                break;
            }

            const int percent = expected
                ? static_cast<int>((std::min)(99ULL, (receivedTotal * 100) / expected))
                : 0;
            if (percent != lastPercent || (!expected && receivedTotal % (1024 * 1024) < buffer.size()))
            {
                lastPercent = percent;
                const wxString message = spellcross_ui_locale::Tr(
                    "updates.downloading", "Downloading the complete installer...") +
                    wxString::Format("  %llu MB", receivedTotal / (1024 * 1024));
                if (!progress.Update(percent, message))
                {
                    error = "Download cancelled.";
                    ok = false;
                    break;
                }
            }
        }
        output.close();
        if (ok && (receivedTotal < kMinInstaller ||
            (expected && receivedTotal != expected)))
        {
            error = "The installer download is incomplete.";
            ok = false;
        }
        if (ok)
        {
            std::ifstream verify(temporary, std::ios::binary);
            char mz[2] = {};
            verify.read(mz, 2);
            if (!verify || mz[0] != 'M' || mz[1] != 'Z')
            {
                error = "The downloaded release is not a Windows executable.";
                ok = false;
            }
        }
        if (!ok)
        {
            fs::remove(temporary, ec);
            return false;
        }

        progress.Update(100);
        fs::remove(installer, ec);
        ec.clear();
        fs::rename(temporary, installer, ec);
        if (ec)
        {
            error = "Could not finalize the installer download: " + ec.message();
            fs::remove(temporary, ec);
            return false;
        }

        // The full SFX prompts for an install folder. The game exits after
        // launch so the installer can overwrite the old Spellcross.exe.
        const std::wstring target = installer.wstring();
        const HINSTANCE launched = ShellExecuteW(nullptr, L"open",
            target.c_str(), nullptr, downloadDir.wstring().c_str(), SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(launched) <= 32)
        {
            error = "Could not launch the downloaded installer.";
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
    inline bool DownloadAndLaunchFullRelease(const std::string&,
        wxWindow*, std::string& error)
    {
        error = "The Windows installer is unavailable on this platform.";
        return false;
    }
#endif
}
