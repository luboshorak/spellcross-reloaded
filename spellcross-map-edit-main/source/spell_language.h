#pragma once

enum class SpellLang { ENG, CZE, POL };

// The 206/207-byte unit-record layout is not a retail language identifier.
// Preserve the established CZ/ENG decision for those editions and mixed data.
// A Polish-only archive is identified by its own original language resources.
inline SpellLang SelectSpellLanguage(bool englishLayout, bool hasCzech,
    bool hasEnglish, bool hasPolish)
{
    if (hasPolish && !hasCzech && !hasEnglish)
        return SpellLang::POL;
    return englishLayout ? SpellLang::ENG : SpellLang::CZE;
}

inline const char* SpellLanguageExtension(SpellLang language)
{
    switch (language)
    {
    case SpellLang::POL: return ".PL";
    case SpellLang::ENG: return ".ENG";
    default: return ".CZ";
    }
}

inline const char* SpellMainFontResource(SpellLang language)
{
    return language == SpellLang::POL ? "FONT_PL.FNT" : "FONT_001.FNT";
}
