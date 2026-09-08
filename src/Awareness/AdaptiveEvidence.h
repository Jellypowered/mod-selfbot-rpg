#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <map>

namespace Sbrpg::Awareness
{
// Session-local bounded evidence. Distinct event attribution is the caller's
// responsibility. Never record yield from despawn, travel or cast completion.
class AdaptiveEvidence
{
public:
    static constexpr uint32_t Capacity = 256;
    static constexpr uint32_t HalfLifeMs = 120000;
    struct Record
    {
        float success = 0;
        float failure = 0;
        uint32_t samples = 0;
        uint32_t updated = 0;
    };

    void Observe(uint64_t key, bool success, uint32_t now)
    {
        auto it = records.find(key);
        if (it == records.end())
        {
            if (records.size() >= Capacity)
            {
                auto oldest = records.begin();
                for (auto candidate = records.begin(); candidate != records.end(); ++candidate)
                    if (uint32_t(now - candidate->second.updated) > uint32_t(now - oldest->second.updated))
                        oldest = candidate;
                records.erase(oldest);
            }
            it = records.emplace(key, Record{}).first;
        }
        auto& record = it->second;
        float const decay = Decay(record, now);
        record.success = std::min(8.0f, record.success * decay + (success ? 1.0f : 0.0f));
        record.failure = std::min(8.0f, record.failure * decay + (success ? 0.0f : 1.0f));
        record.samples = std::min(16u, record.samples + 1);
        record.updated = now;
    }

    float Cost(uint64_t key, float distance, float vertical, uint32_t now) const
    {
        float adjustment = 0;
        auto it = records.find(key);
        if (it != records.end() && it->second.samples >= 2)
            adjustment = std::clamp((it->second.failure - it->second.success) * Decay(it->second, now) * 5.0f, -20.0f, 40.0f);
        return std::max(0.0f, distance) + std::min(20.0f, std::abs(vertical) * 0.25f) + adjustment;
    }

    void Clear() { records.clear(); }
    std::size_t Size() const { return records.size(); }

private:
    static float Decay(Record const& record, uint32_t now)
    {
        return std::exp2(-float(uint32_t(now - record.updated)) / HalfLifeMs);
    }
    std::map<uint64_t, Record> records;
};
}
