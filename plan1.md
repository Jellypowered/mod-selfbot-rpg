# Material Farming Expansion Plan

## Purpose

Expand `mod-selfbot-rpg` from mining/herbalism node routes into a reusable
**material farming** system. A player should be able to travel to a known farming
zone or area, choose a material such as Linen Cloth or Light Leather, optionally
set a duration or quantity goal, and let the self-bot:

1. discover which creatures can produce the selected material;
2. build a pseudo-route through useful creature spawn clusters;
3. deliberately attack eligible creatures even when they are gray;
4. let stock playerbots combat, corpse-loot, and gathering/skinning behavior run;
5. resume the material objective after combat, loot, death recovery, or movement
   interruption;
6. return to the recorded starting point when the run ends, bags fill, the timer
   expires, or the quantity goal is reached.

This feature is motivated by profession materials that are tedious to obtain at
high character level because stock `grind target` rejects gray mobs through
`Player::isHonorOrXPTarget()`. It must not turn the bot into an indiscriminate
open-world grinder. Only creatures proven to be sources for the selected
material are valid proactive targets.

## Current implementation snapshot (2026-09-05)

### Phase 6 completion audit

Phase 6 is implementation-complete. The addon now exposes the current
`mod-selfbot-rpg.conf.dist` settings in the gear-opened settings window:
`Enable`, `Debug`, `MaterialMinimumChance`, `MaterialReservedBagPercent`,
`AttemptsBeforeBlacklist`, `FailedNodeBlacklistSeconds`,
`EmptyNodeBlacklistSeconds`, `StayInCurrentZone`, `GatherSettleDelayMs`,
`ActionDelayMs`, `FishingBobberEntries`, `UseLures`, `FishingPrioritizePools`,
`FishingOpenWaterOnly`, `FishingSearchDistance`, and `FishingCastDistance`.
Settings are persisted
in `SelfBotRPGDB.Settings`, validated through `SET_CONFIG`, applied to the
current session and future runs, and resettable to documented defaults.

The addon also has capability-gated material controls, correlated catalog/source
frames with terminal empty responses, material status, a separate settings
window, scrollable icon-backed node/material pickers, tooltips, saved panel
state, minimap placement, and non-spammy status refresh. `JLYRPG2` replies are
whisper-routed to the requesting character to avoid party/raid crosstalk.

Phase 6 remaining work is runtime soak validation only: verify settings and
protocol behavior after restart in solo, party, and raid contexts. Unsupported
travel controls are intentionally not exposed as implemented features.

The module now has a working node-farming path and a working material-farming
implementation slice. The addon UI and `JLYRPG2` protocol are implemented for
catalog browsing, source/status requests, material starts, and node-farming
compatibility. The required full worldserver build passes.

Implemented end-to-end behavior includes:

- exact catalog item selection for cloth, leather/hides, cooking materials,
  motes, and transformation-marked primals;
- reverse normal-loot/skinning source discovery with grouped/reference rows;
- same-map/current-zone spawn loading, hotspot planning, and bounded mmap route
  segments;
- selected gray-source combat through a module-owned attack action;
- stock playerbot combat, complete corpse loot, and normal-loot-before-skinning;
- quantity, duration, bag-reserve, diagnostics, and captured-start return;
- explicit stop-and-return for both node and material runs, including fishing
  cast/bobber/loot completion before return;
- fishing water search with configurable search/cast distances, targeted-fish
  and current-zone modes, cataloged-fish accounting that excludes trash, and
  periodic addon material-status updates;
- normal playerbot fishing-pole equip plus main-hand/off-hand restoration via
  proper inventory/equipment swaps;
- capability negotiation and bounded catalog/source protocol frames;
- scrollable icon-backed node-resource and material pickers with tooltips.

The remaining work is primarily wider profession-material soak validation,
custom-database validation, coordination with selfbot-owned additional corpse
skills, and explicit player/source level safety. Fishing open-water and pool
support, optional lure use, catalog/source protocol, and addon controls are
implemented; the status labels below separate implementation completion from
soak-test completion.

### Current capability boundaries

- **Starting location:** material runs require the player to already be in the
  current zone containing eligible sources. The controller loads current-map,
  current-zone spawns only.
- **Travel:** the module can route between reachable hotspots within that scope;
  it does not select another zone, use flight paths or portals, visit vendors,
  or teleport.
- **Level safety:** current target validation checks source identity, hostility,
  attackability, map/zone, line of sight, distance, and normal creature rank,
  but does not yet reject a level-5 player from a level-10 source. High-level
  characters have no special level restriction. A player/source level gate with
  an explicit high-level exemption is planned.
- **Preflight:** source, skill/tool, scope, and hotspot reachability checks are
  implemented; level-appropriateness checks are not yet implemented.

## Current foundation

The existing gathering implementation already provides most shared mechanics:

- reusable `Session/ActivitySession` start capture, duration, return request,
  remaining-time calculation, and return-home detection;
- mmap-validated navigation with rejection of `PATHFIND_SHORTCUT`;
- safe use of useful `PATHFIND_INCOMPLETE` corridors;
- persistent objectives through combat;
- live GUID caches without retained raw object pointers;
- stock playerbots/selfbot ownership of corpse and gathering loot windows;
- full-bag and timed-session return-home behavior;
- revision-based telemetry;
- strict `JLYRPG2` addon traffic isolation;
- persistent minimap and addon settings.

Material farming should generalize these components rather than add a second
monolithic controller beside `SBRPG.cpp`.

## Phase 0 review and implementation status

Phase 0 was reviewed against the current prototype before material work began.
The review found that session extraction and coordinate-based route following
were already present, while strategy ownership and shared lifecycle state were
still embedded in the node-specific `FarmState`.

Completed in the first Phase 0 pass:

- `Session/ActivitySession` remains the reusable start/duration/return utility;
- `Activity/ActivityState` now owns active state, phase, run/revision telemetry,
  reason timestamps, and the activity session;
- node-specific `FarmState` inherits `ActivityState` while retaining source
  compatibility for current node farming;
- `Farm/StrategyLease` now owns exact add/suspend/restore behavior for stock
  `loot` and `gather` strategies;
- `RouteFollower` is confirmed to accept typed objectives as coordinates and
  remains independent of mining/herbalism node data;
- no AFK flag or stand-state manipulation is performed by SBRPG.

Remaining Phase 0 verification is behavioral: run the existing node-farming
matrix after the full module/worldserver link and confirm strategy restoration
when stopping during combat, active loot, return-home, and normal travel.

Phase 1 is complete for the catalog and source-analysis implementation. It
adds `Materials/MaterialCatalog` and `Materials/LootSourceIndex`, plus the
diagnostic command `.sbrpg material sources <name or item ID>`. The source index
is lazy, cached, default-loot-mode aware, includes grouped rows, resolves nested
references with cycle/depth protection, and retains quest-required metadata for
diagnostics. The catalog currently covers cloth, leather/hides, cooking
materials, motes, and transformation-marked primals. Manual Linen source output
was validated by name and item ID.

Phase 2 is complete: scoped creature spawn loading and deterministic
reachability-aware hotspot clustering are implemented. The diagnostic command is
`.sbrpg material hotspots <name or item ID>`. Elwynn validation reported scoped
spawns, reachable mmap anchors, hotspot counts, and output limiting.

Phase 3 implementation is complete. The material controller has an explicit
material target state and a module-owned `AttackAction` that accepts only live,
hostile, non-elite creatures whose loaded source data contains the requested
item. Gray level is allowed only through this selected action. It delegates the
actual attack and corpse-loot handoff to stock playerbots. Extended multi-zone
and long-run runtime validation remains separate.

## Goals

### Required goals

- Farm exact selected materials from creature corpse loot.
- Farm leather, hides, scales, and similar products through stock skinning.
- Support combined sources where a creature can provide the selected material
  from normal loot, harvesting, or both.
- Intentionally permit gray creatures when they match the selected source list.
- Use source-aware pseudo-routes based on creature spawn clusters.
- Keep route and creature objectives through incidental combat.
- Finish ordinary corpse loot before skinning or selecting another target.
- Never manually open, close, replace, or repeatedly reopen a loot window.
- Stop new pulls before returning home.
- Reuse duration and full-bag session termination.
- Add an optional target quantity; zero/empty means no quantity limit.
- Explain every stationary period through phase/reason telemetry.
- Avoid changes to core and, for this feature, avoid requiring further
  `mod-playerbots` source edits.

### Secondary goals

- Rank sources using expected and observed yield rather than nearest distance
  alone.
- Support common Cooking, Tailoring, Leatherworking, Engineering, and Alchemy
  drops after cloth and leather are stable.
- Keep the acquisition model extensible to prospecting, milling, disenchanting,
  and reputation farming without pretending those are identical to creature
  grinding. Fishing is in Phase 7 as a supported gathering mode.
- Allow a future generic item-ID request for server-specific materials.

## Non-goals for the first release

- Killing every nearby creature under stock `grind` rules.
- Quest automation or autonomous zone selection.
- Teleporting to an allegedly optimal farming area.
- Vendor travel, mailing, auctioning, or automatic bag disposal.
- Automatic crafting, milling, prospecting, disenchanting, or conversion of
  scraps into full leather.
- Prospecting, milling, disenchanting, and crafting conversions in the first
  Phase 7 material release.
- Pickpocket loops.
- Instance reset farming.
- PvP targets, player targets, battleground farming, or contested-objective logic.
- Reputation farming in the initial material implementation.
- Guaranteeing an exact drop chance when loot groups, references, conditions,
  or server customizations make the database probability only an estimate.

## Material families and rollout

Use exact item IDs internally. Names and aliases are presentation/input helpers,
not database identities.

### Tier 1: cloth

Initial catalog:

- Linen Cloth (`2589`)
- Wool Cloth (`2592`)
- Silk Cloth (`4306`)
- Mageweave Cloth (`4338`)
- Runecloth (`14047`)
- Felcloth (`14256`)
- Netherweave Cloth (`21877`)
- Frostweave Cloth (`33470`)

Cloth is the first vertical slice because it uses ordinary creature loot and
proves gray-target selection, spawn clustering, combat handoff, corpse looting,
and source metrics without adding a profession-tool requirement.

### Tier 2: leather and skinning products

Phase 4 initial leather slice is implemented for the core leather/hide product
IDs below. Material source selection now honors declared acquisition methods,
and leather runs lease stock `gather` so the normal corpse-skinning lifecycle
can complete after corpse loot.

Initial catalog should include:

- Light, Medium, Heavy, Thick, and Rugged Leather;
- Light, Medium, Heavy, Thick, and Rugged Hides;
- Knothide Leather Scraps and Knothide Leather;
- Borean Leather Scraps and Borean Leather;
- common expansion-appropriate scales and specialty hides.

The server catalog must store exact IDs; the list above is a product grouping,
not a name-based runtime query. Leather validates the full sequence:

```text
kill -> normal corpse loot -> corpse becomes skinnable -> stock gather detects
corpse -> skinning cast -> skinning loot -> corpse release/despawn
```

