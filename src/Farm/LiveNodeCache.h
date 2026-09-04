#ifndef SELFBOTRPG_LIVE_NODE_CACHE_H
#define SELFBOTRPG_LIVE_NODE_CACHE_H

#include "ObjectGuid.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Sbrpg
{
    class LiveNodeCache
    {
    public:
        bool Due(uint32 now, uint32 intervalMs = 1500) const
        {
            return lastRefreshMs == 0 || now - lastRefreshMs >= intervalMs;
        }

        void Replace(uint32 now, std::vector<ObjectGuid> nodes)
        {
            lastRefreshMs = now;
            guids = std::move(nodes);
        }

        void Remove(ObjectGuid guid)
        {
            guids.erase(std::remove(guids.begin(), guids.end(), guid), guids.end());
        }

        void Clear()
        {
            guids.clear();
            lastRefreshMs = 0;
        }

        uint32 LastRefreshMs() const { return lastRefreshMs; }
        std::vector<ObjectGuid> const& Guids() const { return guids; }

    private:
        uint32 lastRefreshMs = 0;
        std::vector<ObjectGuid> guids;
    };
}

#endif
