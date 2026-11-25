#include "../source/startup_log.h"
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>

static std::string Read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    const auto directory = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(directory);
    const auto originalDirectory = std::filesystem::current_path();
    const auto path = spellcross_startup_log::Begin(directory / "spellcross.ini");
    if (path != directory / "Spellcross-startup.log") return 3;
    spellcross_startup_log::Write("ITEM", "MURY.DEF");
    // Read while the stream is still open: destruction must not be necessary.
    if (Read(path).find("ITEM: MURY.DEF") == std::string::npos) return 4;
    if (argc == 3 && std::string(argv[2]) == "--abrupt")
    {
        spellcross_startup_log::Write("ITEM", "LAST_BEFORE_ABRUPT_EXIT");
        std::_Exit(23);
    }
    std::vector<std::thread> writers;
    for (int i = 0; i < 4; ++i)
        writers.emplace_back([] {
            for (int j = 0; j < 100; ++j)
                spellcross_startup_log::Write("STEP", "concurrent record");
        });
    for (auto& thread : writers) thread.join();
    const auto text = Read(path);
    size_t count = 0, offset = 0;
    while ((offset = text.find("STEP: concurrent record\n", offset)) != std::string::npos)
    {
        ++count;
        ++offset;
    }
    if (count != 400) return 5;
    if (std::filesystem::current_path() != originalDirectory) return 6;
    std::cout << "PASS: log flushed before close; 400 concurrent records; CWD unchanged.\n";
}
