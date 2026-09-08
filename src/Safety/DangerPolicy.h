#pragma once
#include <cstdint>

namespace Sbrpg::Safety
{
// Callers supply a fresh, filtered snapshot; incomplete scans fail closed.
struct ThreatSummary
{
    uint32_t ordinaryNonGray = 0;
    uint32_t plusTwo = 0;
    uint32_t plusThree = 0;
    bool elite = false;
    bool complete = false;
};
enum class Risk { Unknown, Safe, Elite, DensePack, LevelPack };
inline Risk Assess(ThreatSummary const& threats)
{
    if (!threats.complete) return Risk::Unknown;
    if (threats.elite) return Risk::Elite;
    if (threats.ordinaryNonGray >= 3) return Risk::DensePack;
    if (threats.plusThree || threats.plusTwo >= 2) return Risk::LevelPack;
    return Risk::Safe;
}
}
