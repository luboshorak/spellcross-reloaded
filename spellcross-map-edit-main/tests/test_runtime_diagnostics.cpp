#include "../source/runtime_diagnostics.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

int main()
{
#ifndef _WIN32
    std::cout << "SKIP: Windows runtime log test\n";
    return 0;
#else
    namespace fs = std::filesystem;
    const fs::path directory = fs::temp_directory_path() /
        ("spellcross-diagnostics-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(directory, ec);
    fs::create_directories(directory, ec);
    if (ec) return 1;

    // Packaged releases provide a build fingerprint even in a plain text log.
    {
        std::ofstream info(directory / "BUILD-INFO.txt");
        info << "Git commit: abcdef1234567890\n";
        info << "Spellcross.exe SHA256: deadbeef01234567\n";
    }
    spellcross_diag::Initialize(directory, "diagnostic-test");
    std::vector<std::thread> writers;
    for (int t = 0; t < 4; ++t)
        writers.emplace_back([t]()
        {
            for (int i = 0; i < 750; ++i)
                spellcross_diag::Log(spellcross_diag::Level::Info, "WORKER",
                    "thread=%d sequence=%d %s", t, i,
                    "fixed diagnostic payload for bounded rotation and concurrent writers");
        });
    for (auto& worker : writers)
        worker.join();

    for (int i = 0; i < 2000; ++i)
        spellcross_diag::Log(spellcross_diag::Level::Info, "EVENT",
            "sequence=%d %s", i,
            "fixed diagnostic payload for bounded rotation and concurrent writers");

    spellcross_diag::Log(spellcross_diag::Level::Info, "TEST", "FINAL_MARKER");
    size_t total = 0;
    for (int i = 0; i <= spellcross_diag::kRotatedFiles; ++i)
    {
        const fs::path path = directory / (i == 0 ? "debug.log" :
            ("debug." + std::to_string(i) + ".log"));
        if (!fs::exists(path)) continue;
        const auto size = fs::file_size(path);
        if (size > spellcross_diag::kLogBytes)
        {
            std::cerr << path << " exceeded rotation cap: " << size << '\n';
            return 2;
        }
        total += size;
    }
    if (total > spellcross_diag::kLogBytes * (spellcross_diag::kRotatedFiles + 1))
        return 3;
    const auto active = directory / "debug.log";
    std::ifstream input(active, std::ios::binary);
    const std::string log((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    if (log.find("FINAL_MARKER") == std::string::npos)
        return 4;
    if (total < spellcross_diag::kLogBytes)
        return 5; // The test did not exercise enough rotation.
    // Simulate the Windows unhandled-exception callback without crashing the
    // CI process. This tests the *actual* report and minidump writer.
    {
        std::ofstream oldReport(directory / "crash-last.txt");
        oldReport << "OLD_REPORT";
    }
    {
        std::ofstream oldDump(directory / "crash-last.dmp", std::ios::binary);
        oldDump << "OLD_DUMP";
    }
    auto fail = [](int code, const char* reason) {
        std::cerr << "Runtime diagnostics test failed (" << code << "): " << reason << '\n';
        return code;
    };
    CONTEXT context{};
    RtlCaptureContext(&context);
    EXCEPTION_RECORD record{};
    record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
#ifdef _WIN64
    record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
#else
    record.ExceptionAddress = reinterpret_cast<void*>(context.Eip);
#endif
    record.NumberParameters = 2;
    record.ExceptionInformation[0] = 0; // READ
    record.ExceptionInformation[1] = 0xBAD0;
    EXCEPTION_POINTERS pointers{};
    pointers.ExceptionRecord = &record;
    pointers.ContextRecord = &context;
    if (spellcross_diag::OnUnhandledException(&pointers) != EXCEPTION_CONTINUE_SEARCH)
        return fail(6, "unexpected SEH disposition");
    const fs::path reportPath = directory / "crash-last.txt";
    if (!fs::exists(reportPath)) return fail(7, "crash-last.txt missing");
    std::ifstream reportFile(reportPath, std::ios::binary);
    const std::string report((std::istreambuf_iterator<char>(reportFile)),
        std::istreambuf_iterator<char>());
    if (report.find("0xC0000005") == std::string::npos ||
        report.find("READ address=0xBAD0") == std::string::npos ||
        report.find("abcdef1234567890") == std::string::npos ||
        report.find("Stack (") == std::string::npos ||
        report.find("#00") == std::string::npos)
        { std::cerr << "Report contents: " << report << "\n"; return fail(8, "missing exception, stack, or build metadata"); }
    std::ifstream previous(directory / "crash-prev.txt");
    std::string previousLine;
    std::getline(previous, previousLine);
    if (previousLine != "OLD_REPORT") return fail(9, "crash report rotation incorrect");
    if (!fs::exists(directory / "crash-last.dmp") ||
        fs::file_size(directory / "crash-last.dmp") < 1024)
        return fail(10, "minidump missing or truncated");
    if (!fs::exists(directory / "crash-prev.dmp")) return fail(11, "minidump rotation incorrect");
    std::ifstream updatedLog(active, std::ios::binary);
    const std::string completeLog((std::istreambuf_iterator<char>(updatedLog)),
        std::istreambuf_iterator<char>());
    if (completeLog.find("CRASH_STACK: #00") == std::string::npos ||
        completeLog.find("BUILD: Commit=abcdef1234567890") == std::string::npos)
        return fail(12, "fatal stack or build marker absent from debug.log");
    spellcross_diag::CleanExit();
    fs::remove_all(directory, ec);
    std::cout << "PASS: rotating diagnostics, build fingerprint, crash stack and minidump\n";
    return 0;
#endif
}
