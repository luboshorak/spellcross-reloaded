#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace spellcross_release
{
struct VersionParts
{
    std::vector<int> values;
    bool valid = false;
    int releaseStage = 3; // 0=other prerelease, 1=beta, 2=RC, 3=stable
    int stageNumber = 0;
};

// Compare release tags as versions, not as plain numbers. In particular,
// v0.9.3-RC1 < v0.9.3-RC2 < v0.9.3, even though Windows resource version
// 0.9.3.0 is numerically identical to the final release's base version.
inline VersionParts ParseVersion(const std::string& text)
{
    VersionParts version;
    size_t pos = 0;
    while (pos < text.size() && !std::isdigit(static_cast<unsigned char>(text[pos])))
        ++pos;
    if (pos == text.size()) return version;

    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
    {
        int value = 0;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
        {
            value = value * 10 + text[pos] - '0';
            ++pos;
        }
        version.values.push_back(value);
        if (pos == text.size() || text[pos] != '.') break;
        ++pos;
    }
    version.valid = !version.values.empty();
    if (pos >= text.size() || text[pos] != '-') return version;

    std::string suffix = text.substr(pos + 1);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    size_t numberStart = 0;
    if (suffix.rfind("rc", 0) == 0)
    {
        version.releaseStage = 2;
        numberStart = 2;
    }
    else if (suffix.rfind("beta", 0) == 0)
    {
        version.releaseStage = 1;
        numberStart = 4;
    }
    else
    {
        version.releaseStage = 0;
    }
    for (size_t i = numberStart; i < suffix.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(suffix[i]);
        if (!std::isdigit(c)) break;
        version.stageNumber = version.stageNumber * 10 + (c - '0');
    }
    return version;
}

inline int CompareVersions(const std::string& left, const std::string& right,
    bool& comparable)
{
    const VersionParts a = ParseVersion(left), b = ParseVersion(right);
    comparable = a.valid && b.valid;
    if (!comparable) return 0;
    const size_t count = std::max(a.values.size(), b.values.size());
    for (size_t i = 0; i < count; ++i)
    {
        const int av = i < a.values.size() ? a.values[i] : 0;
        const int bv = i < b.values.size() ? b.values[i] : 0;
        if (av != bv) return av < bv ? -1 : 1;
    }
    if (a.releaseStage != b.releaseStage)
        return a.releaseStage < b.releaseStage ? -1 : 1;
    if (a.stageNumber != b.stageNumber)
        return a.stageNumber < b.stageNumber ? -1 : 1;
    return 0;
}
} // namespace spellcross_release