### Tier 3: profession-focused direct materials

Phase 7 must expand the catalog and source index across the professions most
useful to a selfbot. Each entry is data-driven and must declare its exact item
ID, profession relevance, acquisition method, expansion/tier, source type,
required skill/tool, and whether it is directly farmable. Do not infer that an
item is farmable merely because a profession recipe consumes it.

#### Blacksmithing and engineering metals/reagents

Cover direct creature and gathering sources used by Blacksmithing and
Engineering, including ore/metal nodes, elemental reagents, motes/primals,
engineering salvage, and mechanical creature loot where the server's loot
skill declares Engineering. The node catalog must include copper through
endgame ore tiers present in the database, while creature catalogs include
only confirmed direct-loot or corpse-harvest sources. Prospecting ore remains a
separate transformation and must not be silently performed.

#### Alchemy

Cover direct herb nodes and direct creature/elemental drops used as alchemy
reagents: elemental earth/fire/water/air, essences, motes, primals,
crystallized elements, eternals, venom/poison components, and other
vendor-independent reagents present in the database. Herb gathering uses node
requirements and stock gather behavior; elemental creature sources use normal
loot or the declared corpse skill. Flasks, potions, transmutes, and crafted
conversions are not material-farming sources.

#### Tailoring

Expand cloth through all database-supported tiers, including linen, wool, silk,
mageweave, runecloth, felcloth, netherweave, frostweave, specialty cloth, and
confirmed creature drops such as spider silks. Include cloth sources from
ordinary loot, references, conditions, and custom loot tables in diagnostics.

#### Leatherworking

Expand leather, scraps, hides, scales, carapaces, rugged/specialty hides, and
other skinning products across supported tiers. The source index must retain
required skinning skill, creature level/rank, required tool, multi-yield
behavior, and whether normal loot must precede the corpse skill. Selfbot-owned
corpse-skill support is assumed to cover the additional supported corpse skill
lifecycle; SBRPG still validates requirements and observes completion.

#### Cooking

Cover meats, eggs, fish-derived creature drops, spider legs, clam meat,
seasonal ingredients, and other direct creature/node/fishing ingredients used
by cooking. Keep raw ingredients separate from cooked products and recipes.
Cooking itself is never an implicit farming action.

#### Fishing

Add fishing as a first-class acquisition method rather than treating fish as
creature loot. Support open-water fishing and database-defined fishing holes
when the player has the fishing skill, pole/tool, bait if required, and a valid
water/fishing location. The fishing controller must use normal player fishing
and bobber interaction, observe actual catch/item results, handle failed casts,
water/terrain changes, and resume the quantity/duration goal. It must not
fabricate catches, cast through terrain, or use creature spawn routing.

Fishing source data must include map/zone/area, water or fishing-hole type,
required skill, expected item IDs, seasonal/condition requirements, and a
bounded set of reachable cast positions. Cross-zone fishing uses Phase 10
travel planning; local cast-position movement remains mmap/vmap validated.

#### Shared elemental and reagent families

Cover direct sources for elemental earth/fire/water/air, essences, motes,
primals, crystallized elements, eternals, and custom-database equivalents.
Transformation-marked entries remain linked to their input material but are
not reported as directly farmable.

### Tier 4: unsupported transformations and later acquisition modes

Keep these as separate pipelines with explicit dependency diagnostics:

- pigments from milling herbs;
- gems from prospecting ore;
- enchanting materials from disenchanting items;
- full leather made from scraps;
- crafted intermediate parts;
- transmutes, vendor purchases, reputation rewards, and recipe outputs.

The catalog may describe dependencies, but the controller must reject a direct
request with a precise reason such as `REQUIRES_MILLING`, `REQUIRES_PROSPECTING`,
`REQUIRES_DISENCHANTING`, or `CRAFTED_OUTPUT_NOT_FARMABLE`.

## Core design principle

**SBRPG chooses material, hotspot, and creature. Stock playerbots owns combat and
loot.**

For gray mobs, the module cannot use stock `GrindTargetValue`, because that value
contains an explicit `isHonorOrXPTarget()` requirement. Instead, register a
module-owned action derived from `AttackAction`. The action resolves only the
GUID selected by the material controller and invokes the protected stock
`Attack(target)` helper. This preserves stock attack setup:

- target and selection assignment;
- insertion into `available loot`;
- movement-priority cleanup;
- facing;
- combat-engine transition;
- class combat strategies and pet behavior.

Do not patch `GrindTargetValue`, globally weaken XP/honor checks, or enable a
broad gray-mob grind mode.

## Acquisition model

```cpp
enum class AcquisitionMethod : uint8_t
{
    CreatureLoot,
    Skinning,
    HerbalismCorpse,
    MiningCorpse,
    EngineeringCorpse,
    GameObjectNode,
    Fishing,          // future
    Transformation,   // future
    Reputation        // stretch goal
};

struct MaterialDefinition
{
    uint32_t itemId;
    std::string key;
    std::string displayName;
    MaterialFamily family;
    std::vector<std::string> aliases;
    std::vector<AcquisitionMethod> methods;
};
```

A request is distinct from the catalog entry:

```cpp
struct MaterialRequest
{
    uint32_t itemId;
    uint32_t quantityGoal;       // 0 = unlimited
    uint32_t durationMinutes;    // 0 = unlimited
    Scope scope;                 // area, zone, or same-map advanced mode
    bool allowNeutralTargets;
    bool allowElites;
};
```

Defaults:

- current zone;
- non-elite creatures only;
- hostile creatures, plus neutral attackable creatures only when explicitly
  enabled or when the catalog profile intentionally requires them;
- same map;
- no quantity or duration limit unless supplied.

## Proposed source layout

```text
src/
  Activity/
    ActivityController.h/.cpp      common start/stop/return coordination
    ActivityState.h                common phase/revision/objective fields
    StrategyLease.h/.cpp           add/suspend/restore playerbot strategies
  Session/
    ActivitySession.h/.cpp         existing reusable session lifecycle
  Navigation/
    RouteFollower.h/.cpp           generalized current mmap follower
    SpatialRoutePlanner.h/.cpp     generic anchor ordering/path-cost probes
  Materials/
    MaterialCatalog.h/.cpp         exact items, aliases, capabilities
    LootSourceIndex.h/.cpp         reverse loot/harvest source lookup
    CreatureSpawnRepository.h/.cpp map/zone/area spawn loading
    HotspotPlanner.h/.cpp          density clusters and pseudo-route
    LiveCreatureCache.h             GUID cache and bounded scans
    MaterialTargetSelector.h/.cpp  safe source-specific target ranking
    MaterialFarmState.h             request/objective/metrics/cooldowns
    MaterialFarmAction.h/.cpp      route, select, and stock Attack handoff
    MaterialLootObserver.h/.cpp    requested/incidental yield attribution
  Gathering/
    NodeFarmState.h                 current node-specific state
    NodeRepository.h/.cpp
  Protocol/
    SbrpgProtocol.h/.cpp
    StatusPublisher.h/.cpp
```

The exact migration can be incremental, but reusable session/navigation/status
code must not acquire cloth- or skinning-specific branches.

## Loot source discovery

### Database tables

Creature-drop sources come from:

```text
creature_template.lootid
  -> creature_loot_template.Entry
  -> creature_loot_template.Item / Reference
```

Harvest sources come from:

```text
creature_template.skinloot
  -> skinning_loot_template.Entry
  -> skinning_loot_template.Item / Reference
```

Creature positions come from:

```text
creature.id1/id2/id3, map, zoneId, areaId,
position_x/y/z, spawntimesecs, wander_distance, MovementType
```

At runtime, `CreatureTemplate::GetRequiredLootSkill()` determines whether a
skinnable corpse actually uses Skinning, Herbalism, Mining, or Engineering.
Do not assume every `skinloot` row means Skinning.

### Reverse index

Build an immutable/on-demand reverse index:

```cpp
itemId -> vector<LootSource>

LootSource:
  creatureEntry
  acquisitionMethod
  lootTemplateId
  estimatedChance
  expectedCount
  questRequired
  requiredSkill
```

Requirements:

1. Exclude `QuestRequired != 0` from ordinary material profiles.
2. Require a compatible default loot mode for open-world farming.
3. Resolve `Reference` rows recursively through `reference_loot_template`.
4. Detect reference cycles and cap recursion depth.
5. Include grouped loot entries; do not copy the current playerbots
   `DropMapValue` assumption that inspecting only ungrouped `Entries` is a
   complete source list.
6. Treat zero-chance grouped entries according to AzerothCore loot-group
   semantics rather than as impossible drops.
7. Preserve probability as a ranking estimate, not a promise.
8. Revalidate actual loot through normal server/playerbots behavior.
9. Cache by item ID and invalidate only on world-data reload or explicit debug
   reload.

For the first cloth slice, direct non-reference rows may be implemented first,
but startup diagnostics must state when referenced/grouped rows were omitted.
The final source index is not complete until references and groups are handled.

### Conditions and server customizations

Loot conditions can make a database-listed item unavailable to the current
character. The source score should penalize repeated zero-yield observations,
but one failed roll is not evidence of a bad source. Where practical, inspect
loaded loot templates or conditions using core APIs. Never bypass conditions or
manually award items.

### Source diagnostics

Provide commands such as:

```text
.sbrpg material sources linen
.sbrpg material sources 2589
.sbrpg material hotspots linen
```

Output should include creature entry/name, acquisition method, approximate
chance/count, eligible spawn count in scope, level range, and exclusion reason.
This is essential for custom databases.

## Spawn clustering and pseudo-routes

Creature spawns are not static gathering nodes. They wander, may be dead, may
be engaged by another player, and respawn. Routing every spawn GUID as if it
were a node would create excessive backtracking.

### Cluster construction

1. Load only spawn records for eligible source creature entries on the current
   map and selected scope.
2. Place spawns into a fixed spatial grid.
3. Merge neighboring cells or run a bounded density clustering pass.
4. Compute a hotspot anchor using a medoid/central real spawn rather than an
   arbitrary geometric point that may be off navmesh.
5. Retain member spawn IDs, entries, density, average respawn time, level range,
   wander radius, and estimated material yield.
6. Reject anchors that cannot produce a safe mmap path.
7. Build a cyclic route through the best reachable hotspots using the existing
   bounded route planner.

Suggested initial values, all configurable after testing:

- clustering cell: 80 yards;
- neighboring merge radius: 120 yards;
- minimum useful cluster: 3 eligible spawns;
- live scan radius: playerbots sight distance, minimum 30 yards;
- empty-hotspot dwell: 5–10 seconds;
- hotspot cooldown: derived from average spawn time, capped to avoid long idle
  waits;
- candidate path probes: 12–20 anchors per planning slice.

### Hotspot score

Use a transparent bounded score, for example:

```text
score = expected target items per cycle
      * reachable spawn density
      * observed yield modifier
      * level safety modifier
      / (travel cost + estimated kill time + respawn pressure)
```

