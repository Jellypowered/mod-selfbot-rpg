#pragma once

// Private implementation dependencies; never include from the public facade.
#include "SBRPG.h"
#include "Nodes/NodeRepository.h"
#include "Movement/RouteFollower.h"
#include "Movement/RoutePlanner.h"
#include "Materials/CreatureSpawnRepository.h"
#include "Materials/HotspotPlanner.h"
#include "Materials/LootSourceIndex.h"
#include "Materials/MaterialFarmState.h"
#include "Materials/MaterialCatalog.h"
#include "Protocol/SbrpgProtocol.h"
#include "Protocol/StatusPublisher.h"

#include "Action.h"
#include "AttackAction.h"
#include "AiObjectContext.h"
#include "DKAiObjectContext.h"
#include "DruidAiObjectContext.h"
#include "HunterAiObjectContext.h"
#include "MageAiObjectContext.h"
#include "PaladinAiObjectContext.h"
#include "PriestAiObjectContext.h"
#include "RogueAiObjectContext.h"
#include "ShamanAiObjectContext.h"
#include "WarlockAiObjectContext.h"
#include "WarriorAiObjectContext.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CheckMountStateAction.h"
#include "CellImpl.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Event.h"
#include "GameObject.h"
#include "Group.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "LootObjectStack.h"
#include "LootStrategyValue.h"
#include "Log.h"
#include "Map.h"
#include "Movement/Spline/MoveSplineInitArgs.h"
#include "MovementActions.h"
#include "FishingAction.h"
#include "NamedObjectContext.h"
#include "UseItemAction.h"
#include "NearestGameObjects.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "PathGenerator.h"
#include "ScriptMgr.h"
#include "ServerFacade.h"
#include "SpellMgr.h"
#include "Strategy.h"
#include "StringFormat.h"
#include "Timer.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <exception>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace Acore::ChatCommands;

// Stock playerbot helper; kept as a module-local declaration so no global
// playerbot behavior or public header needs to change.
WorldPosition FindLandFromPosition(PlayerbotAI* botAI, float startDistance, float endDistance,
    float increment, float orientation, WorldPosition targetPos, float fishingSearchWindow, bool checkLOS);
