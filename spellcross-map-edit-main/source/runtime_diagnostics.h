#pragma once

// Bounded, thread-safe runtime breadcrumbs for player crash reports.
// No wxWidgets dependency: the Windows SEH and std::terminate handlers can
// record a fatal error even if the UI event loop or a worker is broken.
// Files are next to Spellcross.exe's writable runtime root:
//   debug.log + debug.1.log .. debug.3.log (maximum ~2 MiB combined).
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cwchar>
#include <fstream>
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
#ifdef _WIN32
    // Fixed paths computed during normal startup; fatal handlers must not
    // allocate memory or resolve filesystem paths after a crash.
    wchar_t report_path[MAX_PATH]{};
    wchar_t previous_report_path[MAX_PATH]{};
    wchar_t dump_path[MAX_PATH]{};
    wchar_t previous_dump_path[MAX_PATH]{};
    char symbol_path[2 * MAX_PATH]{};
    char version[64]{};
    char build_commit[80]{};
    char executable_sha256[80]{};
    std::atomic_flag crash_started = ATOMIC_FLAG_INIT;
#endif
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
// Everything below is best-effort: report the original fault first, then
// capture a small dump and stack. DbgHelp can itself fail when memory is damaged.
inline void CrashLine(HANDLE report, const char* category, const char* format, ...) noexcept
{
    char message[880]{};
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (report != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(report, message, static_cast<DWORD>(std::strlen(message)), &written, nullptr);
        WriteFile(report, "\r\n", 2, &written, nullptr);
    }
    Record(Level::Fatal, category, message, true);
}

inline void CrashModule(DWORD64 pc, char* name, size_t name_size, DWORD64& rva) noexcept
{
    std::snprintf(name, name_size, "unknown");
    rva = pc;
    HMODULE module = nullptr;
    if (!pc || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(static_cast<std::uintptr_t>(pc)), &module))
        return;
    wchar_t module_path[MAX_PATH]{};
    if (GetModuleFileNameW(module, module_path, MAX_PATH))
    {
        const wchar_t* filename = std::wcsrchr(module_path, L'\\');
        filename = filename ? filename + 1 : module_path;
        WideCharToMultiByte(CP_UTF8, 0, filename, -1, name,
            static_cast<int>(name_size), nullptr, nullptr);
        name[name_size - 1] = '\0';
    }
    rva = pc - reinterpret_cast<DWORD64>(module);
}

inline void CrashStack(HANDLE report, const CONTEXT* original) noexcept
{
    if (!original)
    {
        CrashLine(report, "CRASH_STACK", "No CPU context available");
        return;
    }
#if defined(_M_X64) || defined(__x86_64__)
    CONTEXT context = *original; // StackWalk64 modifies its CONTEXT
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
    constexpr DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_IX86)
    CONTEXT context = *original;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Eip;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
    constexpr DWORD machine = IMAGE_FILE_MACHINE_I386;
#else
    CrashLine(report, "CRASH_STACK", "Stack walk unavailable on this architecture");
    return;
