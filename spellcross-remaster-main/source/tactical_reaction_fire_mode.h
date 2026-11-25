#pragma once

#include <cstdint>
#include <unordered_set>

namespace spellcross_tactical
{
    enum class ReactionFireMode : int
    {
        Original = 0,
        Enhanced = 1
    };

    // Enhanced mode gives each defending company one initiative attempt
    // against each advancing enemy company per faction phase, at the FIRST
    // legal firing opportunity. Shared spotters continue supplying sight;
    // an early contact outside weapon range does not waste an attempt.
    // This is tactical phase state, not a per-save or global unit-type rule.
    class EnhancedReactionAttempts
    {
    public:
        static std::uint64_t Key(int mover_id, int shooter_id)
        {
            return (static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(mover_id)) << 32) |
                static_cast<std::uint32_t>(shooter_id);
        }

        bool Has(int mover_id, int shooter_id) const
        {
            return m_attempted.count(Key(mover_id, shooter_id)) != 0;
        }

        bool Mark(int mover_id, int shooter_id)
        {
            return m_attempted.insert(Key(mover_id, shooter_id)).second;
        }

        void Clear() { m_attempted.clear(); }

    private:
        std::unordered_set<std::uint64_t> m_attempted;
    };
}
