#include "LootSourceIndex.h"

#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "QueryResult.h"
#include "StringFormat.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace Sbrpg::Materials
{
    namespace
    {
        struct LootRow
        {
            uint32_t templateId = 0;
            uint32_t itemId = 0;
            int32_t reference = 0;
            float chance = 0.0f;
            bool questRequired = false;
            uint16_t lootMode = 1;
            uint8_t groupId = 0;
            uint8_t minCount = 1;
            uint8_t maxCount = 1;
        };

        struct CreatureLootIds
        {
            uint32_t creatureEntry = 0;
            uint32_t corpseLootId = 0;
            uint32_t skinLootId = 0;
        };

        using LootRows = std::unordered_map<uint32_t, std::vector<LootRow>>;

        struct IndexData
        {
            std::unordered_map<uint32_t, std::vector<LootSource>> sources;
            bool loaded = false;
        };

        IndexData& Data()
        {
            static IndexData data;
            return data;
        }

        void LoadRows(char const* tableName, LootRows& rows)
        {
            QueryResult result = WorldDatabase.Query(Acore::StringFormat(
                "SELECT Entry, Item, Reference, Chance, QuestRequired, LootMode, GroupId, MinCount, MaxCount FROM {}",
                tableName));
            if (!result)
            {
                LOG_ERROR("module", "[SBRPG] Unable to load {} for material source index", tableName);
                return;
            }

            do
            {
                Field* fields = result->Fetch();
                LootRow row;
                row.templateId = fields[0].Get<uint32>();
                row.itemId = fields[1].Get<uint32>();
                row.reference = fields[2].Get<int32>();
                row.chance = fields[3].Get<float>();
                row.questRequired = fields[4].Get<bool>();
                row.lootMode = fields[5].Get<uint16>();
                row.groupId = fields[6].Get<uint8>();
                row.minCount = fields[7].Get<uint8>();
                row.maxCount = fields[8].Get<uint8>();
                rows[row.templateId].push_back(row);
            } while (result->NextRow());
        }

        void AddSource(IndexData& data, uint32_t itemId, LootSource source)
        {
            std::vector<LootSource>& sources = data.sources[itemId];
            auto existing = std::find_if(sources.begin(), sources.end(), [&source](LootSource const& candidate)
            {
                return candidate.creatureEntry == source.creatureEntry &&
                       candidate.method == source.method &&
                       candidate.lootTemplateId == source.lootTemplateId;
            });
            if (existing == sources.end())
            {
                sources.push_back(source);
                return;
            }

            existing->estimatedChance = std::max(existing->estimatedChance, source.estimatedChance);
            existing->minCount = std::min(existing->minCount, source.minCount);
            existing->maxCount = std::max(existing->maxCount, source.maxCount);
            existing->questRequired = existing->questRequired && source.questRequired;
            existing->fromReference = existing->fromReference || source.fromReference;
        }

        void ResolveTemplate(IndexData& data, LootRows const& rows, uint32_t templateId,
            uint32_t creatureEntry, AcquisitionMethod method, uint32_t rootTemplateId,
            float parentChance, uint32_t minMultiplier, uint32_t maxMultiplier,
            bool fromReference, std::unordered_set<uint32_t>& activeReferences, uint8_t depth)
        {
            if (depth > 16 || activeReferences.contains(templateId))
                return;

            auto const found = rows.find(templateId);
            if (found == rows.end())
                return;

            activeReferences.insert(templateId);
            for (LootRow const& row : found->second)
            {
                // Loot mode 1 is the normal open-world mode. Rows restricted
                // to another mode are not reliable proactive farming sources.
                if ((row.lootMode & 1) == 0)
                    continue;

                float const chance = parentChance * std::max(0.0f, row.chance) / 100.0f;
                uint32_t const minCount = minMultiplier * std::max<uint8_t>(1, row.minCount);
                uint32_t const maxCount = maxMultiplier * std::max<uint8_t>(1, row.maxCount);
                if (row.reference != 0)
                {
                    ResolveTemplate(data, rows, static_cast<uint32_t>(std::abs(row.reference)),
                        creatureEntry, method, rootTemplateId, chance, minCount, maxCount,
                        true, activeReferences, depth + 1);
                    continue;
                }

                if (row.itemId == 0)
                    continue;

                AddSource(data, row.itemId, LootSource{
                    creatureEntry, method, rootTemplateId, chance, minCount, maxCount,
                    row.lootMode, row.groupId, row.questRequired, fromReference});
            }
            activeReferences.erase(templateId);
        }

        void Load()
        {
            IndexData& data = Data();
            if (data.loaded)
                return;
            data.loaded = true;

            LootRows corpseRows;
            LootRows skinRows;
            LoadRows("creature_loot_template", corpseRows);
            LoadRows("skinning_loot_template", skinRows);

            QueryResult creatures = WorldDatabase.Query(
                "SELECT entry, lootid, skinloot FROM creature_template WHERE lootid <> 0 OR skinloot <> 0");
            if (!creatures)
            {
                LOG_ERROR("module", "[SBRPG] Unable to load creature loot IDs for material source index");
                return;
            }

            do
            {
                Field* fields = creatures->Fetch();
                CreatureLootIds ids{
                    fields[0].Get<uint32>(), fields[1].Get<uint32>(), fields[2].Get<uint32>()};
                if (ids.corpseLootId != 0)
                {
                    std::unordered_set<uint32_t> activeReferences;
                    ResolveTemplate(data, corpseRows, ids.corpseLootId, ids.creatureEntry,
                        AcquisitionMethod::CreatureLoot, ids.corpseLootId, 100.0f, 1, 1,
                        false, activeReferences, 0);
                }
                if (ids.skinLootId != 0)
                {
                    std::unordered_set<uint32_t> activeReferences;
                    ResolveTemplate(data, skinRows, ids.skinLootId, ids.creatureEntry,
                        AcquisitionMethod::Skinning, ids.skinLootId, 100.0f, 1, 1,
                        false, activeReferences, 0);
                }
            } while (creatures->NextRow());

            for (auto& [itemId, sources] : data.sources)
            {
                std::sort(sources.begin(), sources.end(), [](LootSource const& left, LootSource const& right)
                {
                    if (left.creatureEntry != right.creatureEntry)
                        return left.creatureEntry < right.creatureEntry;
                    return static_cast<uint8_t>(left.method) < static_cast<uint8_t>(right.method);
                });
            }

            LOG_INFO("module", "[SBRPG] Material source index loaded for {} item IDs", data.sources.size());
        }
    }

    std::vector<LootSource> const& LootSourceIndex::Find(uint32_t itemId)
    {
        Load();
        auto const found = Data().sources.find(itemId);
        if (found != Data().sources.end())
            return found->second;
        static std::vector<LootSource> const empty;
        return empty;
    }

    std::string LootSourceIndex::Describe(uint32_t itemId)
    {
        MaterialDefinition const* material = Materials::Find(itemId);
        std::ostringstream output;
        if (material)
            output << material->displayName << " (" << material->itemId << ")";
        else
            output << "item " << itemId;

        std::vector<LootSource> const& sources = LootSourceIndex::Find(itemId);
        output << ": " << sources.size() << " creature sources";
        for (LootSource const& source : sources)
        {
            CreatureTemplate const* creature = sObjectMgr->GetCreatureTemplate(source.creatureEntry);
            output << " | " << source.creatureEntry;
            if (creature)
                output << " " << creature->Name;
            output << " [" << MethodName(source.method) << ", ~" << source.estimatedChance << "%";
            if (source.minCount != source.maxCount)
                output << ", " << source.minCount << "-" << source.maxCount;
            else
                output << ", " << source.minCount;
            if (source.groupId != 0)
                output << ", group " << static_cast<uint32_t>(source.groupId);
            if (source.fromReference)
                output << ", ref";
            if (source.questRequired)
                output << ", quest";
            output << "]";
        }
        return output.str();
    }

    void LootSourceIndex::Reset()
    {
        Data() = IndexData();
    }
}
