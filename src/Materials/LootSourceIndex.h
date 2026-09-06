#ifndef SELFBOTRPG_LOOT_SOURCE_INDEX_H
#define SELFBOTRPG_LOOT_SOURCE_INDEX_H

#include "Materials/MaterialCatalog.h"
#include "SharedDefines.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sbrpg::Materials
{
    struct LootSource
    {
        uint32_t creatureEntry = 0;
        AcquisitionMethod method = AcquisitionMethod::CreatureLoot;
        uint32_t lootTemplateId = 0;
        float estimatedChance = 0.0f;
        uint32_t minCount = 1;
        uint32_t maxCount = 1;
        uint16_t lootMode = 1;
        uint8_t groupId = 0;
        bool questRequired = false;
        bool fromReference = false;
        SkillType requiredSkill = SKILL_NONE;
    };

    class LootSourceIndex
    {
    public:
        // Builds the reverse index lazily from the loaded world database. The
        // result is cached until Reset(), which is intended for a world-data
        // reload or a test fixture reset.
        static std::vector<LootSource> const& Find(uint32_t itemId);
        static std::string Describe(uint32_t itemId);
        static void Reset();
    };
}

#endif
