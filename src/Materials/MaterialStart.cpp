#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Core/SbrpgConfig.h"
#include "Fishing/FishingStart.h"
#include "Integration/PlayerbotIntegration.h"
#include "Materials/MaterialSourceSelector.h"
#include "Materials/MaterialStatus.h"
#include "Materials/MaterialStart.h"
#include "Nodes/NodeResources.h"
#include "Safety/EligibilityPolicy.h"

namespace Sbrpg::Runtime
{
    bool StartMaterial(Player* player, std::string materialName, uint32 durationMinutes,
                       uint32 quantityGoal, std::string* error)
    {
        // Material starts use the same module-owned selfbot lease as node
        // farming. Refuse overlap so Stop can always release exactly what this
        // run acquired and return to its own captured start position.
        if (!player)
        {
            if (error)
                *error = "Player is unavailable.";
            return false;
        }
        if (Sbrpg::IsActive(player))
        {
            if (error)
                *error = "Stop the active node-farming session before starting material farming.";
            return false;
        }
        if (!RuntimeEnabled())
        { if (error) *error = "SBRPG is disabled by SelfBotRpg.Enable."; return false; }
        if (ActivityRegistry::Materials().contains(player->GetGUID()))
        {
            if (error)
                *error = "Stop the active material session before starting another one.";
            return false;
        }

        uint32 itemId = 0;
        Sbrpg::Materials::MaterialDefinition const* material = ResolveMaterial(std::move(materialName), itemId);
        if (!material)
        { if (error) *error = "Unknown material; use a catalog name (cloth or leather) or an exact item ID."; return false; }

        float const minimumChance = runtimeSettings.materialMinimumChance;
        auto supportsMethod = [material](Sbrpg::Materials::AcquisitionMethod method)
        {
            return std::find(material->methods.begin(), material->methods.end(), method) != material->methods.end();
        };
        if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::Fishing))
            return StartFishingMaterial(player, itemId, durationMinutes, quantityGoal, error, false,
                runtimeSettings.fishingPrioritizePools, runtimeSettings.fishingOpenWaterOnly);
        if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::GameObjectNode))
        {
            std::vector<uint32> nodeEntries = ResolveNodeEntriesForItem(itemId);
            if (nodeEntries.empty())
            { if (error) *error = "No database gathering nodes produce this material."; return false; }
            Sbrpg::Profession profession = IsMiningMaterial(itemId) ?
                Sbrpg::Profession::Mining : Sbrpg::Profession::Herbalism;
            return Sbrpg::Start(player, profession, std::move(nodeEntries), durationMinutes,
                error, itemId, quantityGoal);
        }

        bool enabledBySbrpg = false;
        if (!EnsureSelfBot(player, enabledBySbrpg))
        { if (error) *error = "Unable to enable self-bot mode."; return false; }
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        std::vector<SkillType> harvestSkills;
        std::vector<uint32> sourceEntries;
        uint32 blockedHarvestSources = 0;
        auto addHarvestSkill = [&harvestSkills](SkillType skill)
        {
            if (IsCorpseHarvestSkill(skill) &&
                std::find(harvestSkills.begin(), harvestSkills.end(), skill) == harvestSkills.end())
                harvestSkills.push_back(skill);
        };
        for (Sbrpg::Materials::LootSource const& source : Sbrpg::Materials::LootSourceIndex::Find(itemId))
        {
            if (source.questRequired || !supportsMethod(source.method) ||
                source.estimatedChance < minimumChance)
                continue;

            if (source.requiredSkill != SKILL_NONE)
            {
                if (!IsCorpseHarvestSkill(source.requiredSkill) ||
                    !ai->HasSkill(source.requiredSkill) ||
                    !HasHarvestTool(player, source.requiredSkill))
                {
                    ++blockedHarvestSources;
                    continue;
                }
                addHarvestSkill(source.requiredSkill);
            }

            if (std::find(sourceEntries.begin(), sourceEntries.end(), source.creatureEntry) == sourceEntries.end())
                sourceEntries.push_back(source.creatureEntry);
        }
        if (sourceEntries.empty())
        {
            if (error)
            {
                if (blockedHarvestSources != 0)
                    *error = "All qualifying sources require a missing profession, skill, or tool.";
                else if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::Transformation))
                    *error = "This material is a crafted or transformation output, not a direct farm target.";
                else if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::Fishing))
                    *error = "Fishing material runs require the module-owned fishing controller.";
                else if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::GameObjectNode))
                    *error = "Use the node-farming controls for this gathering material until material-target node bridging is enabled.";
                else
                    *error = Acore::StringFormat("No supported creature sources meet the {:.2f}% minimum chance.", minimumChance);
            }
            DisableOwnedSelfBot(player, enabledBySbrpg);
            return false;
        }

        std::vector<Sbrpg::Materials::Hotspot> hotspots =
            Sbrpg::Materials::HotspotPlanner::Build(
                Sbrpg::Materials::CreatureSpawnRepository::Load(player, sourceEntries, runtimeSettings.stayInCurrentZone));
        hotspots = Sbrpg::Materials::HotspotPlanner::Plan(player, std::move(hotspots));
        if (hotspots.empty())
        { DisableOwnedSelfBot(player, enabledBySbrpg); if (error) *error = "No reachable creature hotspots found in the current zone."; return false; }

        Sbrpg::Materials::MaterialFarmState& state = ActivityRegistry::Materials()[player->GetGUID()];
        state = Sbrpg::Materials::MaterialFarmState();
        state.active = true;
        state.selfBotEnabledBySbrpg = enabledBySbrpg;
        state.itemId = itemId;
        state.quantityGoal = quantityGoal;
        state.inventoryStart = player->GetItemCount(itemId, true);
        state.inventoryCount = state.inventoryStart;
        Sbrpg::BeginActivitySession(state.session, player, durationMinutes, getMSTime());
        state.mapId = player->GetMapId();
        state.zoneId = player->GetZoneId();
        state.creatureEntries = std::move(sourceEntries);
        state.needsSkinning = std::find(harvestSkills.begin(), harvestSkills.end(), SKILL_SKINNING) != harvestSkills.end();
        state.harvestSkills = std::move(harvestSkills);
        state.hotspots = std::move(hotspots);
        SetMaterialPhase(player, state, Acore::StringFormat("Material route loaded: {} nearby hotspots.", state.hotspots.size()));
        state.previousLootStrategy = ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Get()->GetName();
        state.lootStrategy.SetStrategy("loot");
        // Ensure stock loot exists even when selfbot was just enabled; the
        // lease records whether SBRPG added it so it can be restored on stop.
        state.lootStrategy.Acquire(ai);
        state.lootStrategy.Suspend(ai);
        ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Set(
            LootStrategyValue::instance("all"));
        state.lootStrategyOverridden = true;
        state.materialLootStrategy.SetStrategy("sbrpg material loot");
        state.materialLootStrategy.Acquire(ai);
        if (!state.harvestSkills.empty())
        {
            state.gatherStrategy.SetStrategy("gather");
            state.gatherStrategy.Acquire(ai);
        }
        state.grindStrategy.SetStrategy("grind");
        state.grindStrategy.Suspend(ai);
        state.rpgStrategy.SetStrategy("rpg");
        state.rpgStrategy.Suspend(ai);
        state.materialStrategy.SetStrategy("sbrpg material");
        state.materialStrategy.Acquire(ai);
        // Reuse playerbot's mount controller. It evaluates the character's
        // learned riding skill and available mounts, then chooses the best
        // valid ground or flying mount automatically. Fishing may mount while
        // travelling to water or between pools, then dismount before casting.
        state.mountStrategy.SetStrategy("mount");
        state.mountStrategy.Acquire(ai);
        return true;
    }

}
