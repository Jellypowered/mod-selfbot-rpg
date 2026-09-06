#ifndef SELFBOTRPG_LIVE_NODE_CACHE_H
#define SELFBOTRPG_LIVE_NODE_CACHE_H

#include "ObjectGuid.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace Sbrpg
{
    struct LiveNodeObservation
    {
        ObjectGuid guid;
        uint32 entry = 0;
        uint32 spawn = 0;
        uint32 mapId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        uint32 observedMs = 0;
        bool spawned = false;
        bool selectable = false;
    };

    class LiveNodeCache
    {
    public:
        bool Due(uint32 now, uint32 intervalMs = 1500) const
        {
            return lastRefreshMs == 0 || now - lastRefreshMs >= intervalMs;
        }

        void Replace(uint32 now, std::vector<LiveNodeObservation> nodes)
        {
            lastRefreshMs = now;
            observations = std::move(nodes);
        }

        void Remove(ObjectGuid guid)
        {
            observations.erase(std::remove_if(observations.begin(), observations.end(),
                [guid](LiveNodeObservation const& node) { return node.guid == guid; }), observations.end());
        }

        void Clear()
        {
            observations.clear();
            lastRefreshMs = 0;
        }

        uint32 LastRefreshMs() const { return lastRefreshMs; }
        std::vector<ObjectGuid> Guids() const
        {
            std::vector<ObjectGuid> guids;
            guids.reserve(observations.size());
            for (LiveNodeObservation const& node : observations)
                guids.push_back(node.guid);
            return guids;
        }

        std::vector<LiveNodeObservation> const& Observations() const { return observations; }

    private:
        uint32 lastRefreshMs = 0;
        std::vector<LiveNodeObservation> observations;
    };
}

#endif
