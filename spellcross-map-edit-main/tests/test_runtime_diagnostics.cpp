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
    spellcross_diag::CleanExit();
    fs::remove_all(directory, ec);
    std::cout << "PASS: concurrent logging, final breadcrumb and bounded rotation\n";
    return 0;
#endif
}
