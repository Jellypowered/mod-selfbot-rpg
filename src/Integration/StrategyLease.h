#ifndef SELFBOTRPG_STRATEGY_LEASE_H
#define SELFBOTRPG_STRATEGY_LEASE_H

#include <string>
#include <utility>

class PlayerbotAI;

namespace Sbrpg
{
    // Tracks one module-owned strategy without assuming that the strategy was
    // absent at activity start. This keeps add/suspend/restore behavior reusable
    // across node, material, and future activity modes.
    class StrategyLease
    {
    public:
        StrategyLease() = default;
        explicit StrategyLease(std::string strategy) : strategyName(std::move(strategy)) { }

        void SetStrategy(std::string strategy) { strategyName = std::move(strategy); }
        void Acquire(PlayerbotAI* ai);
        void Suspend(PlayerbotAI* ai);
        void Release(PlayerbotAI* ai);

        bool IsOwned() const { return owned; }
        bool IsSuspended() const { return suspended; }
        bool IsActive() const { return owned || suspended; }
        std::string const& Name() const { return strategyName; }

    private:
        std::string strategyName;
        bool owned = false;
        bool suspended = false;
    };
}

#endif
