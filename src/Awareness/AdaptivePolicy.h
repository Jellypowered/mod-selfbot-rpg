#pragma once

#include "Movement/RouteFollower.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace Sbrpg::Awareness
{
    // Cooldowns are session-local candidate hints, never permanent exclusions.
    // They are separate from the legacy empty/failed-node blacklist so a status
    // or debug record can retain why a candidate was skipped.
    enum class CooldownReason : uint8_t
    {
        Danger,
        Empty,
        Unreachable,
        Contested,
        RecentlyCleared,
        RouteStalled
    };

    inline char const* CooldownReasonName(CooldownReason reason)
    {
        switch (reason)
        {
            case CooldownReason::Danger: return "danger";
            case CooldownReason::Empty: return "empty";
            case CooldownReason::Unreachable: return "unreachable";
            case CooldownReason::Contested: return "contested";
            case CooldownReason::RecentlyCleared: return "recently-cleared";
            case CooldownReason::RouteStalled: return "route-stalled";
        }
        return "unknown";
    }

    struct Cooldown
    {
        uint32_t untilMs = 0;
        CooldownReason reason = CooldownReason::Danger;
    };

    class CooldownBook
    {
    public:
        static constexpr uint32_t Capacity = 256;

        void Set(uint64_t key, CooldownReason reason, uint32_t now, uint32_t durationMs)
        {
            if (cooldowns.find(key) == cooldowns.end() && cooldowns.size() >= Capacity)
                return;
            cooldowns[key] = { now + durationMs, reason };
        }

        bool Active(uint64_t key, uint32_t now) const
        {
            auto it = cooldowns.find(key);
            return it != cooldowns.end() && now < it->second.untilMs;
        }

        void Expire(uint32_t now)
        {
            for (auto it = cooldowns.begin(); it != cooldowns.end(); )
                if (now >= it->second.untilMs) it = cooldowns.erase(it);
                else ++it;
        }

        void Pause(uint32_t pauseStartedMs, uint32_t pausedMs)
        {
            for (auto& entry : cooldowns)
                if (entry.second.untilMs >= pauseStartedMs)
                    entry.second.untilMs += pausedMs;
        }

        void Clear() { cooldowns.clear(); }
        std::size_t Size() const { return cooldowns.size(); }

    private:
        std::unordered_map<uint64_t, Cooldown> cooldowns;
    };

    // A committed objective is retained for a short interval unless a new
    // candidate materially improves its cost. This prevents scan-order flips
    // between cave/ground candidates while keeping deterministic ties.
    inline bool ShouldSwitchObjective(uint32_t committedSinceMs, float currentCost,
        float challengerCost, uint32_t now)
    {
        constexpr uint32_t MinimumCommitmentMs = 15000;
        constexpr float RequiredImprovement = 0.15f;
        if (committedSinceMs == 0 || now - committedSinceMs >= MinimumCommitmentMs)
            return challengerCost < currentCost;
        return challengerCost < currentCost * (1.0f - RequiredImprovement);
    }

    // This is a geometry classification, not a terrain oracle. "Underground"
    // means a validated corridor has cave-like detour/vertical complexity; it
    // never assumes an unvalidated direct line is above ground or traversable.
    enum class GeometryClass : uint8_t { Flat, Vertical, Detour, UndergroundLike };

    inline char const* GeometryClassName(GeometryClass kind)
    {
        switch (kind)
        {
            case GeometryClass::Flat: return "flat";
            case GeometryClass::Vertical: return "vertical";
            case GeometryClass::Detour: return "detour";
            case GeometryClass::UndergroundLike: return "underground-like";
        }
        return "unknown";
    }

    struct GeometryAssessment
    {
        float basePenalty = 40.0f;
        float detourRatio = 1.0f;
        float verticalDelta = 0.0f;
        uint32_t segments = 0;
        GeometryClass kind = GeometryClass::UndergroundLike;
    };

    // Scores only a validated mmap corridor. A complex candidate remains
    // eligible when it is the sole option; alternate density merely makes its
    // bounded penalty more meaningful when simpler candidates are available.
    inline GeometryAssessment AssessGeometry(RouteStep const& step, float startX, float startY, float startZ)
    {
        GeometryAssessment assessment;
        if (step.corridor.empty())
            return assessment;
        float routeLength = 0.0f;
        float previousX = startX, previousY = startY, previousZ = startZ;
        for (G3D::Vector3 const& point : step.corridor)
        {
            float const dx = point.x - previousX, dy = point.y - previousY, dz = point.z - previousZ;
            routeLength += std::sqrt(dx * dx + dy * dy + dz * dz);
            previousX = point.x; previousY = point.y; previousZ = point.z;
        }
        float const directDx = step.x - startX, directDy = step.y - startY, directDz = step.z - startZ;
        float const directLength = std::sqrt(directDx * directDx + directDy * directDy + directDz * directDz);
        assessment.detourRatio = directLength > 1.0f ? routeLength / directLength : 1.0f;
        assessment.verticalDelta = std::abs(step.z - startZ);
        assessment.segments = static_cast<uint32_t>(step.corridor.size() - 1);
        float const detour = std::max(0.0f, assessment.detourRatio - 1.0f) * 12.0f;
        float const segments = std::min(12.0f, assessment.segments * 0.5f);
        float const vertical = std::min(8.0f, assessment.verticalDelta * 0.2f);
        assessment.basePenalty = std::min(40.0f, detour + segments + vertical);
        if (assessment.detourRatio >= 1.75f && (assessment.verticalDelta >= 8.0f || assessment.segments >= 12))
            assessment.kind = GeometryClass::UndergroundLike;
        else if (assessment.verticalDelta >= 12.0f)
            assessment.kind = GeometryClass::Vertical;
        else if (assessment.detourRatio >= 1.25f || assessment.segments >= 8)
            assessment.kind = GeometryClass::Detour;
        else
            assessment.kind = GeometryClass::Flat;
        return assessment;
    }

    inline float GeometryPenalty(GeometryAssessment const& assessment, uint32_t alternateCount)
    {
        float const alternatePenalty = assessment.kind == GeometryClass::UndergroundLike ||
            assessment.kind == GeometryClass::Detour ? std::min(12.0f, alternateCount * 2.0f) : 0.0f;
        return std::min(40.0f, assessment.basePenalty + alternatePenalty);
    }
}