#endif

    // There is only one crash reporter per process (atomic guard). Never do
    // concurrent DbgHelp symbol/stack operations from other worker threads.
    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SymGetOptions() | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS |
        SYMOPT_LOAD_LINES | SYMOPT_NO_PROMPTS | SYMOPT_FAIL_CRITICAL_ERRORS);
    const bool symbols = SymInitialize(process,
        Current().symbol_path[0] ? Current().symbol_path : nullptr, TRUE) != FALSE;
    if (!symbols)
        CrashLine(report, "CRASH_STACK", "Symbols unavailable; Win32 error=%lu; using module offsets", GetLastError());

    CrashLine(report, "CRASH_STACK", "Stack (faulting context, module+RVA; source lines when matching PDB is installed):");
    DWORD64 previous_pc = 0;
    for (int i = 0; i < 32; ++i)
    {
        const DWORD64 pc = frame.AddrPC.Offset;
        if (!pc || pc == previous_pc)
            break;
        previous_pc = pc;
        char module_name[160]{};
        DWORD64 rva = 0;
        CrashModule(pc, module_name, sizeof(module_name), rva);

        char symbol_text[450]{};
        if (symbols)
        {
            alignas(SYMBOL_INFO) unsigned char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME + 1]{};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            DWORD64 symbol_offset = 0;
            if (SymFromAddr(process, pc, &symbol_offset, symbol))
            {
                std::snprintf(symbol_text, sizeof(symbol_text), " %s+0x%llX",
                    symbol->Name, static_cast<unsigned long long>(symbol_offset));
                IMAGEHLP_LINE64 line{};
                line.SizeOfStruct = sizeof(line);
                DWORD line_offset = 0;
                if (SymGetLineFromAddr64(process, pc, &line_offset, &line) && line.FileName)
                {
                    const size_t used = std::strlen(symbol_text);
                    std::snprintf(symbol_text + used, sizeof(symbol_text) - used,
                        " %s:%lu", line.FileName, line.LineNumber);
                }
            }
        }
        CrashLine(report, "CRASH_STACK", "#%02d %s+0x%llX pc=0x%llX%s",
            i, module_name, static_cast<unsigned long long>(rva),
            static_cast<unsigned long long>(pc), symbol_text);
        if (!symbols)
            break; // Fault PC is still useful even when walking is unavailable.
        if (!StackWalk64(machine, process, GetCurrentThread(), &frame, &context,
            nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
    }
    if (symbols)
        SymCleanup(process);
}

// crash-last.* is rotated only on a fatal error, not every time the game starts.
// Two reports and two minidumps are retained; logs have their own bounded rotation.
inline void CrashArtifacts(EXCEPTION_POINTERS* exception, const char* reason) noexcept
{
    State& state = Current();
    if (!state.enabled || state.crash_started.test_and_set())
        return;

    DeleteFileW(state.previous_report_path);
    MoveFileExW(state.report_path, state.previous_report_path, MOVEFILE_REPLACE_EXISTING);
    DeleteFileW(state.previous_dump_path);
    MoveFileExW(state.dump_path, state.previous_dump_path, MOVEFILE_REPLACE_EXISTING);

    HANDLE report = CreateFileW(state.report_path, GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    CrashLine(report, "CRASH", "Spellcross Reloaded crash report: %s", reason ? reason : "unknown");
    CrashLine(report, "CRASH", "Version=%s commit=%s exe_sha256=%s pid=%lu tid=%lu",
        state.version[0] ? state.version : "unknown",
        state.build_commit[0] ? state.build_commit : "not-packaged",
        state.executable_sha256[0] ? state.executable_sha256 : "not-packaged",
        GetCurrentProcessId(), GetCurrentThreadId());

    CONTEXT fallback{};
    RtlCaptureContext(&fallback);
    CONTEXT* context = exception && exception->ContextRecord ? exception->ContextRecord : &fallback;
    const EXCEPTION_RECORD* record = exception ? exception->ExceptionRecord : nullptr;
    if (record)
    {
        CrashLine(report, "CRASH", "SEH code=0x%08lX fault=%p flags=0x%08lX",
            record->ExceptionCode, record->ExceptionAddress, record->ExceptionFlags);
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
        {
            const ULONG_PTR access = record->ExceptionInformation[0];
            CrashLine(report, "CRASH", "Access violation: %s address=0x%llX",
                access == 0 ? "READ" : access == 1 ? "WRITE" :
                access == 8 ? "EXECUTE" : "UNKNOWN",
                static_cast<unsigned long long>(record->ExceptionInformation[1]));
        }
    }
#ifdef _WIN64
    CrashLine(report, "CRASH", "Registers: RIP=0x%llX RSP=0x%llX RBP=0x%llX RAX=0x%llX RBX=0x%llX RCX=0x%llX RDX=0x%llX",
        static_cast<unsigned long long>(context->Rip), static_cast<unsigned long long>(context->Rsp),
        static_cast<unsigned long long>(context->Rbp), static_cast<unsigned long long>(context->Rax),
        static_cast<unsigned long long>(context->Rbx), static_cast<unsigned long long>(context->Rcx),
        static_cast<unsigned long long>(context->Rdx));
    CrashLine(report, "CRASH", "Registers: RSI=0x%llX RDI=0x%llX R8=0x%llX R9=0x%llX R10=0x%llX R11=0x%llX R12=0x%llX R13=0x%llX R14=0x%llX R15=0x%llX",
        static_cast<unsigned long long>(context->Rsi), static_cast<unsigned long long>(context->Rdi),
        static_cast<unsigned long long>(context->R8), static_cast<unsigned long long>(context->R9),
        static_cast<unsigned long long>(context->R10), static_cast<unsigned long long>(context->R11),
        static_cast<unsigned long long>(context->R12), static_cast<unsigned long long>(context->R13),
        static_cast<unsigned long long>(context->R14), static_cast<unsigned long long>(context->R15));
#endif

    // A normal minidump contains thread stacks, contexts and a module list,
    // not all process memory. It can still contain sensitive game/user strings.
    HANDLE dump = CreateFileW(state.dump_path, GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dump != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION info{};
        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = exception;
        info.ClientPointers = FALSE;
        const MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
            MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
        const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
            dump, type, exception ? &info : nullptr, nullptr, nullptr);
        const DWORD error = ok ? ERROR_SUCCESS : GetLastError();
        FlushFileBuffers(dump);
        CloseHandle(dump);
        if (ok)
            CrashLine(report, "CRASH", "Minidump written: crash-last.dmp");
        else
        {
            DeleteFileW(state.dump_path);
            CrashLine(report, "CRASH", "Minidump FAILED: Windows error=%lu", error);
        }
    }
    else
        CrashLine(report, "CRASH", "Minidump cannot be created: Windows error=%lu", GetLastError());

    CrashStack(report, context);
    CrashLine(report, "CRASH", "Report complete: crash-last.txt; previous crash: crash-prev.txt");
    if (report != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers(report);
        CloseHandle(report);
    }
}

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
    // Write the dump before Windows Error Reporting takes over. The first
    // fatal breadcrumb above survives even if the extra capture fails.
    CrashArtifacts(exception, "unhandled SEH");
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
#ifdef _WIN32
    CrashArtifacts(nullptr, "std::terminate");