Do not overfit theoretical drop rates. Track session observations per creature
entry and hotspot:

- kills;
- requested items looted;
- harvest successes;
- combat seconds;
- travel seconds;
- unreachable/contested targets;
- zero-yield streak.

Use an EWMA or similarly bounded adjustment. Never permanently blacklist a
valid source merely because several random drops failed.

### Route behavior

- Persist the selected hotspot until its local sweep completes or it becomes
  unreachable.
- At the hotspot, prioritize live eligible creatures over the next anchor.
- When no valid creature is loaded, wait only for the bounded dwell period and
  then advance.
- Revisit hotspots according to their cooldown/respawn estimate.
- Caves and mineshafts remain valid when their anchors are connected by mmaps.
- Route to real spawn/cluster anchors through cached mmap corridors; never draw
  direct splines across terrain.
- A true map change stops the activity safely.

## Live creature selection

Cache GUIDs, never `Creature*` pointers. Re-resolve and validate every candidate
before movement or attack.

A proactive material target must:

- be a creature entry in the selected material source set;
- be alive, spawned, in world, and on the current map;
- be attackable through `Player::IsValidAttackTarget()`;
- not be a player, pet, totem, critter, training dummy, trigger, escort, or
  scripted non-combat actor;
- not be friendly;
- pass the current configured rank policy (normal creatures by default);
- note that player-versus-creature level appropriateness is not yet enforced;
- not be a boss, rare elite, world boss, or elite unless explicitly permitted;
- not already be tapped/owned by another unrelated player or group;
- not already have more attackers than allowed by group policy;
- not be evading, unreachable, crowd-controlled by another player, or in a
  prohibited area;
- have a safe path to a pull/attack position;
- remain within the active hotspot/leash radius.

Gray level is deliberately **not** an exclusion when all other material-target
checks pass.

Ranking order:

1. attackers already threatening the bot or party;
2. the persistent selected material target;
3. nearby safe source creatures with the best expected yield/time;
4. distance and path cost;
5. lower add risk and lower target contention.

Do not preempt stock combat to switch to a higher-scoring material creature.

## Combat handoff

Implement a module-owned `MaterialFarmAction : public AttackAction` or a small
paired controller/attack action. When a selected creature is in a valid pull
position:

1. store its GUID as the persistent material objective;
2. invoke `Attack(target)` once;
3. let `AttackAction` add the GUID to `available loot`;
4. yield to the combat engine;
5. preserve hotspot, route cursor, and source identity;
6. after combat, allow stock loot to preempt material movement;
7. resume the same material objective lifecycle until corpse loot/harvest is
   complete, then choose the next target.

Do not enable stock `grind` solely for this mode. It selects unrelated targets
and rejects gray ones. If `grind`, `rpg`, `follow`, or another non-combat travel
strategy conflicts with material movement, use `StrategyLease` to suspend only
what is necessary and restore exactly what was present before the run.

Incidental aggro remains normal stock combat. An incidental creature may be
looted, but it counts as a material kill only when it is a valid source. If it
drops the selected item, count the requested item yield regardless of whether
it was the planned target.

## Corpse loot and harvesting lifecycle

Stock `loot` must always outrank material routing. Stock `gather` is enabled for
harvest-capable material runs and restored afterward according to strategy
ownership.

### Creature-loot-only objective

```text
TargetSelected -> ApproachingTarget -> Pulling -> CombatPaused
-> CorpseLootPending -> Looting -> TargetComplete
```

Completion requires stock loot target/stack resolution or corpse disappearance.
A kill alone is not completion.

### Skinning/harvest objective

```text
TargetSelected -> ApproachingTarget -> Pulling -> CombatPaused
-> CorpseLootPending -> Looting -> HarvestPending -> Harvesting
-> TargetComplete
```

Requirements:

- normal corpse loot completes first;
- the corpse becomes `UNIT_FLAG_SKINNABLE`;
- `LootObject` determines required skill and required skill value;
- stock `gather` discovers/re-adds the skinnable corpse;
- stock `OpenLootAction` performs the skinning/herbalism/mining/engineering cast;
- the complete harvest loot window is processed;
- SBRPG observes results but never casts or reopens the corpse itself.

Before starting a harvest run, validate:

- the relevant profession exists;
- current skill meets at least one selected source's requirement;
- required tool is present (skinning knife, mining pick, or accepted equivalent);
- at least one eligible spawn is reachable.

If some source levels are too high, exclude only those sources and report the
count. If all are excluded, reject START with a clear error.

### Stale corpse handling

Use bounded observation rather than active loot manipulation:

- keep target GUID/source identity while stock loot is possible;
- wait while a loot GUID/window is active;
- after normal loot, allow a short stock gather discovery interval;
- if the corpse is no longer skinnable, mark harvest complete;
- if another player skins/despawns it, mark externally completed;
- on timeout, cool down that spawn and continue;
- never repeatedly call `open loot`, set `loot target`, or clear all available
  loot.

## State machine

Generalize shared phases but retain mode-specific substates:

```text
Stopped
Validating
LoadingSources
PlanningHotspots
BuildingPath
TravellingToHotspot
SweepingHotspot
SelectingCreature
ApproachingCreature
Pulling
CombatPaused
CorpseLootPending
Looting
HarvestPending
Harvesting
Recovering
WaitingForRespawn
Returning
Completed
Failed
```

Persistent material state owns:

- activity/run ID and revision;
- `ActivitySession`;
- selected item/family/request;
- source entries and acquisition methods;
- hotspot route and cursor;
- current hotspot and creature GUID/spawn ID;
- objective stage (alive, dead, corpse looted, harvest pending, complete);
- route follower and path revision;
- per-spawn and per-hotspot cooldowns;
- source/hotspot observation metrics;
- strategy lease;
- requested item count at start and acquired count;
- incidental loot count/value if exposed;
- last progress timestamp and exact reason.

Combat changes phase but does not erase the creature or hotspot objective.

## Session termination and bags

Reuse `ActivitySession` for all modes. Return is requested when any enabled
condition fires:

- duration expires;
- target quantity is reached;
- bags cross the configured reserve threshold;
- explicit Stop-and-return command;
- unrecoverable source/path failure when configured to return.

Before return:

1. stop selecting/pulling new creatures immediately;
2. finish current combat;
3. give stock corpse loot/harvest a bounded chance to finish;
4. do not abandon an open loot window;
5. begin safe return-home travel;
6. restore leased strategies only when activity ownership ends;
7. publish a final summary at the start point.

Use a reserve-slot policy for creature farming so the final target item is not
lost before the current `100% full` test triggers. Start conservatively with one
or two free normal-bag slots, configurable after testing. Do not destroy items
to make room.

A quantity goal counts units of the exact requested item observed in loot events,
not number of corpses. Keep `quantityGoal = 0` as unlimited.

## Metrics

Track separately:

- eligible creatures killed;
- incidental creatures killed;
- corpses fully looted;
- harvest attempts/successes;
- requested material units;
- incidental item units;
- requested items per minute;
- kills per minute;
- requested items per eligible kill;
- travel/combat/loot/wait time;
- current source entry and hotspot;
- source zero-yield streak;
- unreachable/contested skips.

`PlayerScript::OnPlayerLootItem` should attribute exact item ID/count and loot
GUID. A creature GUID can map back to the active source/objective. Do not infer
success merely from `OpenLootAction` returning true.

Example reasons:

- `Planning Linen Cloth hotspots: 84 eligible spawns in Silverpine Forest`
- `Travelling to Linen Cloth hotspot 2/5 — 173 yd`
- `Sweeping hotspot — 4 eligible humanoids visible`
- `Pulling Rot Hide Gnoll (gray; selected Linen Cloth source)`
- `Combat paused — material target retained`
- `Corpse loot pending — stock playerbots owns loot`
- `Harvest pending — waiting for stock skinning discovery`
- `Waiting 6 sec — hotspot recently cleared`
- `Returning — target quantity 80/80 reached`

## Addon design

Add a second mode/tab without altering the existing mining/herbalism START
format.

### Farming panel

```text
Mode:       [Gathering | Materials]
Family:     [Cloth | Leather | Cooking | Elemental | Other]
Material:   searchable dropdown / item link input
Scope:      [Current area | Current zone]
Duration:   minutes, empty = unlimited
Quantity:   units, empty = unlimited
Options:    [ ] Allow neutral source creatures
            [ ] Allow normal elites
Start / Stop
```

Show preflight results before or immediately after START ACK:

- material and exact item ID;
- acquisition method(s);
- profession/tool requirement;
- source entries and spawn count in scope;
- reachable hotspot count;
- excluded high-level/elite/conditioned sources.

### Protocol additions

Preserve existing protocol v1 operations. Add distinct opcodes rather than
changing the positional meaning of current `START`:

```text
JLYRPG2\t1\tMATERIAL_CATALOG\t<requestId>
JLYRPG2\t1\tSTART_MATERIAL\t<requestId>\tmaterial\t<material>\t<durationMinutes>\t<quantity>
JLYRPG2\t1\tMATERIAL_SOURCES\t<requestId>\tmaterial\t<material>
```

Catalog/source replies stay below the addon-message payload limit by sending
bounded ordered rows with request ID, sequence, and total. The server advertises
`MATERIAL_CATALOG`, `MATERIAL_SOURCES`, `START_MATERIAL`, and `MATERIAL_STATUS`
through the `CAPABILITIES` response. Older addons remain able to use node
farming; the addon does not automatically fall back to chat.

Use a separate material status opcode or an explicitly versioned key/value
payload so appending fields cannot shift the existing positional STATUS parser.

Manual fallback examples:

```text
.sbrpg material linen
.sbrpg material "frostweave cloth" 60
.sbrpg material item 2589 duration 30 quantity 80 scope zone
.sbrpg material stop
```

All normal resource names remain exact one-field values in addon transport.

## Safety and coexistence

- Never target players.
- Never target friendly creatures.
- Neutral attacks require explicit policy and a clear addon warning.
- Skip tapped/claimed creatures and avoid kill stealing.
- Respect party combat and loot ownership.
- Do not attack elites/bosses by default.
- Keep a bounded pull radius and avoid chain-pulling while recovering.
- Pause proactive pulls when health, mana, pet state, resurrection sickness, or
  durability policy says recovery is needed; let stock consumable actions act.
- Stock combat and loot actions always outrank material movement.
- Suspend only conflicting strategies and restore their exact prior state.
- Do not alter global playerbots delays, grind rules, sight distance, or loot
  filters.
- Do not manipulate AFK flags; current playerbots owns self-bot AFK behavior.
- No GM/admin teleport, clipping, synthetic relocation, or shortcut completion.
  Phase 10 may use legitimate hearthstones, portals, area triggers, and flying
  mounts only after normal-player availability and arrival validation.
- Preserve strict addon isolation in solo, party, raid, and battleground chat.
- Do not run proactive material farming in battlegrounds/arenas.

## Performance limits

