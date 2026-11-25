#pragma once

enum class SpellLang { ENG, CZE, POL, ITA };

// The 206/207-byte unit-record layout is not a retail language identifier.
// Preserve the established CZ/ENG decision for those editions and mixed data.
// Localized-only archives are identified by their original language resources.
// Keep established precedence if multiple retail languages are present.
inline SpellLang SelectSpellLanguage(bool englishLayout, bool hasCzech,
    bool hasEnglish, bool hasPolish, bool hasItalian = false)
{
    if (hasPolish && !hasCzech && !hasEnglish)
        return SpellLang::POL;
    if (hasItalian && !hasCzech && !hasEnglish && !hasPolish)
        return SpellLang::ITA;
    return englishLayout ? SpellLang::ENG : SpellLang::CZE;
}

inline const char* SpellLanguageExtension(SpellLang language)
{
    switch (language)
    {
    case SpellLang::ITA: return ".IT";
    case SpellLang::POL: return ".PL";
    case SpellLang::ENG: return ".ENG";
    default: return ".CZ";
    }
}

inline const char* SpellMainFontResource(SpellLang language)
{
    if (language == SpellLang::ITA) return "FONT_IT.FNT";
    return language == SpellLang::POL ? "FONT_PL.FNT" : "FONT_001.FNT";
}

