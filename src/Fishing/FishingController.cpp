#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgConfig.h"
#include "Core/SbrpgLogging.h"
#include "Fishing/FishingEquipment.h"
#include "Materials/MaterialStatus.h"
#include "Movement/MountController.h"
#include "Fishing/LureAction.h"
#include "Fishing/FishingController.h"

namespace Sbrpg::Runtime
{
bool FishingController::Execute(Sbrpg::Materials::MaterialFarmState& state, uint32 now)
{
            if (state.session.returnRequested)
            {
                // Stop cancels future casts, but an active cast/bobber is a
                // normal player action that must be allowed to finish before
                // returning home.
                if (bot->IsNonMeleeSpellCast(true))
                {
                    SetMaterialPhase(bot, state, "finishing fishing cast before return");
                    return false;
                }

                std::stringstream configuredBobbers(runtimeSettings.fishingBobberEntries);
                std::string bobberValue;
                while (std::getline(configuredBobbers, bobberValue, ','))
                {
                    uint32 bobberEntry = 0;
                    try { bobberEntry = static_cast<uint32>(std::stoul(bobberValue)); }
                    catch (...) { continue; }
                    std::list<GameObject*> bobbers;
                    bot->GetGameObjectListWithEntryInGrid(bobbers, bobberEntry, 30.0f);
                    for (GameObject* bobber : bobbers)
                    {
                        if (!bobber || bobber->GetOwnerGUID() != bot->GetGUID() ||
                            bobber->GetGoType() != GAMEOBJECT_TYPE_FISHINGNODE)
                            continue;
                        if (bobber->getLootState() == GO_READY)
                        {
                            SetMaterialPhase(bot, state, "reeling fishing catch before return");
                            bobber->Use(bot);
                        }
                        else
                            SetMaterialPhase(bot, state, "waiting for fishing bite before return");
                        return false;
                    }
                }

                LootObject loot = AI_VALUE(LootObject, "loot target");
                if (loot.IsEmpty() && bot->GetLootGUID().IsEmpty())
                    return travel.ReturnHome(state, now);
                SetMaterialPhase(bot, state, "finishing fishing loot before return");
                return false;
            }
            if (bot->IsInCombat() || bot->IsNonMeleeSpellCast(true))
                return false;


            if (!state.fishingPoolMode && !state.fishingPools.empty() &&
                now - state.fishingPoolLastScanMs >= 60000)
            {
                state.fishingPoolMode = true;
                state.fishingPoolMisses = 0;
                state.fishingPoolIndex = 0;
            }
            if (state.fishingPoolMode && !state.fishingPools.empty())
            {
                if (state.fishingPoolIndex >= state.fishingPools.size())
                    state.fishingPoolIndex = 0;
                auto& pool = state.fishingPools[state.fishingPoolIndex];
                std::list<GameObject*> livePools;
                bot->GetGameObjectListWithEntryInGrid(livePools, pool.entry, 20.0f);
                bool poolPresent = false;
                for (GameObject* object : livePools)
                    if (object && object->isSpawned() &&
                        bot->GetExactDist(object) <= 20.0f)
                    { poolPresent = true; break; }
                if (!poolPresent)
                {
                    ++state.fishingPoolMisses;
                    state.fishingPoolIndex = (state.fishingPoolIndex + 1) % state.fishingPools.size();
                    state.fishingPoolStep = Sbrpg::RouteStep();
                    if (state.fishingPoolMisses >= state.fishingPools.size())
                    {
                        state.fishingPoolMode = false;
                        state.fishingPoolLastScanMs = now;
                        state.fishingPoolMisses = 0;
                        SetMaterialPhase(bot, state, "no active fishing pools; open water fallback");
                    }
                    return false;
                }
                state.fishingPoolMisses = 0;
                if (bot->GetExactDist(pool.x, pool.y, pool.z) > 10.0f)
                {
                    if ((bot->movespline && !bot->movespline->Finalized()) || bot->isMoving())
                        return false;
                    Sbrpg::RouteStep step;
                    if (Sbrpg::BuildRouteStep(bot, pool.x, pool.y, pool.z, step) &&
                        travel.IssueBoundedMove(step))
                    {
                        state.fishingPoolStep = step;
                        SetMaterialPhase(bot, state, "travelling to fishing pool");
                    }
                    else
                    {
                        state.fishingPoolIndex = (state.fishingPoolIndex + 1) % state.fishingPools.size();
                    }
                    return false;
                }
                SetMaterialPhase(bot, state, "fishing pool");
                if (bot->IsMounted())
                {
                    bot->Dismount();
                    return true;
                }
            }

            // The stock action searches only about 60 yards. SBRPG keeps the
            // same normal-player land/water rules but expands the bounded
            // search so fishing can reach water from a distant inland start.
            bool customWaterSearch = false;
            if (!state.fishingPoolMode && state.fishingLastWaterSearchMs == 0)
            {
                state.fishingLastWaterSearchMs = now;
                // Clear any stale stock-playerbot fishing spot first. The
                // configured cast distance must not be silently replaced by
                // the stock action's wider default search.
                SET_AI_VALUE(WorldPosition, "fishing spot", WorldPosition());
                WorldPosition water = FindWaterRadial(bot, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                    bot->GetMap(), bot->GetPhaseMask(), 10.0f, runtimeSettings.fishingSearchDistance, 10.0f, false);
                if (water.IsValid())
                {
                    float const angle = bot->GetAngle(water.GetPositionX(), water.GetPositionY());
                    WorldPosition land = FindLandFromPosition(botAI, 0.0f, runtimeSettings.fishingCastDistance,
                        1.0f, angle, water, runtimeSettings.fishingSearchDistance, false);
                    if (land.IsValid())
                    {
                        SET_AI_VALUE(WorldPosition, "fishing spot", land);
                        state.fishingCustomWaterSpot = true;
                    }
                }
                customWaterSearch = true;
            }

            // Open-water fishing must use the configured module-owned spot.
            // Only use the stock search if the custom search was not needed
            // (for example, a pool session or an already valid stock spot).
            MoveNearWaterAction moveNearWater(botAI);
            bool const waterSpotAvailable = customWaterSearch ?
                AI_VALUE(WorldPosition, "fishing spot").IsValid() : moveNearWater.isPossible();
            if (moveNearWater.isUseful())
            {
                SetMaterialPhase(bot, state, "moving to open water");
                WorldPosition fishingSpot = AI_VALUE(WorldPosition, "fishing spot");
                Sbrpg::RouteStep waterStep;
                if (fishingSpot.IsValid() && Sbrpg::BuildRouteStep(bot,
                    fishingSpot.GetPositionX(), fishingSpot.GetPositionY(), fishingSpot.GetPositionZ(), waterStep))
                {
                    if (!PrepareTravelMove(bot))
                        return true;
                    if (Sbrpg::FollowRouteStep(bot, waterStep) || travel.IssueBoundedMove(waterStep))
                        return true;
                    Debug(bot, Acore::StringFormat("water route built but movement was rejected ({:.1f}, {:.1f}, {:.1f})",
                        fishingSpot.GetPositionX(), fishingSpot.GetPositionY(), fishingSpot.GetPositionZ()));
                }
                else if (!fishingSpot.IsValid())
                {
                    if (state.fishingWaterLastReportMs == 0 || now - state.fishingWaterLastReportMs >= 5000)
                    {
                        state.fishingWaterLastReportMs = now;
                        ChatHandler(bot->GetSession()).PSendSysMessage(
                            "[SBRPG] No reachable fishing water found from the current position; check VMAP/MMap data and fishing distance settings.");
                    }
                    SetMaterialPhase(bot, state, "water search failed");
                    Sbrpg::RequestActivityReturn(state.session, now, "no reachable fishing water");
                    return true;
                }
                // The stock action may still succeed when its cached spot is
                // not mmap-routeable from this controller tick. Apply the
                // same mount gate before allowing that fallback movement.
                if (!PrepareTravelMove(bot))
                    return true;
                if (moveNearWater.Execute(Event("sbrpg move near water")))
                    return true;
                if (state.fishingWaterLastReportMs == 0 || now - state.fishingWaterLastReportMs >= 5000)
                {
                    state.fishingWaterLastReportMs = now;
                    ChatHandler(bot->GetSession()).PSendSysMessage(
                        "[SBRPG] Fishing water route could not be started; movement was rejected.");
                }
                return true;
            }
            if (!waterSpotAvailable && !AI_VALUE(WorldPosition, "fishing spot").IsValid())
            {
                SetMaterialPhase(bot, state, "no reachable water found");
                Sbrpg::RequestActivityReturn(state.session, now, "no reachable water");
                return true;
            }

            if (bot->IsMounted())
            {
                bot->Dismount();
                return true;
            }

            std::vector<uint32> bobberEntries;
            std::string configured = runtimeSettings.fishingBobberEntries;
            bool hasOwnedBobber = false;
            std::stringstream entries(configured);
            std::string value;
            while (std::getline(entries, value, ','))
            {
                try { bobberEntries.push_back(static_cast<uint32>(std::stoul(value))); }
                catch (...) { }
            }
            std::list<GameObject*> nearby;
            for (uint32 entry : bobberEntries)
                bot->GetGameObjectListWithEntryInGrid(nearby, entry, 30.0f);
            for (GameObject* bobber : nearby)
            {
                if (bobber && bobber->GetOwnerGUID() == bot->GetGUID() &&
                    bobber->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE)
                {
                    hasOwnedBobber = true;
                    if (bobber->getLootState() == GO_READY)
                    {
                        SetMaterialPhase(bot, state, "reeling fishing catch");
                        bobber->Use(bot);
                        state.fishingLastCastMs = now;
                        return true;
                    }
                }
            }
            if (hasOwnedBobber)
            {
                SetMaterialPhase(bot, state, "waiting for fishing bite");
                return false;
            }
            if (state.fishingLastCastMs == 0 || now - state.fishingLastCastMs >= 5000)
            {
                // Check the lure immediately before every real cast. A lure
                // action may take a tick; never mark fishing as failed when a
                // lure is absent or unusable.
                if (runtimeSettings.useLures)
                {
                    Item* pole = FindEquippedFishingPole(bot);
                    if (pole && pole->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT) == 0)
                        if (Item* lure = FindFishingLure(bot))
                        {
                            SetMaterialPhase(bot, state, "applying optional fishing lure");
                            SbrpgLureUseAction lureAction(botAI);
                            if (lureAction.Apply(lure, pole))
                                return true;
                            if (state.fishingLureLastReportMs == 0 || now - state.fishingLureLastReportMs >= 10000)
                            {
                                state.fishingLureLastReportMs = now;
                                ChatHandler(bot->GetSession()).PSendSysMessage(
                                    "[SBRPG] Fishing lure found but could not be applied; continuing without lure.");
                            }
                        }
                }
                SetMaterialPhase(bot, state, "casting fishing line");
                bot->CastSpell(bot, 18248, true);
                state.fishingLastCastMs = now;
                return true;
            }
            SetMaterialPhase(bot, state, "waiting for fishing bobber");
            return false;
        }

}