- Load source index once/on demand, not per controller tick.
- Load scoped spawn records once per start/scope change.
- Cluster spawns once per plan revision.
- Live scan no faster than twice per second and only in relevant phases.
- Cache GUIDs, IDs, and coordinates; never retain raw world-object pointers.
- Bound candidate creatures and mmap probes per update.
- Reuse the active mmap corridor until completion, displacement, or timeout.
- Evaluate observed source scores only after a kill/loot event or low-rate
  planning interval.
- Publish status on revision plus low-rate countdown heartbeat.
- Cap source-reference recursion and catalog response size.

## Reputation farming stretch goal

The Discord discussion correctly notes that the same route/target machinery
could support reputation farming, but reputation is not merely another item.
Implement it later as `GoalType::Reputation` with its own source repository:

```text
creature_template / creature spawns
  + creature_onkill_reputation
  + faction/reputation cap and team rules
```

A reputation request must account for:

- which faction receives reputation;
- per-kill amount and team-dependent faction fields;
- maximum standing/cap;
- gray-level reputation reduction rules;
- championing/tabard or aura effects;
- current standing and target standing/quantity;
- creatures that grant loot materials and reputation simultaneously.

Reuse hotspot planning, safe target selection, combat handoff, session return,
and metrics, but do not overload `MaterialDefinition` or pretend an item loot
hook proves reputation progress. Observe actual reputation change events/state.

Default safety should be stricter than material farming because reputation
profiles may include neutral factions or dense camps.

## Implementation phases

### Phase 0 — Refactor shared foundations

1. Move common activity/session/return fields out of node-specific `FarmState`.
2. Generalize route follower input from node spawn to typed coordinate objective.
3. Add `StrategyLease` with exact add/suspend/restore ownership tests.
4. Keep existing mining/herbalism behavior unchanged.
5. Complete full-link and in-game regression validation for strategy restoration,
   return-home, combat pause/resume, corpse loot, and gathering.

**Status:** implementation pass complete; runtime regression validation pending.

**Exit:** current node farming passes its existing runtime matrix with no
behavior regression.

### Phase 1 — Material catalog and source index

1. Add exact starter cloth catalog. **Implemented.**
2. Build direct creature-loot reverse lookup. **Implemented.**
3. Add reference/group traversal and cycle/depth guards. **Implemented.**
4. Filter quest-required/incompatible loot modes. **Implemented for default loot
   mode; quest metadata is retained for diagnostics.**
5. Add source diagnostics and pure source-index tests. **Diagnostics implemented;
   pure tests remain.**

**Status:** implementation slice complete. Manual runtime validation passed for
`.sbrpg material sources linen` and `.sbrpg material sources 2589`; both resolve
to the same exact item and produce consistent source output. Automated tests and
additional reference/group/custom-database validation remain pending.

**Exit:** every starter cloth item produces a reproducible, explainable creature
source list on the active world database.

### Phase 2 — Spawn repository and hotspot planner

1. Load same-map area/zone spawn records for source entries. **Implemented.**
2. Build bounded spatial clusters and real-spawn anchors. **Implemented.**
3. Score/order reachable hotspots. **Implemented as bounded nearest reachable
   ordering; adaptive yield scoring remains future work.**
4. Add dry-run route and exclusion diagnostics. **Implemented.**

**Status:** diagnostic implementation complete. Runtime validation passed in
Elwynn zone 12: Linen Cloth resolved to 588 spawns and 65 hotspots, with
reachable mmap anchors reported and output limiting functioning. The user build
completed successfully without errors.

**Exit:** `.sbrpg material hotspots linen` shows stable reachable clusters and
never selects an off-map/off-scope anchor.

### Phase 3 — Cloth combat vertical slice

1. Register module-owned material strategy/action. **Implemented.**
2. Select only live eligible source creatures. **Implemented for nearby
   same-zone targets.**
3. Bypass the gray XP gate only through the selected material action.
   **Implemented through the derived `AttackAction`.**
4. Invoke stock `AttackAction::Attack()` and yield to stock combat.
   **Implemented.**
5. Preserve target/hotspot through combat and incidental aggro. **Target GUID
   persists through combat; hotspot travel integration remains pending.**

**Status:** nearby-source combat slice implemented and runtime-validated. Material
actions yield to stock selfbot recovery while eating, drinking, or sitting below
full health/mana; they do not issue movement or attack requests until recovery
finishes.
Proactive source selection now applies the configurable
`SelfBotRpg.MaterialMinimumChance` threshold (default `1.0%`) so very low
world-loot chances are diagnostic-only rather than attack objectives. When no
valid creature is visible, the controller now follows reachable hotspot anchors
through the shared mmap-safe `RouteFollower`, then delegates actual movement
to stock `MovementAction` pacing rather than directly launching a spline; it
rescans at each anchor.

**Exit:** a high-level selfbot intentionally kills gray Linen sources but ignores
nearby creatures that cannot drop Linen.

### Phase 4 — Corpse lifecycle and cloth metrics

1. Ensure stock `loot` has unconditional priority over material travel.
   **Implemented.**
2. Observe complete corpse lifecycle without setting loot target.
   **Implemented, including the skinning re-queue handoff.**
3. Attribute exact requested item quantities. **Implemented through the loot
   item hook.**
4. Add quantity goal, reserve bag slots, and final summary. **Complete.**
   Quantity goals, configurable bag-use reserve, material status, kill/loot/corpse
   metrics, final summaries, and timeout/empty-corpse handling are implemented.
5. Validate timed/full-bag/quantity return-home behavior. **Complete for the
   server-side lifecycle.** Session timers, quantity goals, bag reserve, combat
   interruption handling, corpse completion, and mmap return-home behavior are
   implemented; extended live soak testing remains a follow-up.

**Status:** Phase 4 complete. The controller now yields to stock loot, handles
empty corpses, permits combat during return, completes post-combat loot/skinning,
and resumes the captured return route.

### Phase 5 — Leather and corpse harvesting

1. Add leather/hide catalog and skinning source index. **Complete.**
2. Validate skill/tool/source-level requirements at start. **Complete.**
3. Enable stock `gather` through strategy lease. **Complete.**
4. Observe normal-loot-to-skin transition and multi-yield skinning. **Implemented;
   runtime validation is ongoing with the new empty-corpse and return-combat
   handling.**
5. Consume herbalism/mining/engineering corpse skill results without
   hardcoding every skinnable creature as Skinning. **Implemented; the source
   index classifies corpse loot from `GetRequiredLootSkill()`, validates the
   required profession/tool, and yields to the selfbot/playerbot gather
   lifecycle.**

**Status:** Phase 5 implementation complete. Leather/hide discovery, source
indexing, profession/tool prerequisites, Herbalism/Mining/Engineering corpse
classification, strategy ownership, normal-loot-before-harvest, and stale-corpse
recovery are implemented. Extended live/custom-database runtime validation
remains a soak-test follow-up.

### Phase 6 — Addon and protocol

The implementation also includes a bounded icon-backed, scrollable node-resource
picker and searchable scrollable material picker. Material icons use item IDs
with `GetItemIcon`/`GetItemInfo` fallback lookup, following the item-picker
pattern used by PBAltManager. Tooltips expose material family, acquisition
method, item ID, and control behavior. The panel uses a bounded idle fade that
remains fully opaque while the cursor is over the frame or an open
picker/edit control.

1. Add material controls and catalog-backed material selection. **Implemented.**
2. Implement capability negotiation and chunked catalog/source responses.
   **Implemented with capability-gated controls, indexed `MATERIAL_CATALOG` and
   `MATERIAL_SOURCE` frames, request correlation, and a terminal
   `MATERIAL_SOURCES_END` frame for empty/completed source responses.**
3. Implement `START_MATERIAL` and material status. **Implemented with duration
   and quantity goals.**
4. Preserve existing gathering protocol and manual-only fallback. **Complete;
   node `START`/`STATUS` and `/sbrpgchat` remain isolated.**
5. Validate solo/party/raid crosstalk isolation and saved settings. **Server
   replies are WHISPER-routed, addon controls are capability-gated, and panel
   position/form selections persist in `SelfBotRPGDB`; live solo/party/raid
   soak validation remains.**
6. Expose the complete current module configuration in the addon settings
   overlay. **Implemented:** the gear-button overlay provides Apply, Close, and
   Reset Defaults; all `.conf.dist` values, including Enable, Debug, bobber
   entries, and optional UseLures, persist in `SelfBotRPGDB.Settings`. A new
   capability-gated `SET_CONFIG` operation validates and applies values to the
   current module session and future runs. The config file remains the startup
   default source after a worldserver restart.

**Status:** Phase 6 implementation and polish complete; runtime protocol soak
testing remains. The addon selects exact catalog materials, requests source
metadata, gates controls from server capabilities, persists panel/settings,
handles indexed catalog/source frames, and retains the existing node-farming
controls. Runtime configuration is editable from the gear-button settings
window through validated `SET_CONFIG`; unsupported older servers leave that
control unavailable rather than falling back to chat. Material and node
requests remain isolated; server replies use WHISPER routing to prevent party
or raid crosstalk. Status is requested on panel
open rather than emitted as a periodic chat/status stream.

### Phase 7 — Profession material coverage

Phase 7 now treats profession materials as several acquisition modes rather than
one creature-drop list. Selfbot owns the additional supported corpse-skill
lifecycle; SBRPG owns material intent, source indexing, eligibility, planning,
quantity accounting, and diagnostics.

1. **Catalog expansion**
   - exact-ID catalogs now cover the implemented Blacksmithing, Alchemy,
     Tailoring, Engineering, Leatherworking, Cooking, and Fishing families;
   - fishing entries distinguish open-water and pool-associated sources;
   - custom-database additions through external data/configuration are deferred
     intentionally; the current catalog remains compiled in code;
   - future catalog additions must carry authoritative item IDs, family,
     acquisition methods, and source diagnostics.
2. **Acquisition classification**
   - mark each entry as `CreatureLoot`, `Skinning`, `HerbalismCorpse`,
     `MiningCorpse`, `EngineeringCorpse`, `GameObjectNode`, or `Fishing`;
   - mark hybrid sources when an item can come from multiple methods;
   - mark milling, prospecting, disenchanting, crafted, vendor, reputation,
     and other unsupported outputs with explicit dependency reasons.
3. **Profession and tool preflight**
   - validate learned profession, current skill, tool/pole, bait, node lock,
     corpse required skill, and source-specific requirements before movement;
   - distinguish “profession not required for direct loot” from “profession is
     required for harvest/fishing.”
4. **Corpse-skill integration**
   - consume selfbot's supported additional corpse-skill results and lifecycle;
   - identify the required skill from `CreatureTemplate::GetRequiredLootSkill()`
     and source metadata rather than assuming every corpse is Skinning;
   - preserve normal-loot-before-harvest, complete multi-yield processing, and
     corpse release/despawn observation.
5. **Fishing vertical slice**
   - implement open-water and fishing-hole source records, cast-position
     planning, normal fishing/bobber interaction, catch attribution, failed-cast
     recovery, and quantity/duration/bag termination;
   - keep fishing movement and casts separate from creature hotspot targeting;
   - support targeted fish and current-zone fishing sessions, with open water as
     the default and optional pool prioritization; expose distinct addon start
     controls and reject missing fish/zone selections.
