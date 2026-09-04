# mod-selfbot-rpg

`mod-selfbot-rpg` adds safe, gathering-focused farming behavior for
`mod-playerbots` self-bots on AzerothCore WotLK.

The module has two isolated modes:

- **Node farming:** mine or gather configured gameobject resource families.
- **Material farming:** select an exact item, find database-proven creature
  sources, fight eligible creatures (including gray creatures), and collect
  the requested material.

The module does not enable a general gray-mob grind mode. It selects only
creatures that are sources for the requested item.

## Installation and build

1. Place this module at `modules/mod-selfbot-rpg`.
2. Copy `addon/SelfBotRPG` to the client
   `Interface/AddOns/SelfBotRPG` directory.
3. Enable self-bot mode:

   ```text
   .playerbots bot self
   ```

4. Build with the repository's required command:

   ```bash
   /stuff/Source/azerothcore-wotlk/acore.sh compiler build
   ```

The module configuration is installed as `etc/modules/mod-selfbot-rpg.conf`
by the build. Restart/reload the worldserver after changing server settings.

## Commands

### Node farming

```text
.sbrpg farm mining copper
.sbrpg farm mining fel iron
.sbrpg farm herbalism peacebloom
.sbrpg farm zone mining
.sbrpg farm zone herbalism
.sbrpg farm both zone
```

Node names are resolved against the world database's gameobject templates.
Zone mode restricts the route to the zone where the run started. Same-map area
changes are tolerated; a true map change stops the node activity safely.

### Material farming

```text
.sbrpg material sources linen
.sbrpg material sources 2589
.sbrpg material hotspots linen
.sbrpg material start linen
.sbrpg material start light leather 30 80
.sbrpg material start 2318
.sbrpg material status
.sbrpg mstatus
.sbrpg stop
.sbrpg status
```

`duration` is in minutes. `0` or an omitted value means unlimited. `quantity`
is an exact item-unit goal; `0` or an omitted value means unlimited.

`material sources` reports database sources, acquisition method, estimated
chance/count, references, and exclusions. `material hotspots` reports scoped
spawn clusters and mmap reachability. These diagnostics can show low-chance
sources that are intentionally excluded from proactive targeting.

`.sbrpg stop` is a **stop-and-return** command. It cancels new objectives,
allows current combat and corpse processing to finish, then returns to the
captured starting position and orientation. Farming state is released only
when the return completes. If there is no active run, the command has no
movement effect.

## Supported material catalog

The catalog uses exact item IDs internally. Names and aliases are only input
helpers and UI labels.

### Cloth

| Item | ID | Acquisition |
|---|---:|---|
| Linen Cloth | 2589 | Creature loot |
| Wool Cloth | 2592 | Creature loot |
| Silk Cloth | 4306 | Creature loot |
| Mageweave Cloth | 4338 | Creature loot |
| Runecloth | 14047 | Creature loot |
| Felcloth | 14256 | Creature loot |
| Netherweave Cloth | 21877 | Creature loot |
| Frostweave Cloth | 33470 | Creature loot |

### Leather and hides

The catalog includes Light, Medium, Heavy, Thick, and Rugged Leather/Hides,
Knothide Leather Scraps, Knothide Leather, Borean Leather Scraps, and Borean
Leather. These entries use creature loot and/or Skinning according to the
catalog definition and loaded database templates.

Before a Skinning run starts, the module validates the Skinning skill, skill
level, compatible skinning tool, eligible sources, and reachable hotspots.
Normal corpse loot is completed before stock playerbot skinning is allowed.

### Cooking and elemental materials

The catalog currently includes common creature cooking materials such as Chunk
of Boar Meat, Mutton Chop, Stringy Wolf Meat, Coyote Meat, Lynx Meat, Bat Flesh,
and Deeprock Salt, plus Motes of Air, Earth, Fire, Life, Mana, Shadow, and
Water.

Primal Fire, Water, Air, Earth, Life, Shadow, and Mana are catalogued as
**Transformation** materials. Transformation farming is not implemented as a
creature-farming pipeline; these entries are rejected before movement when no
supported direct source exists.

