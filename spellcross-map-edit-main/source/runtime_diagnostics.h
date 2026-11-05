#pragma once

// Bounded, thread-safe runtime breadcrumbs for player crash reports.
// No wxWidgets dependency: the Windows SEH and std::terminate handlers can
// record a fatal error even if the UI event loop or a worker is broken.
// Files are next to Spellcross.exe's writable runtime root:
//   debug.log + debug.1.log .. debug.3.log (maximum ~2 MiB combined).
#ifdef _WIN32
#include <windows.h>
#endif
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <mutex>
#include <string>

namespace spellcross_diag
{
enum class Level { Info, Warning, Error, Fatal };
constexpr std::uint64_t kLogBytes = 512u * 1024u;
constexpr int kRotatedFiles = 3;

struct State
{
    std::mutex mutex;
    std::filesystem::path log_path;
    std::uint64_t bytes = 0;
    bool enabled = false;
};

inline State& Current() noexcept
{
    // Intentionally process-lifetime: fatal handlers may run during teardown.
    static State* state = new State;
    return *state;
}

inline const char* LevelName(Level level) noexcept
{
    switch (level)
    {
    case Level::Warning: return "WARN";
    case Level::Error: return "ERROR";
    case Level::Fatal: return "FATAL";
    default: return "INFO";
    }
}

inline void Rotate(State& state) noexcept
{
    try
    {
        namespace fs = std::filesystem;
        const fs::path dir = state.log_path.parent_path();
        std::error_code ec;
        for (int n = kRotatedFiles; n >= 1; --n)
        {
            const fs::path destination = dir / ("debug." + std::to_string(n) + ".log");
            const fs::path source = n == 1 ? state.log_path :
                dir / ("debug." + std::to_string(n - 1) + ".log");
            fs::remove(destination, ec);
            ec.clear();
            if (fs::exists(source, ec) && !ec)
                fs::rename(source, destination, ec);
            ec.clear();
        }
        state.bytes = fs::exists(state.log_path, ec) && !ec ?
            fs::file_size(state.log_path, ec) : 0;
        if (ec) state.bytes = 0;
    }
    catch (...) {} // Logging must never crash the game.
}

inline void WriteLine(const char* entry, bool fatal = false, bool bypass_lock = false) noexcept
{
#ifdef _WIN32
    try
    {
        auto& state = Current();
        if (!state.enabled || !entry)
            return;
        std::unique_lock<std::mutex> guard(state.mutex, std::defer_lock);
        if (!bypass_lock)
            guard.lock();
        const auto length = static_cast<DWORD>(std::strlen(entry));
        if (!bypass_lock && state.bytes + length > kLogBytes)
            Rotate(state);
        const HANDLE handle = CreateFileW(state.log_path.c_str(), FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return;
        DWORD written = 0;
        const BOOL ok = WriteFile(handle, entry, length, &written, nullptr);
        if (fatal) FlushFileBuffers(handle);
        CloseHandle(handle);
        if (ok && !bypass_lock)
            state.bytes += written;
    }
    catch (...) {}
#else
    (void)entry; (void)fatal; (void)bypass_lock;
#endif
}

inline void Record(Level level, const char* category, const char* message,
    bool emergency = false) noexcept
{
#ifdef _WIN32
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char safe[700]{};
    if (message)
    {
        size_t i = 0;
        for (; i + 1 < sizeof(safe) && message[i]; ++i)
            safe[i] = (message[i] == '\r' || message[i] == '\n') ? ' ' : message[i];
        safe[i] = '\0';
    }
    char entry[1000]{};
    std::snprintf(entry, sizeof(entry),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [%s] pid=%lu tid=%lu %s: %s\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
        now.wSecond, now.wMilliseconds, LevelName(level),
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(GetCurrentThreadId()),
        category ? category : "GENERAL", safe);
    WriteLine(entry, level == Level::Fatal || level == Level::Error, emergency);
#else
    (void)level; (void)category; (void)message; (void)emergency;
#endif
}

template <typename... Args>
inline void Log(Level level, const char* category, const char* format, Args... args) noexcept
{
    char message[700]{};
    if (format)
        std::snprintf(message, sizeof(message), format, args...);
    Record(level, category, message);
}

template <typename... Args>
inline void Emergency(const char* category, const char* format, Args... args) noexcept
{
    char message[700]{};
    if (format)
        std::snprintf(message, sizeof(message), format, args...);
    // Do not wait on a mutex that might have been held by the crashing thread.
    Record(Level::Fatal, category, message, true);
}

#ifdef _WIN32
inline LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* exception) noexcept
{
    const unsigned int code = exception && exception->ExceptionRecord ?
        exception->ExceptionRecord->ExceptionCode : 0u;
    const void* address = exception && exception->ExceptionRecord ?
        exception->ExceptionRecord->ExceptionAddress : nullptr;
    HMODULE module = nullptr;
    if (address)
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &module);
    const unsigned long long rva = module && address ?
        static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(address) -
            reinterpret_cast<std::uintptr_t>(module)) : 0ull;
#ifdef _WIN64
    const unsigned long long ip = exception && exception->ContextRecord ?
        static_cast<unsigned long long>(exception->ContextRecord->Rip) : 0ull;
    Emergency("CRASH",
        "Unhandled SEH code=0x%08X address=%p module_base=%p RVA=0x%llX RIP=0x%llX",
        code, address, module, rva, ip);
#else
    Emergency("CRASH",
        "Unhandled SEH code=0x%08X address=%p module_base=%p RVA=0x%llX",
        code, address, module, rva);
#endif
    // Let Windows Error Reporting produce the normal error report/dump.
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

inline void OnTerminate() noexcept
{
    try
    {
        const auto current = std::current_exception();
        if (current)
            std::rethrow_exception(current);
        Emergency("CRASH", "std::terminate without active C++ exception");
    }
    catch (const std::exception& ex)
    {
        Emergency("CRASH", "Uncaught C++ exception in thread: %s", ex.what());
    }
    catch (...)
    {
        Emergency("CRASH", "Uncaught non-standard C++ exception in thread");
    }
    std::abort();
}

inline void Initialize(const std::filesystem::path& runtime_root, const char* version) noexcept
{
    try
    {
        auto& state = Current();
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.log_path = runtime_root / "debug.log";
            std::error_code ec;
            std::filesystem::create_directories(runtime_root, ec);
            if (ec)
                return;
            state.bytes = std::filesystem::exists(state.log_path, ec) && !ec ?
                std::filesystem::file_size(state.log_path, ec) : 0;
            if (ec) state.bytes = 0;
            state.enabled = true;
        }
        std::set_terminate(&OnTerminate);
#ifdef _WIN32
        SetUnhandledExceptionFilter(&OnUnhandledException);
#endif
        Log(Level::Info, "SESSION", "Game started; version=%s; bounded log=512KiB x 4",
            version ? version : "unknown");
    }
    catch (...) {}
}

inline void CleanExit() noexcept
{
    Record(Level::Info, "SESSION", "Game closed normally");
}
} // namespace spellcross_diag