6. **Profession-family rollout**
   - stabilize Tailoring cloth and Cooking direct ingredients first;
   - add Leatherworking hides/scales and Alchemy elemental/herb sources;
   - add Blacksmithing/Engineering metals, nodes, salvage, and reagents;
   - add Fishing and custom-database source validation;
   - expose only families whose source diagnostics and runtime lifecycle pass.
7. **Diagnostics and protocol**
   - report acquisition method, profession/tool requirement, source skill,
     item-ID provenance, expected yield, unsupported transformations, and
     rejected sources in plan/status/catalog responses;
   - retain chunking and whisper isolation for expanded catalogs and sources.
8. **Validation**
   - validate grouped/reference loot, conditions, multi-yield harvests,
     custom database rows, fishing catches, bag handling, and return-home
     behavior for every enabled profession family.

**Status:** Phase 7 implementation now includes expanded cloth, leather/hide,
cooking, elemental, Alchemy reagent, Engineering salvage, ore/herb node, and
Fishing catalogs. Fishing supports open water and matching pools, with optional
lures that never block fishing. Remaining Phase 7 work is broader live/custom-
database soak validation and source-condition coverage, not external catalog
loading. Material requests for database-linked ore/herb nodes bridge to
the existing safe node controller with item-specific quantity tracking.
Fishing has a module-owned cast/bobber/reel/loot loop with skill, pole,
optional lure application, quantity, duration, and bag checks. It now supports
selected-fish and current-zone sessions, defaults to open water, and optionally
prioritizes matching pools before open-water fallback. The module equips a
valid pole through the normal playerbot fishing action and restores the prior
main-hand/off-hand equipment when the session ends. Lures are best-effort: an
absent or unusable lure never blocks fishing. Fishing resolves
`fishing_loot_template` pool entries, routes through current-zone spawned pool
points when available, and falls back to open-water casting when no matching
pool is present. Recent refinement adds node-material source reporting, Mining/Herbalism
skill/tool preflight, safer active-bobber handling, pool-miss fallback, and
periodic pool rediscovery. The optional persisted `UseLures` setting controls best-effort lure
application immediately before each cast without making lures mandatory.
`FishingSearchDistance` bounds water discovery and `FishingCastDistance` bounds
shoreline cast-point selection. Remaining work is expanded custom-database
validation and live profession-family soak testing.
External catalog/config-file loading is explicitly deferred; the current
catalog remains compiled into the module.

**Exit:** every exposed material has at least one valid reachable source or is
rejected before movement with a precise reason.

### Phase 8 — Adaptive scoring and polish

1. Feed observed yield/kill/travel data into bounded source ranking.
2. Improve hotspot cooldowns using respawn observations.
3. Tune post-combat, corpse, and empty-hotspot delays.
4. Add summary export/debug logging.

**Exit:** repeated runs avoid consistently poor/unreachable hotspots without
permanently excluding valid low-chance sources.

### Phase 9 — Reputation prototype

1. Add separate reputation goal/source model.
2. Resolve `creature_onkill_reputation` rules and caps.
3. Reuse creature targeting/hotspot routing.
4. Observe actual reputation gains and stop at goal/cap.
5. Keep this feature disabled/experimental until a dedicated safety matrix
   passes.

## Phase 10 — Travel capabilities and level-aware activity

### Purpose

Phase 10 removes the current-zone-only limitation using only travel methods a
regular player can use: walking, ground/flying mounts where permitted, known
flight paths, hearthstones, player portals, ships, zeppelins, transports, and
other validated public interactions. It never uses GM/admin movement or
synthetic relocation. The planner may chain legitimate shortcuts when they are
actually available and faster—for example, hearthstone to a bound town, then
use a normal portal and continue by mount. The phase adds explicit travel
planning for node farming, material farming, and an optional constrained grind
goal. It also makes proactive target selection aware of player level, source
level, profession requirements, and configured risk.

The result must answer these questions before a run starts:

```text
Can this character safely farm this target now?
Where is the best eligible source zone?
Can the stock playerbot travel graph reach that zone?
Which transfer modes are available and permitted?
What will be farmed/collected after arrival?
Why was a destination or source rejected?
```

### Non-negotiable ownership boundary

> **SBRPG owns intent, eligibility, destination scoring, policy, and telemetry.
> mod-playerbots owns execution of normal player travel: walking/mount movement,
> taxis, ships, zeppelins, and validated transport interactions, as well as
> combat and quest/loot packet handling.**

The permitted-travel rule is strict: every Phase 10 leg must be achievable by a
normal player from the bot's actual location, learned abilities, inventory,
known routes, and current state. Hearthstones, player portals, and legitimate
area-trigger/transport interactions are allowed when their normal requirements
are met. SBRPG must not create or inject those effects: it requests or invokes
the same stock player-facing action and verifies the resulting state change.
Do not add GM/admin movement, raw-coordinate relocation, synthetic teleport
packets, custom flight movement, or synthetic transport completion to SBRPG.
Do not modify global playerbot travel selection, `GrindTargetValue`, or
movement actions. The module may set a well-defined `TravelTarget` only while
it owns a Phase-10 travel lease, then must restore the target/strategy state it
displaced.

### Existing mod-playerbots integration points

The implementation must build on the following existing APIs rather than
recreating their behavior:

| Need | Existing playerbot facility | Intended Phase 10 use |
|---|---|---|
| Travel destination/state | `Mgr/Travel/TravelMgr.h`: `TravelDestination`, `TravelTarget`, `TravelStatus`, `TravelState` | Represent the selected farming zone/anchor as a module-owned destination and track prepare/travel/work/cooldown/expire state. |
| Travel execution | `Ai/Base/Strategy/TravelStrategy.cpp`, `MoveToTravelTargetAction` | Lease stock `travel` only for SBRPG's explicit target; preserve stock destination selection and safe movement/loot preemption. |
| Graph/transfers | `TravelMgr::mapTransDistance`, `fastMapTransDistance`, `addMapTransfer`, `loadMapTransfers` | Score only travel-graph-connected map/zone candidates; never fabricate map links. |
| Flight capability | `GetNearestFlightMasterInfo`, `GetOptimalFlightDestinations`, `GetFlightNodesInZone` | Preflight discovered flight masters/routes and optionally let stock travel/taxi handling execute an allowed route. |
| Level-aware locations | `TravelMgr::GetLocsPerLevelCache(level)` and `PrepareZone2LevelBracket` | Seed level-appropriate zone candidates; source evidence still wins over generic bracket data. |
| Destination selection | `getQuestTravelDestinations`, `getGrindTravelDestinations`, `TravelDestination::isActive/isFull/nextPoint` | Reuse availability, visitor, cooldown, and point-selection semantics where compatible; do not alter their global destination sets. |
| Movement safety | `MoveToTravelTargetAction`, `MovementAction`, current SBRPG `RouteFollower` | Use stock travel for inter-zone/map legs and current SBRPG mmap-bounded hotspot movement only after arrival. |
| Target-level baseline | `Player::isHonorOrXPTarget`, playerbot `GrindTargetValue` | Use only as a reference for normal grind mode; material targets retain their selected-gray exception but gain an SBRPG-local over-level rejection policy. |

Relevant source locations to re-audit against the pinned playerbot revision at
implementation time:

```text
modules/mod-playerbots/src/Mgr/Travel/TravelMgr.h/.cpp
modules/mod-playerbots/src/Ai/Base/Strategy/TravelStrategy.cpp
modules/mod-playerbots/src/Ai/Base/Actions/ChooseTravelTargetAction.cpp
modules/mod-playerbots/src/Ai/Base/Actions/MoveToTravelTargetAction.cpp
modules/mod-playerbots/src/Ai/Base/Actions/TravelAction.cpp
modules/mod-playerbots/src/Ai/Base/Value/GrindTargetValue.cpp
modules/mod-playerbots/src/Ai/World/Rpg/Action/NewRpgBaseAction.cpp
```

### Scope and user-facing modes

Keep current behavior as the conservative default. Add an explicit scope/mode,
not an implicit expansion of current-zone runs:

```text
Current zone       current implementation behavior; no external travel
Current map        select an eligible zone/anchor on the current map only
Connected travel   permit stock travel graph/map transfers and allowed taxis
Manual destination player provides a zone ID/name; preflight may reject it
```

Suggested commands/protocol additions (exact names may be refined, but do not
change existing positional `START` or `START_MATERIAL` frames):

```text
.sbrpg material plan <material> [scope]
.sbrpg material start <material> [duration] [quantity] [scope]
.sbrpg farm <profession> <resource> [scope]
.sbrpg travel status
.sbrpg travel cancel

JLYRPG2  TRAVEL_CAPABILITIES
JLYRPG2  MATERIAL_PLAN
JLYRPG2  START_MATERIAL_V2   requestId material name duration quantity scope
JLYRPG2  TRAVEL_STATUS
JLYRPG2  TRAVEL_CANCEL
```

`START_MATERIAL` and existing node `START` retain current-zone semantics for
backward compatibility. New scope behavior must be capability-negotiated and
shown in the addon before it is selectable.

### Data model

Add a generic planned-activity layer, separate from per-mode farm state:

```cpp
enum class ActivityScope : uint8_t
{
    CurrentZone,
    CurrentMap,
    ConnectedTravel,
    ManualZone
};

enum class TravelLegKind : uint8_t
{
    StockGroundTravel,
    StockMapTransfer,
    StockTaxi,
    ArrivalHotspot
};

struct LevelPolicy
{
    uint8_t maxSourceLevelAbovePlayer;   // low/mid-level safety ceiling
    uint8_t highLevelExemptAt;           // e.g. max-level / configurable threshold
    bool permitGraySourcesAtHighLevel;
    bool permitEliteSources;
};

struct SourceProfile
{
    uint32_t creatureEntry;
    uint8_t minLevel;
    uint8_t maxLevel;
    CreatureEliteType rank;
    uint32_t requiredSkill;
    uint32_t requiredSkillValue;
    float estimatedYield;
};

struct ActivityDestination
{
    uint32_t mapId;
    uint32_t zoneId;
    uint32_t areaId;
    WorldPosition anchor;
    std::vector<Hotspot> hotspots;
    float travelCost;
    float sourceScore;
    bool reachable;
    std::string rejectionReason;
};

struct SbrpgTravelState
{
    ActivityScope scope;
    ActivityDestination destination;
    std::vector<TravelLegKind> plannedLegs;
    TravelTargetSnapshot displacedTarget;
    StrategyLease travelStrategy;
    bool awaitingArrival;
    bool arrivalConfirmed;
    uint32_t noProgressSinceMs;
    uint8_t retryCount;
    uint32_t estimatedSeconds;
    std::string selectedRouteReason;
};

// A route is a sequence of ordinary player actions, not a relocation command.
enum class PlayerTravelMethod : uint8_t
{
    Walk,
    GroundMount,
    FlyingMount,
    FlightPath,
    Hearthstone,
    PlayerPortal,
    AreaTrigger,
    Ship,
    Zeppelin,
    Ferry,
    PublicTransport
};

struct TravelLeg
{
    PlayerTravelMethod method;
    WorldPosition start;
    WorldPosition expectedArrival;
    uint32_t expectedMapId;
    uint32_t expectedZoneId;
    uint32_t estimatedSeconds;
    bool requirementsMet;
    std::string requirementSummary;
};
```