#endif
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
#ifdef _WIN32
            auto copy_path = [](wchar_t* target, size_t length, const std::filesystem::path& path) {
                const std::wstring value = path.wstring();
                wcsncpy_s(target, length, value.c_str(), _TRUNCATE);
            };
            copy_path(state.report_path, MAX_PATH, runtime_root / "crash-last.txt");
            copy_path(state.previous_report_path, MAX_PATH, runtime_root / "crash-prev.txt");
            copy_path(state.dump_path, MAX_PATH, runtime_root / "crash-last.dmp");
            copy_path(state.previous_dump_path, MAX_PATH, runtime_root / "crash-prev.dmp");
            const std::wstring search = (runtime_root / "symbols").wstring() + L";" + runtime_root.wstring();
            WideCharToMultiByte(CP_ACP, 0, search.c_str(), -1,
                state.symbol_path, static_cast<int>(sizeof(state.symbol_path)), nullptr, nullptr);
            std::snprintf(state.version, sizeof(state.version), "%s", version ? version : "unknown");
            // Installed releases have a build fingerprint; local developer runs
            // can omit BUILD-INFO.txt and still get module-relative stack offsets.
            std::ifstream build(runtime_root / "BUILD-INFO.txt");
            std::string line;
            while (std::getline(build, line))
            {
                const std::string commit = "Git commit: ";
                const std::string sha = "Spellcross.exe SHA256: ";
                if (line.compare(0, commit.size(), commit) == 0)
                    std::snprintf(state.build_commit, sizeof(state.build_commit), "%s", line.c_str() + commit.size());
                else if (line.compare(0, sha.size(), sha) == 0)
                    std::snprintf(state.executable_sha256, sizeof(state.executable_sha256), "%s", line.c_str() + sha.size());
            }
#endif
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
#ifdef _WIN32
        Log(Level::Info, "BUILD", "Commit=%s exe_sha256=%s symbol_dir=symbols",
            state.build_commit[0] ? state.build_commit : "not-packaged",
            state.executable_sha256[0] ? state.executable_sha256 : "not-packaged");
#endif
    }
    catch (...) {}
}

inline void CleanExit() noexcept
{
    Record(Level::Info, "SESSION", "Game closed normally");
}
} // namespace spellcross_diag
