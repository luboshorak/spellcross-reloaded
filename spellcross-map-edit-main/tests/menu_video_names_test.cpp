#include "../source/menu_video_names.h"
#include <string_view>

using namespace spellcross_menu_video;
static_assert(std::string_view(Intro(true)) == "INTRO.CAN");
static_assert(std::string_view(Credits(true)) == "CREDITS.DPK");
static_assert(std::string_view(Intro(false)) == "INTRO.SMK");
static_assert(std::string_view(Credits(false)) == "CREDITS.SMK");
int main() {}

static_assert(std::string_view(Victory(true)[0]) == "TITAN1.CAN");
static_assert(std::string_view(Victory(true)[1]) == "TITAN2.CAN");
static_assert(std::string_view(Victory(true)[2]) == "S_10.DP2");
static_assert(std::string_view(Victory(true)[3]) == "LOGO.CAN");
static_assert(std::string_view(Victory(true)[4]) == "CREDITS.DPK");
static_assert(std::string_view(Victory(false)[0]) == "TITAN1.SMK");
static_assert(std::string_view(Victory(false)[1]) == "TITAN2.SMK");
static_assert(std::string_view(Victory(false)[2]) == "S_10.DP2");
static_assert(std::string_view(Victory(false)[3]) == "LOGO.SMK");
static_assert(std::string_view(Victory(false)[4]) == "CREDITS.SMK");
static_assert(std::string_view(Defeat()) == "S_OVER.DP2");