`TravelTargetSnapshot` must capture whether a stock travel target was active,
its destination/position/status/forced flag, and only restore it if SBRPG owns
the replacement. Never overwrite a party leader's forced/group-copied target.
If a group leader, follow strategy, active quest travel target, or explicit
master command owns travel, reject or pause the SBRPG travel request with a
visible reason rather than stealing it.

### Level-aware farming, gathering, and grinding

#### Policy

1. Resolve source creature templates before loading spawn candidates.
2. For non-exempt characters, reject a source if its **minimum** possible level
   is above `playerLevel + configuredDelta`; use max level for preferred-source
   ranking, not a blanket rejection when a template has a broad range.
3. Reject elite/rare/boss sources unless the mode explicitly permits them.
4. For Skinning/harvest sources, separately enforce profession skill/tool and
   source required skill; character combat level does not replace profession
   requirements.
5. For high-level/exempt characters, allow lower-level gray sources when they
   match the exact requested material. This preserves the material-farming
   reason for existence. The same exemption applies to gray quests: a high-level
   character may accept/complete a gray quest when its prerequisites and
   objective safety checks pass.
6. For optional general grinding, use the stricter of SBRPG's level policy and
   the stock `isHonorOrXPTarget()` semantics. Do not make material gray-target
   exceptions global.
7. Revalidate the live creature's actual level/rank immediately before a pull.
   Template validation is only preflight/ranking data.
8. If every source is rejected for level/rank, do not move. Return a structured
   `NO_LEVEL_APPROPRIATE_SOURCE` response containing nearest source bracket and
   required level range.

Suggested initial defaults, all config-backed and documented:

```ini
SelfBotRpg.LevelAwareFarming = 1
SelfBotRpg.MaxSourceLevelsAbovePlayer = 2
SelfBotRpg.HighLevelGraySourceExemptAt = 70
SelfBotRpg.AllowEliteSources = 0
SelfBotRpg.AllowCrossZoneTravel = 0
SelfBotRpg.AllowTaxiTravel = 0
SelfBotRpg.AllowHearthstoneTravel = 1
SelfBotRpg.AllowPlayerPortalTravel = 1
SelfBotRpg.AllowAreaTriggerTravel = 1
SelfBotRpg.AllowPublicTransportTravel = 1
SelfBotRpg.AllowFlyingMountTravel = 1
SelfBotRpg.AllowGrayQuestAtHighLevel = 1
SelfBotRpg.AllowGroupQuestAssist = 0
SelfBotRpg.MaxTravelDistance = 0        # 0 = policy-derived / no arbitrary direct move
```

The high-level exemption must be configurable rather than hard-coded to a
specific expansion cap. `HighLevelGraySourceExemptAt = 0` disables the
exemption for testing. The policy must be visible in status and addon preflight
results.

#### Node gathering

For mining/herbalism, use the existing lock/profession requirements as the
primary capability check. Add a node skill-band index from gameobject lock data
and only plan resources whose required skill is within the character's skill
and configured progression window. Do not infer gathering eligibility from item
name or zone level alone.

For cross-zone node selection:

- resolve selected gameobject entries first;
- aggregate spawn density by map/zone;
- obtain area/zone level brackets from TravelMgr's prepared cache only as a
  scoring signal;
- require a stock travel-graph-reachable anchor;
- after arrival, reuse the current safe live-node cache and route follower;
- retain current-zone behavior when scope is not explicitly expanded.

#### Material farming

Add `CreatureTemplate` level/rank facts to source candidates. Score candidates
by:

```text
exact-item expected yield
* source density
* level safety margin
* profession feasibility
/ (stock travel graph cost + local hotspot travel cost + risk)
```

Level feasibility is a hard gate for non-exempt characters; travel cost is only
a ranker among feasible destinations. A nearby but unsafe level-10 source must
not beat a farther safe level-5 source for a level-5 character.

#### Optional constrained grinding

Do not expose a generic unrestricted `.sbrpg grind` in the first subphase.
First add a private `SbrpgGrindProfile` with explicit creature entries, level
band, rank policy, scope, duration, quantity/kill goal, and return policy. It
must select targets through a module-owned action, keep stock combat/loot
ownership, and never alter stock global grind behavior. Only expose a user
command after the safety matrix passes.

### Travel planning and execution sequence

1. Capture the full activity session start position before any travel.
2. Resolve material/node/grind sources and apply level/profession/rank filters.
3. Build spawn density candidates by zone/map, not one raw spawn at a time.
4. For each candidate, enumerate feasible walking, mount, flight-path,
   hearthstone, portal, area-trigger, ship, zeppelin, ferry, and public-
   transport legs from the actual player state.
5. Validate cooldowns, learned/trained abilities, bound hearth, portal
   availability, flight/mount area rules, transport schedule, money, combat,
   loot, and interaction range. Missing requirements eliminate a leg.
6. Compute complete route alternatives, including combinations such as
   `hearthstone → portal → flying mount` and `walk → ship → ground mount`.
   Select the fastest safe route with deterministic tie-breaking and a
   configurable risk penalty; graph distance alone must not invent a route.
7. Reject unreachable, prohibited-transfer, PvP-prohibited, instance-only, or
   requirement-failing candidates with a retained diagnostic reason.
8. Select the best feasible destination deterministically; report the selected
   methods, estimated time, and rejected alternatives in debug/plan output.
9. Snapshot the current stock `travel target`; reject if it is protected by
   group-copy/forced/master ownership.
10. Create a module-owned `TravelDestination` backed by stable
    `WorldPosition` storage. Configure visitor/cooldown limits and set it on
    the bot's existing `travel target` value. Lease `travel` strategy only if
    absent.
11. Let stock actions execute each leg. SBRPG observes actual hearth/portal
    completion, area-trigger use, boarding, flight/transport state, map changes,
    distance, retry count, combat, and loot—not relocate the bot.
12. At arrival radius, verify map, zone, phase, and live source/hotspot
    reachability. If verification fails, expire/cool down the travel target and
    either choose the next valid route/destination or return home.
13. Release/suspend the travel lease, clear only SBRPG's travel target, begin
    existing local farm state, and publish `arrived; farming started`.
14. On stop/timer/bag/quantity/failure, finish combat/loot and use the same
    fastest validated ordinary-player route back to the captured start. If no
    valid route exists, stop safely with `RETURN_ROUTE_UNAVAILABLE`; never
    synthesize a relocation.

### Permitted travel methods and transport policy

Phase 10 is limited to methods available to a regular player. This includes
legitimate shortcuts; “no teleport” means no GM/admin or synthetic relocation,
not “no hearthstone or portal.” A leg is permitted only after preflight proves
that the character can use it from the current state, and stock playerbot code
performs the interaction.

| Method | Phase 10 policy |
|---|---|
| Walking | Allowed with normal mmap/vmap and movement pacing. |
| Ground mounts | Allowed when trained/available and the area permits mounting. |
| Flying mounts | Allowed for sufficiently trained players when the area and map rules permit flying; otherwise fall back to ground travel. |
| Known flight paths | Allowed after validating learned nodes, route, money, flight-master access, and normal stock taxi execution. |
| Hearthstones | Allowed when bound, off cooldown, usable in the current state, and the resulting arrival is verified. |
| Player portals | Allowed when a real player/caster or stock-supported portal interaction is available and usable; do not create a portal or inject its effect. |
| Area triggers | Allowed for legitimate player-accessible entrances, portals, boats, and world transfers after validating the expected interaction and arrival. |
| Ships, zeppelins, ferries, and public transports | Allowed through verified stock boarding, waiting, riding, and disembark behavior. |
| GM/admin movement, synthetic teleport packets, forced relocation, or fabricated transport completion | Prohibited. |

Implementation requirements:

- Build a multimodal route planner that compares complete alternatives by
  estimated time plus configurable risk. It must be able to choose
  `hearthstone → portal → flying mount` when faster than walking or flight paths,
  and explain why it selected the route.
- Validate cooldowns, learned abilities, bound location, portal availability,
  transport schedule, flight/mount restrictions, money, combat/loot state, and
  interaction range before selecting a route.
- Use stock playerbot actions/handlers for hearthstone, portals, area triggers,
  taxis, transports, and movement. Never send relocation/transport packets
  directly from SBRPG.
- Flying-mount selection must observe actual area/map restrictions and mount
  capability; it must not set flight flags, bypass no-fly rules, or fly through
  invalid terrain.
- Add each ship/zeppelin/transport route through explicit data-backed
  adapters. Observe boarding, in-transit state, disembark, map/position arrival,
  timeout, and missed-transport recovery; never move the bot by coordinates.
- Reject routes that need an unavailable method with
  `UNSUPPORTED_PLAYER_TRAVEL_LEG`, naming the missing requirement. Do not
  silently substitute an unavailable shortcut.
- Return travel obeys the same policy. If no valid normal-player route exists,
  stop safely with `RETURN_ROUTE_UNAVAILABLE`.

### Addon and protocol UX

Add a preflight/plan panel before enabling an expanded-scope Start:

```text
Scope:             [Current zone | Current map | Connected travel]
Level policy:      Safe sources only (player 5; max source level 7)
Destination:       Westfall — 14 Linen hotspots
Travel:            1,240 yd stock ground route
Requirements:      Skinning 75 + knife                     [met]
Rejected sources:  Redridge (levels 15–18; over policy)
[Preview plan] [Start] [Cancel travel]
```

The current-zone option remains selected by default. The addon must display
`not implemented/server does not advertise travel capability` instead of
silently widening scope. Keep catalog/source traffic chunked and isolated.

New status phases:

```text
validating level policy
planning destination
awaiting stock travel
travelling via stock graph
arrived; validating hotspot
farming
returning via stock graph
return route unavailable
```

### Phase 10 implementation slices

1. **Source facts and level preflight**
   - add template level/rank and required skill facts to source profiles;
   - implement current-zone low-level rejection only; no travel yet;
   - add config, diagnostics, protocol preflight fields, and unit tests.
2. **Current-map destination planning**
   - aggregate source/node spawn density by zone;
   - choose a level-feasible, mmap-reachable destination on the current map;
   - use existing local movement only after the player is already near/in that
     zone; do not activate stock inter-zone travel yet.
3. **Stock travel-target lease**
   - introduce snapshot/lease ownership, module destination, travel-state
     observation, cancellation, and restoration tests;
   - enable explicit connected-map travel while preserving normal player travel
     requirements and stock action ownership.
