#include "../source/pak_dataset_import.h"
#include <wx/init.h>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    wxInitializer init;
    if (!init.IsOk()) return 2;
    try
    {
        auto dataset = spellcross_pak::inspect(std::filesystem::u8path(argv[1]));
        spellcross_pak::materialize(dataset, std::filesystem::u8path(argv[2]));
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
