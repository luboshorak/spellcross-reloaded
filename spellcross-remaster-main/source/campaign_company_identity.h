#pragma once

#include "level.h"
#include "spell_units.h"
#include "other.h"
#include <wx/string.h>

namespace spellcross_company
{
    // Original DEF/tactical names are CP895; strategic JSON/UI names are UTF-8.
    inline std::string OriginalNameToUtf8(const std::string& value)
    {
        const wxScopedCharBuffer utf8 = wxString(char2wstringCP895(value.c_str())).ToUTF8();
        return utf8 ? std::string(utf8.data()) : std::string();
    }

    inline std::string NormalizeStoredName(const std::string& value)
    {
        // Older saves accidentally wrote raw CP895 into custom_name. Repair
        // those names once, preserving valid UTF-8 player/imported names.
        return value.empty() || !wxString::FromUTF8(value).empty()
            ? value : OriginalNameToUtf8(value);
    }

    inline std::string TacticalName(const std::string& utf8Name)
    {
        return wstring2stringCP895(wxString::FromUTF8(NormalizeStoredName(utf8Name)).ToStdWstring());
    }

    inline void ApplyTacticalIdentity(MapUnit& unit, const LevelData::PlayerUnitAdd& company)
    {
        // VoluntUnit lasts for the strategic chapter. MissionUnit is reserved
        // for helpers that must disappear after this single tactical mission.
        unit.spec_type = company.temporary ? MapUnitType::VoluntUnit : MapUnitType::NormalUnit;
        unit.name = company.extra == "-" ? std::string() : company.extra;
    }
}