4. **Return graph and failure recovery**
   - implement cross-map stop/finish return through the same stock graph;
   - handle map-load, logout, death, forced/group target, no-progress, and
     missing destination state without loops.
5. **Multimodal normal-player travel**
   - compare walking, mounts, known flight paths, hearthstones, portals, area
     triggers, ships, zeppelins, and public transports by safe estimated time;
   - validate flying mounts only where the player and area permit them;
   - add one verified shortcut/transport family at a time with real arrival
     confirmation and failure recovery; never synthesize relocation.
6. **Constrained grind profile**
   - add private profile-driven grind using the completed level policy;
   - expose only after no-unrelated-target and return tests pass.

### Phase 10 verification matrix

#### Level policy

1. Level 5 requests a material whose only normal source is level 10: reject
   before movement with source range and policy details.
2. Level 5 requests a material with level 4–6 and level 10 sources: choose only
   the feasible source/destination.
3. Level 70 requests Linen from gray level-5 sources: permit selected sources.
4. Disable the high-level exemption: verify gray source behavior follows the
   documented policy.
5. Elite, rare elite, boss, and broad-level templates: verify rank/actual-level
   revalidation rejects unsafe live targets.
6. Skinning with safe combat level but insufficient profession skill/tool:
   reject before travel.

#### Travel

1. Current-zone default produces no travel-target mutation.
2. Current-map eligible destination routes safely and starts only after arrival
   validation.
3. Connected travel chooses a graph-reachable target and preserves a forced or
   group-copied stock target by refusing to take ownership.
4. Missing graph path, unavailable hearth/portal, invalid area trigger,
   missed ship/zeppelin, failed taxi, combat, death, and map load produce
   bounded status/retry behavior and no synthetic relocation.
5. Stop during walking, mounted, flying, hearth/portal, taxi, and transport legs
   completes combat/loot and returns through allowed normal-player travel, or
   reports return-route failure without relocation.
6. Solo, group leader, group follower, party, and raid tests verify no travel
   target/strategy crosstalk.

#### Regression

1. Existing current-zone material/node runs remain unchanged with Phase 10
   disabled.
2. Stock playerbot quest/RPG travel continues after SBRPG releases its lease.
3. Existing return-home and corpse-skinning behavior remains intact.

**Exit:** a low-level selfbot cannot start an unsafe material/node/grind plan;
a high-level selfbot may explicitly farm selected gray sources; and an opt-in
travel plan reaches only graph-validated, policy-approved source zones using
stock travel execution and safe return behavior.

## Phase 11 — Automated questing

### Purpose

Phase 11 adds a deliberate, module-owned quest campaign controller. It must not
turn on stock `rpg`, `quest`, or `accept all quests` globally and hope for the
best. The controller plans a small, explainable set of level-appropriate quests,
uses Phase 10 travel policy for destinations, and delegates actual quest packet
handling/combat/loot to existing playerbot actions.

Questing is a separate goal type. It may share travel, source selection,
level-policy, return, and telemetry infrastructure with farming, but it must
not overload `MaterialDefinition` or treat a quest objective as a generic grind
request.

### Existing mod-playerbots integration points

| Need | Existing playerbot facility | Intended Phase 11 use |
|---|---|---|
| Quest destinations | `TravelMgr::getQuestTravelDestinations` and `QuestRelationTravelDestination` / `QuestObjectiveTravelDestination` | Discover acceptor, objective, and turn-in destinations with stock active/full/cooldown semantics. |
| Objective status | `TravelMgr::getObjectiveStatus`, `Player::GetQuestStatus`, `QuestStatusData` | Read exact creature/GO/item progress; never infer completion from a kill alone. |
| NPC dialog checks | `dialog status`, `can accept quest npc`, `can accept quest low level npc`, `can turn in quest npc` values | Preflight and interaction gating; low-level quests must be policy-filtered, not accepted accidentally. |
| Accept/turn-in packets | `QuestAction::ProcessQuests`, `QuestAction::AcceptQuest`; `NewRpgBaseAction::AcceptQuest`/`TurnInQuest` | Delegate valid in-range interactions; do not handcraft duplicate accept/reward packets. |
| Quest updates | `QuestUpdateAddKillAction`, `QuestUpdateAddItemAction`, `QuestUpdateCompleteAction`, `QuestItemPushResultAction` | Observe confirmed objective progress and trigger replanning. |
| Existing strategy behavior | `DefaultQuestStrategy`, `AcceptAllQuestsStrategy`, `NewRpgDoQuestAction` | Reference only; lease narrowly or call safe existing actions. Do not enable `accept all quests` for autonomous campaigns. |
| POIs | `NewRpgDoQuestAction::GetQuestPOIPosAndObjectiveIdx` / `DoIncompleteQuest` | Reuse validated same-map POI logic selectively; its random/idle behavior is not sufficient as the SBRPG campaign controller. |

Relevant source locations:

```text
modules/mod-playerbots/src/Mgr/Travel/TravelMgr.h/.cpp
modules/mod-playerbots/src/Ai/Base/Actions/QuestAction.*
modules/mod-playerbots/src/Ai/Base/Actions/AcceptQuestAction.*
modules/mod-playerbots/src/Ai/Base/Actions/QuestConfirmAcceptAction.*
modules/mod-playerbots/src/Ai/Base/Value/QuestValues.*
modules/mod-playerbots/src/Ai/Base/Strategy/QuestStrategies.*
modules/mod-playerbots/src/Ai/World/Rpg/Action/NewRpgAction.*
modules/mod-playerbots/src/Ai/World/Rpg/Action/NewRpgBaseAction.*
```

### Quest campaign model

```cpp
enum class QuestPolicy : uint8_t
{
    PreviewOnly,
    AcceptAndComplete,
    AcceptCompleteAndTurnIn
};

enum class QuestStage : uint8_t
{
    Planning,
    TravelToGiver,
    Accepting,
    TravelToObjective,
    CompletingObjective,
    TravelToTurnIn,
    TurningIn,
    WaitingForChoice,
    Blocked,
    Returning,
    Finished
};

struct QuestCandidate
{
    uint32_t questId;
    uint8_t minLevel;
    uint8_t questLevel;
    uint8_t maxLevel;
    bool repeatable;
    bool timed;
    bool dungeon;
    bool raid;
    bool pvp;
    bool autoComplete;
    std::vector<QuestObjectiveTravelDestination*> objectives;
    std::vector<QuestRelationTravelDestination*> givers;
    std::vector<QuestRelationTravelDestination*> takers;
    float travelCost;
    float rewardScore;
    std::string rejectionReason;
};

struct QuestCampaignState
{
    ActivitySession session;
    QuestPolicy policy;
    QuestStage stage;
    uint32_t activeQuestId;
    int32_t activeObjectiveIndex;
    std::vector<uint32_t> acceptedQuestIds;
    std::vector<uint32_t> blockedQuestIds;
    std::vector<uint32_t> deferredQuestIds;
    SbrpgTravelState travel;
    StrategyLease questStrategy;
    StrategyLease lootStrategy;
    StrategyLease gatherStrategy;
    uint32_t stageSinceMs;
    uint32_t noProgressSinceMs;
    std::string reason;
};
```

Persist only stable identifiers and policy/options. Re-resolve all quest,
destination, creature, gameobject, and world-object pointers each tick. Never
retain `Quest*`, `WorldObject*`, or `TravelDestination*` beyond the lifetime
owned by stock TravelMgr.

### Quest eligibility policy

Start conservative and require all of the following:

- quest is available to the player according to stock dialog/quest APIs;
- `CanTakeQuest`, `CanAddQuest`, quest-log capacity, bag capacity, race/class,
  reputation, prerequisite, and expansion checks pass;
- quest level fits the Phase 10 player-level policy; high-level characters may
  intentionally select gray quests when the gray-quest exemption is enabled;
- a quest chain may cross zones or maps when every linked step is independently
  eligible and the multimodal Phase 10 route planner can reach its giver,
  objectives, and turn-in through valid player travel;
- active objective/turn-in destinations exist in TravelMgr and are reachable
  through allowed Phase 10 scope/travel rules;
- no timed, dungeon, raid, battleground/PvP, escort, vehicle, scripted-choice,
  profession-specialization, or item-destruction quest in the first release;
- group-required quests are eligible only when group assistance is explicitly
  enabled and safe eligible group members can be invited/accepted; otherwise
  they remain blocked rather than being solo-attempted;
- no quest that requires an unimplemented interaction type (spell click,
  vehicle, cinematic, phasing handoff, multi-step gossip choice, item target,
  or instance reset) is auto-started;
- objective creature levels/ranks pass the same revalidated source policy used
  by Phase 10;
- rewards do not require an unresolved choice. The first release pauses at a
  reward choice and asks the user through addon/chat; it never selects an item
  automatically.

Add explicit allow/deny lists by quest ID for custom databases. A deny decision
must include a reason in preview/status output.

### Campaign lifecycle

1. **Preview first.** `.sbrpg quest plan [scope]` returns scored candidates,
   prerequisites, complete multimodal route legs, estimated time, objective
   types, chain dependencies, and rejection reasons. The addon must display this
   before an autonomous start.
2. **Select bounded work.** Start with one quest or a small configurable batch;
   do not fill the quest log. Preserve player-owned quests and do not drop any
   quest automatically.
3. **Resolve chain and group needs.** For a selected cross-zone chain, plan each
   linked quest in order and revalidate it after every turn-in. For a group quest,
   identify eligible nearby group members, request/invite only when group-help
   is explicitly enabled, and wait for actual group membership before starting
   group objectives. Never force invitations, disrupt an existing group, or
   fabricate group completion.
4. **Travel.** Use Phase 10's `SbrpgTravelState` and stock travel lease to
   reach a quest giver/objective/taker using the fastest validated route.
4. **Interact.** At the resolved quest giver, call/lease the stock quest
   interaction path (`QuestAction::ProcessQuests` or the equivalent safe
   in-range action). Confirm actual `GetQuestStatus` transition before moving
   on.
5. **Complete objectives.** For kill/loot objectives, create a bounded
   objective-specific target set from `QuestObjectiveTravelDestination` data;
   use module-owned selected targeting and stock combat/loot. For gameobjects,
   let stock `gather`/`use game object` handling own the interaction after
   eligibility checks. Observe quest update packets/status, not action success.
6. **Replan on progress.** On `QuestUpdateAddKill`, `QuestUpdateAddItem`, or
   objective status change, recompute the next incomplete objective. Do not
   continue killing completed objectives.
7. **Turn in.** After complete status, travel to a valid quest taker and use
   stock turn-in handling. If reward choice exists, set `WaitingForChoice`,
   publish choices, and wait for explicit user selection.
8. **Bound failures.** If an objective has no verified progress after a
   configured dwell/attempt budget, mark that quest blocked for this campaign,
   preserve it in the quest log, report exact cause, and choose another
   candidate or return. Do not repeatedly abandon/reaccept it.
