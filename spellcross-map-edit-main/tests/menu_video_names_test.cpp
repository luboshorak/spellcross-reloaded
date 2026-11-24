#include "../source/menu_video_names.h"
#include <string_view>

using namespace spellcross_menu_video;
static_assert(std::string_view(Intro(true)) == "INTRO.CAN");
static_assert(std::string_view(Credits(true)) == "CREDITS.DPK");
static_assert(std::string_view(Intro(false)) == "INTRO.SMK");
static_assert(std::string_view(Credits(false)) == "CREDITS.SMK");
int main() {}
