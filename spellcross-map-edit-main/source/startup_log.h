#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

// Diagnostics must never change whether a retail data set can be loaded.
// Flush each record before posting GUI events so the last item survives a crash.
namespace spellcross_startup_log
{
    struct Session
    {
        std::mutex mutex;
        std::ofstream stream;
        std::filesystem::path path;
        std::chrono::steady_clock::time_point started;
    };

    inline Session& Current()
    {
        static Session session;
        return session;
    }

    inline std::filesystem::path Begin(const std::filesystem::path& config) noexcept
    {
        try
        {
            auto& session = Current();
            std::lock_guard<std::mutex> lock(session.mutex);
            session.stream.close();
            session.path.clear();
            session.started = std::chrono::steady_clock::now();
            std::error_code ec;
            auto directory = std::filesystem::absolute(config, ec).parent_path();
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                if (attempt == 1)
                {
                    ec.clear();
                    directory = std::filesystem::temp_directory_path(ec);
                }
                if (ec || directory.empty())
                    continue;
                const auto path = directory / "Spellcross-startup.log";
                session.stream.clear();
                session.stream.open(path, std::ios::binary | std::ios::trunc);
                if (!session.stream)
                    continue;
                session.path = path;
                session.stream << "Spellcross data loader started\n";
                session.stream.flush();
                return path;
            }
        }
        catch (...) {} // Logging failure must not fail startup.
        return {};
    }

    inline void Write(const char* kind, const std::string& message) noexcept
    {
        try
        {
            auto& session = Current();
            std::lock_guard<std::mutex> lock(session.mutex);
            if (!session.stream)
                return;
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - session.started).count();
            session.stream << '[' << elapsed << " ms] " << kind << ": ";
            // Retain resource-name bytes; do not run game data through a decoder.
            session.stream.write(message.data(), static_cast<std::streamsize>(message.size()));
            session.stream << '\n';
            session.stream.flush();
        }
        catch (...) {}
    }
}