10. **Stop/return.** An explicit stop, duration, bags, unsafe combat state, or
   user cancellation stops new accepts/objectives, finishes owned combat/loot,
   and returns through Phase 10 policy. It never auto-drops quests or forcibly
   removes group members.

### Objective support rollout

| Objective type | First-release policy |
|---|---|
| Kill named creature | Supported after target/source and level safety validation. |
| Kill creature family | Supported using exact objective entry IDs from travel destinations. |
| Loot item from creature | Supported only when source/objective linkage and normal loot attribution are available. |
| Gather/activate gameobject | Supported only via stock gather/use handling and current-map safe path validation. |
| Explore area | Preview-only initially; enable after reliable area-completion observation. |
| Talk/interact/turn-in | Supported through stock quest giver actions. |
| Escort, vehicle, spell click, scripted gossip | Blocked in Phase 11 initial release. |
| Timed, dungeon, raid, PvP quests | Blocked by default. |
| Group quests | Allowed only with explicit group-assist mode, eligible members, and safe invitation/coordination; otherwise blocked. |
| Gray quests for high-level characters | Allowed when the gray-quest exemption and normal quest prerequisites are satisfied. |
| Item choice, reputation choice, branching quest | Pause for explicit user decision. |

### Strategy and coexistence rules

- Introduce a module `sbrpg quest` strategy/action/context rather than adding
  broad global triggers to `mod-playerbots`.
- Lease `loot`, `gather`, and only necessary quest strategy behavior through
  `StrategyLease`; restore exact prior ownership at campaign end.
- Do not enable `accept all quests`: it accepts every nearby eligible quest and
  violates campaign planning/quest-log bounds.
- Do not mutate stock `travel target` while stock quest/RPG/follow/group target
  owns it; queue/pause with a visible reason.
- Stock combat, corpse loot, recovery, party safety, and user commands preempt
  quest movement.
- Use existing playerbot quest packet handlers/actions. No direct inventory,
  quest-status, reward, or objective counter mutation.
- Keep Phase 11 disabled unless Phase 10 level-aware travel is enabled and its
  safety matrix passes.

### Addon/protocol UX

Add a distinct Quest tab/panel; do not overload material controls:

```text
Quest mode:       [Preview | Autonomous]
Scope:            [Current zone | Current map | Connected travel]
Level policy:     Safe quests only
Campaign size:    [1 | 3 | 5]
Current quest:    The Westfall Stew
Stage:            Travel to objective 1/2
Progress:         Gnolls 4/8
Blocked:          1 (escort objective not supported)
[Preview plan] [Start questing] [Pause] [Stop and return]
```

Protocol additions must be versioned/capability-gated and chunked:

```text
QUEST_CAPABILITIES
QUEST_PLAN
QUEST_PLAN_ROW
QUEST_STATUS
START_QUEST_CAMPAIGN
QUEST_REWARD_CHOICES
SELECT_QUEST_REWARD
PAUSE_QUEST_CAMPAIGN
STOP_QUEST_CAMPAIGN
```

Every status must name the quest ID/title, stage, objective index/progress,
destination, travel state, retry/no-progress time, and reason. Party/raid
request isolation follows Phase 6: replies are whispered to the requesting
self-bot.

### Phase 11 implementation slices

1. **Read-only planner**
   - discover/score candidates from TravelMgr and player quest state;
   - apply deny rules and Phase 10 level/travel policy;
   - expose chat/addon preview only; no accept/travel/action side effects.
2. **One safe quest lifecycle**
   - accept one non-timed, non-group kill/loot quest;
   - travel, complete, and turn in using stock actions;
   - observe actual status/packet transitions and safe return.
3. **Objective controller**
   - add exact kill, item-loot, and approved gameobject objectives;
   - support replanning, bounded no-progress, and blocked/deferred reporting.
4. **Campaign queue and reward decisions**
   - bounded quest batch, prerequisites, turn-in ordering, quest-log protection,
   - explicit addon reward-choice pause/resume.
5. **Expanded objective types**
   - evaluate explore and approved interaction quests one type at a time;
   - retain deny-by-default for escort/vehicle/instance/PvP content.
6. **Custom database hardening**
   - quest travel-table gaps, scripted quests, conditions, phase/area changes,
   - per-quest overrides with auditable reasons.

### Phase 11 verification matrix

#### Planning and acceptance

1. Level-appropriate available quest appears in preview with giver/objective/
   turn-in route and cost.
2. High-level gray quests appear only when the gray-quest exemption is enabled,
   with their lower-level status clearly shown.
3. Cross-zone quest chains produce ordered route plans and revalidate each next
   step after turn-in.
4. Low-level, over-level, timed, dungeon, PvP, escort, vehicle, and denied
   quests are excluded with exact reasons; group quests explain whether group
   assist is unavailable or awaiting members.
3. Full quest log, full bags, missing prerequisite, and unavailable quest giver
   reject before travel/interaction.
5. Existing player-owned quests remain untouched; no automatic quest drop.

#### Objective execution

1. Kill objective advances only after real quest kill update/status change.
2. Loot objective advances only after real item/objective update, including
   grouped/reference loot and full inventory handling.
3. Completed objective stops proactive pulls immediately and selects next
   objective/turn-in.
4. No-progress timer marks only the campaign quest blocked; it does not loop,
   abandon, or select unrelated mobs.
5. Gameobject and exploration objectives remain disabled until their dedicated
   test suites pass.

#### Travel, interaction, and rewards

1. Giver/objective/taker travel uses Phase 10 stock travel lease, compares
   walking/mount/flight/hearth/portal/transport alternatives, and restores prior
   playerbot travel ownership.
2. NPC and gameobject quest givers use stock interaction handling only when in
   valid range.
3. A group quest requests only explicitly allowed members, confirms actual
   membership, and never begins the objective until group-assist requirements
   are met.
3. Completion reaches a valid taker; a reward choice pauses and requires an
   explicit user selection.
4. Stop/pause, combat, death, map transition, disconnect/relog, and bag limit
   preserve safe state and never duplicate acceptance or reward.

#### Coexistence

1. Current node/material modes cannot run concurrently with a quest campaign
   unless a future composite goal explicitly coordinates ownership.
2. Solo, group leader, group follower, party, and raid tests show no quest or
   addon protocol crosstalk.
3. Existing stock `quest`, `rpg`, `travel`, and `accept all quests` behavior is
   unchanged when `sbrpg quest` is absent.

**Exit:** a user can preview, approve, and safely run a bounded set of
level-appropriate, non-scripted open-world quests; travel, combat, loot,
acceptance, turn-in, and reward decisions remain observable, policy-gated, and
owned by the established playerbot systems rather than unsafe module shortcuts.

## Automated verification

### Pure/unit tests

- material alias resolves to exact item ID;
- ambiguous/unknown item names fail clearly;
- direct loot source lookup;
- grouped loot source lookup;
- nested reference lookup;
- reference cycle/depth rejection;
- quest-required source exclusion;
- skinning source and required-skill classification;
- cluster stability independent of database row order;
- hotspot medoid is a member/reachable coordinate;
- score ordering and bounded observed-yield adjustment;
- gray source accepted while unrelated gray creature rejected;
- elite, friendly, tapped, player, pet, and prohibited target rejection;
- persistent objective through combat;
- normal loot before harvest;
- quantity counts item units, not corpse count;
- timer/full-bag/quantity return request precedence;
- strategy lease exact restoration;
- malformed/oversized/chunk-missing protocol rejection.

### Build/static checks

- reconfigure CMake after adding translation units;
- compile all affected C++ units from `compile_commands.json`;
- full module/worldserver link;
- `luac -p addon/SelfBotRPG/*.lua`;
- verify the gear-button settings overlay exposes every `.conf.dist` option;
- verify Apply sends validated `SET_CONFIG` values to the current session;
- verify Reset Defaults and `SelfBotRPGDB.Settings` persistence across reload;
- verify optional lure enable/disable behavior without blocking fishing;
- verify fishing moves to reachable water before casting and restores equipment;
- verify targeted-fish and current-zone fishing controls reject invalid selections;
- verify open-water default and pool-priority override behavior;
- git diff --check;
- verify no new `mod-playerbots` or core source dependency/edit is required;
- verify no `StaticPopupDialogs` or unrelated addon globals are modified.

## In-game validation matrix

### Cloth

1. High-level selfbot in a known Linen area with only gray mobs.
2. Confirm only database-proven Linen sources are proactively attacked.
3. Run at least 30 minutes across multiple hotspots.
4. Compare requested-item telemetry against inventory gain.
5. Verify low-chance zero-yield kills do not permanently blacklist a source.

### Leather

1. No Skinning profession: START rejected.
2. Insufficient skill: incompatible sources excluded/reported.
3. Missing knife: START rejected with exact requirement.
4. Valid beast: normal loot fully completes, then skinning completes once.
5. Multi-yield skin: all item stacks are looted and metrics count quantities.
6. Other player skins/despawns corpse: objective releases without loop.

### Navigation/combat

1. Open terrain hotspot loop.
2. Cave/mineshaft source cluster reached through the actual entrance.
3. Cliff-separated geometric-near hotspot rejected when mmap path fails.
4. Incidental combat retains material target and route.
5. Death/recovery does not select stale pointers.
6. Beyond-sight hotspot travel continues safely.

### Session lifecycle

1. Empty duration/quantity runs indefinitely.
2. Timer expiry stops new pulls and returns home after current combat/loot.
3. Quantity goal returns after exact units acquired.
4. Bag reserve stops pulls before all slots are consumed.
5. Explicit stop during combat finishes combat and corpse processing safely,
   then returns to the captured session start.
6. Map change fails/stops safely.

### Coexistence

1. Solo, party, and raid addon operation without crosstalk.
2. Existing mining/herbalism behavior unchanged.
3. Pre-existing loot/gather/grind/follow strategies restored exactly.
4. No Away-loop or module-owned AFK manipulation.
5. Minimap/material settings persist across `/reload` and relog.

## Diagnostics required before release

Every wait or refusal must have a bounded reason visible in status/debug output:

- no database source;
- no source in current scope;
- all sources too high-level/elite;
- profession or tool missing;
- no safe hotspot path;
- hotspot temporarily empty;
- target contested/tapped;
- target unreachable/blacklisted;
- combat active;
- corpse loot pending;
- harvesting pending;
- recovering health/mana;
- bag reserve reached;
- timer or quantity complete;
- returning home.

A generic `waiting` or `gather pending` lasting indefinitely is not acceptable.

## Definition of done

Material farming is complete when a high-level selfbot can be placed in a known
low-level farming zone, select Linen Cloth, and reliably route among valid gray
mob clusters, fight with stock class AI, fully loot corpses, retain objectives
through interruptions, stop on duration/quantity/bags, and return home—without
attacking unrelated mobs or taking ownership of the loot lifecycle.

Leather support is complete only when the same controller correctly sequences
normal corpse loot and stock skinning for all eligible corpses without stale
loops or lost multi-yields. Additional profession materials may be exposed only
after their acquisition method, source validation, safety requirements, and
runtime verification are explicit.