The catalog is intentionally conservative. An item used by a profession is
not assumed to be directly farmable merely because it appears in a recipe.

## How material farming works

The ownership boundary is:

> **SBRPG chooses the item, source, hotspot, and creature. Stock playerbots
> owns combat setup, corpse loot, and harvesting.**

The material controller:

1. Resolves the exact catalog item.
2. Builds or loads a reverse source index from creature loot and skinning data.
3. Loads same-map scoped creature spawns.
4. Plans bounded density hotspots and mmap-validated route segments.
5. Selects only live, hostile, eligible source creatures.
6. Uses a module-owned attack action to permit selected gray creatures.
7. Hands combat and loot lifecycle back to stock playerbot actions.
8. Attributes exact item quantities from loot events.
9. Returns home on stop, timer, quantity, bag reserve, or completion.

The module does not globally change `GrindTargetValue`, gray-target rules, loot
filters, sight distance, or playerbot movement behavior.

## Loot, skinning, and corpse behavior

For every selected creature, the intended sequence is:

```text
select -> approach -> attack -> combat -> normal corpse loot
-> corpse becomes skinnable -> stock Skinning -> corpse release
```

Important behavior:

- Gray creatures are accepted only when they match the selected source set.
- Stock loot is prioritized over material travel.
- All corpse items, including gray items, are eligible during a material run.
- SBRPG does not repeatedly open, close, replace, or clear stock loot targets.
- Empty or stale corpses receive bounded recovery time and then are released.
- A return request does not abandon current combat or post-combat corpse loot.
- Skinning is not considered eligible until normal loot is complete.
- Current corpse-skill support is Skinning-focused; Herbalism, Mining, and
  Engineering corpse harvesting remain follow-up work.

## Navigation and safety

- Routes are map-local and use mmap validation.
- `PATHFIND_NOPATH` and repeated no-progress objectives are temporarily
  blacklisted.
- Useful incomplete mmap corridors may be followed segment by segment.
- Movement is bounded and uses normal `MovementAction` pacing.
- No teleport, clipping, click-to-move route injection, or unsafe direct spline
  movement is used.
- No players, pets, totems, friendly creatures, bosses, or default-prohibited
  elites are proactively targeted.
- Tapped/claimed, unreachable, evading, or otherwise unsafe creatures are
  skipped.
- Recovery, health, mana, and combat state take priority over new pulls.
- AFK flags and unrelated global playerbot behavior are not manipulated.

## Session lifecycle

Every node and material run records:

- starting map;
- starting X/Y/Z position;
- starting orientation;
- start time and optional deadline;
- return reason and return progress.

A run can return because of:

- explicit `.sbrpg stop`;
- duration expiry;
- quantity goal reached;
- configured bag reserve reached;
- unrecoverable activity failure.

During return, new targets are suspended. Existing combat and loot are allowed
to resolve. The same safe route logic is used to reach the recorded start.
Strategies leased by SBRPG are restored only after the activity ends.

## Configuration

Defaults are in `conf/mod-selfbot-rpg.conf.dist`:

```ini
SelfBotRpg.Enable = 1
SelfBotRpg.Debug = 0
SelfBotRpg.MaterialMinimumChance = 1.0
SelfBotRpg.MaterialReservedBagPercent = 0
SelfBotRpg.AttemptsBeforeBlacklist = 3
SelfBotRpg.FailedNodeBlacklistSeconds = 120
SelfBotRpg.EmptyNodeBlacklistSeconds = 120
SelfBotRpg.StayInCurrentZone = 1
SelfBotRpg.GatherSettleDelayMs = 1000
SelfBotRpg.ActionDelayMs = 1000
```

`MaterialMinimumChance` is a percentage used only for proactive creature
selection. Diagnostic source commands still expose lower-chance sources.
`MaterialReservedBagPercent` stops material farming before the configured
percentage of bag capacity is used. `ActionDelayMs` limits SBRPG-owned
controller decisions; it does not change stock playerbot action timing.

Active node-run options can be changed with:

```text
.sbrpg set attempts 3
.sbrpg set failedblacklist 120
.sbrpg set emptyblacklist 120
.sbrpg set zone 1
.sbrpg set settledelay 1000
```

## Addon UI

`/sbrpg` toggles the SelfBot RPG panel. The panel provides:

- Node profession picker.
- Scrollable node resource picker with matching ore/herb icons.
- Searchable, scrollable material picker.
- Material item icons and metadata tooltips.
- Duration and quantity goals.
- Start Farming, Start Material, and Stop controls.
- Status and diagnostic feedback.

The panel uses bounded popups so long catalogs do not extend off-screen. It
fades when idle and returns to full opacity while the cursor is over the main
frame or an open picker/edit control.

The minimap launcher is stored in `SelfBotRPGDB`:

- Left-click: toggle farming panel.
- Right-click: open settings.
- Drag: reposition the launcher.

## Addon protocol

The addon uses the isolated, versioned `JLYRPG2` protocol described in
[`docs/addon-protocol.md`](docs/addon-protocol.md).

Supported operations include:

- `HELLO` / `CAPABILITIES` negotiation;
- node `START`, `STATUS`, `SET`, and `STOP`;
- `MATERIAL_CATALOG`;
- `MATERIAL_SOURCES`;
- `START_MATERIAL`;
- `MATERIAL_STATUS`.

Catalog and source responses are sent as bounded indexed frames. Requests may
use party/raid transport, but server replies are whispered to the requesting
self-bot to prevent catalog/status cross-talk. Existing node farming protocol
frames and the manual `/sbrpgchat` fallback remain isolated.

## Diagnostics and metrics

Set `SelfBotRpg.Debug = 1` to emit route, target, corpse, handoff, timeout,
blacklist, return, and strategy messages to self-bot system chat and the
worldserver debug log.

Status and summaries expose, where applicable:

- current phase and reason;
- selected material/source/target;
- route or hotspot progress;
- kills and loot events;
- exact requested item units;
- item and kill rates;
- skinning state;
- corpse timeouts;
- return reason and remaining time.

A persistent wait should have a specific reason such as `combat`, `corpse loot
pending`, `harvesting`, `recovering`, `no safe hotspot`, or `returning`.

## Validation status and remaining work

Implemented and statically/build validated:

- shared activity sessions and start-position capture;
- mmap-safe node and material route following;
- cloth, leather/hide, cooking, mote, and primal catalog metadata;
- reverse loot-source indexing with references and grouped rows;
- spawn loading and hotspot planning;
- gray-source combat selection;
- stock loot ownership and normal-loot-before-skinning ordering;
- quantity, duration, bag-reserve, diagnostics, and return-home lifecycle;
- addon capability negotiation, catalog chunks, material status, icons,
  tooltips, and scrollable pickers;
- stop-and-return behavior;
- required full worldserver build.

Still requiring broader runtime/custom-database validation:

- long material farming soak tests;
- solo/party/raid addon crosstalk testing in the live client;
- all Skinning multi-yield and external-skinning cases;
- Herbalism/Mining/Engineering corpse harvest support;
- custom loot-condition and custom-database coverage;
- adaptive hotspot scoring and respawn learning;
- transformation, fishing, reputation, vendor, bag-disposal, and autonomous
  questing pipelines.

## Related files

```text
src/SBRPG.cpp                         controller, commands, protocol dispatch
src/Activity/ActivityState.h          activity state and telemetry
src/Session/ActivitySession.*         start/stop/return lifecycle
src/Farm/RouteFollower.*              bounded mmap route following
src/Materials/MaterialCatalog.*       exact item catalog and metadata
src/Materials/LootSourceIndex.*       reverse loot-source discovery
src/Materials/CreatureSpawnRepository.*
src/Materials/HotspotPlanner.*
src/Materials/MaterialFarmState.h
src/Protocol/SbrpgProtocol.*
addon/SelfBotRPG/SelfBotRPG.lua       client panel and protocol client
docs/addon-protocol.md                JLYRPG2 wire format
plan1.md                              implementation plan and validation matrix
```
