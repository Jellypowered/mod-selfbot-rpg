# mod-selfbot-rpg

`mod-selfbot-rpg` adds gathering-focused farming tools for AzerothCore WotLK
servers running `mod-playerbots`. It is designed to extend the existing
selfbot rather than replace or globally alter playerbot behavior.

The module currently provides three modes:

- **Node farming:** mine or gather configured gameobject resource families.
- **Material farming:** select an exact catalog item, find database-proven
  creature sources, fight eligible creatures, and collect the requested item.
- **Fishing:** fish for a selected fish or the current zone's cataloged fish.

The module is not a general gray-mob grinding system. Gray creatures are
selected only when they are proven sources for the requested material.

## Important notes and limitations

Please read these before starting a long run:

1. **AFK protection:** long runs may still be affected by the client-side AFK
   restriction, which normally expects player input after roughly five minutes.
   The playerbot selfbot AFK changes may reduce this problem, depending on the
   version of `mod-playerbots` in use. See the relevant upstream change:
   <https://github.com/mod-playerbots/mod-playerbots/commit/6704d5532a299972aa0d7b8082dab6b74b4fb1ff>.
2. **Current-zone operation:** farming currently starts and operates on the
   player's current map and zone. The module does not yet choose a zone or
   travel between zones.
3. **Normal-player movement:** routes use bounded, mmap-validated playerbot
   movement. The module does not teleport, clip, inject unsafe splines, or use
   GM relocation.
4. **Account Security** You will need a GM level account to utilize this, unless you change the following in playerbots.conf
```
# Player can be activated as a bot (selfbot)
# Selfbot permission level (0 = disabled, 1 = GM only (default), 2 = all players, 3 = activate on login)
AiPlayerbot.SelfBotLevel = 1
```
`Set this to 2 or change your account security level.`

5. **Live testing:** fishing, custom database rows, profession requirements,
   bag handling, and long-duration runs still benefit from careful live testing.

If something behaves incorrectly, please report it through the repository's
**Issues** section. Include the module version or commit, AzerothCore and
`mod-playerbots` versions, relevant configuration, exact commands or addon
steps, and useful worldserver logs. Screenshots and a short reproduction path
are especially helpful.

## Installation and build

You should be comfortable installing AzerothCore modules and building the
server before using this project.

1. Place this module in `modules/mod-selfbot-rpg`.
2. Copy `addon/SelfBotRPG` into the client directory:
   `Interface/AddOns/SelfBotRPG`.
3. Build the server from the AzerothCore repository:

   ```bash
   ./acore compiler build
   ```

   Use the equivalent build command if your checkout provides a wrapper.

The build installs the module configuration as
`env/dist/etc/modules/mod-selfbot-rpg.conf`. Restart or reload the worldserver after
changing server configuration. Settings changed in the addon are applied live
and take priority for the current session.

Start controls automatically enable selfbot mode when necessary. Stop and
completion paths disable it again only when SBRPG enabled it; an already
manually enabled selfbot is left alone.

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

Resource names are resolved against the world database's gameobject templates.
Zone mode restricts node selection to the zone where the run began. Same-map
area changes are allowed, but a true map change stops the activity safely.
Node farming does not arrange travel to another zone.

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

`duration` is measured in minutes. `0`, or an omitted value, means unlimited.
`quantity` is an exact item-unit goal; `0`, or an omitted value, means
unlimited.

`material sources` reports database sources, acquisition methods, estimated
chance or count, references, and exclusions. `material hotspots` reports
scoped spawn clusters and mmap reachability. Diagnostics may show low-chance
sources that are intentionally excluded from proactive targeting.

Material farming is limited to the player's current map and zone at start. If
no eligible, reachable source exists there, the start request is rejected. The
module does not select a farming zone, use flight paths, travel through
portals, visit vendors, or teleport.

`.sbrpg stop` is a stop-and-return command. It cancels new objectives, allows
current combat, fishing casts and bobbers, and loot processing to finish, then
returns to the captured starting position and orientation. The return route is
also bounded and mmap-validated. Farming state and leased strategies are
released only after the return completes.

## Supported material catalog

The catalog uses exact item IDs internally. Names and aliases are input helpers
and UI labels; they do not replace the item-ID checks.

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
Knothide Leather and Scraps, and Borean Leather and Scraps. Each entry uses
creature loot and/or Skinning according to the catalog definition and loaded
database templates.

Before a Skinning run starts, the module validates the Skinning skill, skill
level, compatible tool, eligible sources, and reachable hotspots. Normal corpse
loot is completed before stock playerbot skinning is allowed.

### Cooking and elemental materials

The catalog includes common creature cooking materials such as Chunk of Boar
Meat, Mutton Chop, Stringy Wolf Meat, Coyote Meat, Lynx Meat, Bat Flesh, and
Deeprock Salt. It also includes Motes of Air, Earth, Fire, Life, Mana, Shadow,
and Water.

Primal Fire, Water, Air, Earth, Life, Shadow, and Mana are cataloged as
**Transformation** materials. Transformation farming is not implemented as a
creature-farming pipeline, so these entries are rejected before movement when
no supported direct source exists.

The catalog is deliberately conservative. An item being used by a profession
does not automatically make it directly farmable.

## Fishing

Fishing can be started from the addon or protocol using either a selected fish
or the current zone. Targeted fishing counts only the selected catalog item.
Zone fishing counts cataloged fishing materials found in the player's current
zone and excludes trash.

Open-water fishing is the default. Matching fishing pools can optionally be
prioritized, with open-water fallback when no active pool is available. Search
and cast distances are bounded configuration values:

- `FishingSearchDistance` controls how far the module searches for reachable
  water.
- `FishingCastDistance` controls the maximum inland distance used when choosing
  a shoreline cast point.

The module uses normal fishing-pole equipment actions, applies available lures
on a best-effort basis, and never makes a lure a requirement. Previous
main-hand and off-hand equipment is restored when fishing ends. If a run is
stopped, the current cast, bobber, reel, and loot sequence is allowed to finish
before the return route begins.

## How material farming works

The ownership boundary is:

> **SBRPG chooses the item, source, hotspot, and creature. Stock playerbots own
> combat setup, corpse loot, chest loot, and harvesting.**

A material run generally does the following:

1. Resolves the exact catalog item.
2. Builds or loads a reverse source index from creature loot and skinning data.
3. Loads current-map and current-zone creature spawns.
4. Plans bounded density hotspots and mmap-validated route segments.
5. Selects live, hostile, eligible source creatures.
6. Uses a module-owned attack action where selected gray creatures are needed.
7. Hands combat, chest/corpse loot, and harvesting back to stock playerbot
   actions.
8. Attributes exact item quantities from safe inventory observations and loot
   events.
9. Returns home after stop, timer expiry, quantity completion, bag reserve, or
   unrecoverable failure.

The module does not globally change `GrindTargetValue`, gray-target rules, loot
filters, sight distance, or playerbot movement behavior.

## Loot, chests, skinning, and corpses

For a selected creature, the intended sequence is:

```text
select -> approach -> attack -> combat -> normal corpse loot
-> corpse becomes skinnable -> stock Skinning -> corpse release
```

Important behavior:

- Gray creatures are accepted only when they match the selected source set.
- Stock loot is prioritized over material travel.
- Normal playerbot chest and gameobject loot remains enabled.
- During route movement and return, queued kills and chests are approached with
  normal bounded movement before stock playerbot actions open and loot them.
- All corpse items, including gray items, are eligible during a material run.
- SBRPG does not repeatedly open, close, replace, or clear stock loot targets.
- Empty or stale corpses receive bounded recovery time and are then released.
- A return request does not abandon active combat or post-combat corpse loot.
- Harvesting is not eligible until normal loot is complete.
- Corpse harvest requirements are resolved from source metadata and validated
  against the required profession, skill value, and tool.

## Level eligibility and safety boundary

The current release does not compare player level with source-creature level
when starting or selecting a material target. It validates source identity,
map/zone, hostility, attackability, line of sight, distance, and normal
creature rank, but a low-level character may still encounter a database-proven
source above its level.

Use material farming only in a zone appropriate for the character. A player-
versus-source level gate, including an explicit high-level exemption and clear
preflight rejection, is planned but is not currently implemented.

## Navigation and safety

- Routes are map-local and use mmap validation.
- `PATHFIND_NOPATH` and repeated no-progress objectives are temporarily
  blacklisted.
- Useful incomplete mmap corridors may be followed segment by segment.
- Movement is bounded and uses normal `MovementAction` pacing.
- No teleport, clipping, click-to-move injection, or unsafe direct spline
  movement is used, but the playerbot system isn't perfect, ymmv.
- Players, pets, totems, friendly creatures, bosses, and default-prohibited
  elites are not proactively targeted.
- Tapped, claimed, unreachable, evading, or otherwise unsafe creatures are
  skipped.
- Recovery, health, mana, and combat take priority over new pulls.
- AFK flags and unrelated global playerbot behavior are not manipulated.

## Session lifecycle

Every node and material run records its starting map, position, orientation,
start time, optional deadline, return reason, and return progress.

A run can return because of:

- explicit `.sbrpg stop`;
- duration expiry;
- quantity completion;
- configured bag reserve;
- unrecoverable activity failure.

During return, new targets are suspended. Existing combat, queued kills, chest
loot, corpse loot, fishing, and harvesting are allowed to resolve. The same
safe route logic is used to reach the recorded start. Strategies leased by
SBRPG are restored only after the activity ends.

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
SelfBotRpg.FishingBobberEntries = 35591
SelfBotRpg.UseLures = 1
SelfBotRpg.FishingPrioritizePools = 0
SelfBotRpg.FishingOpenWaterOnly = 1
SelfBotRpg.FishingSearchDistance = 500.0
SelfBotRpg.FishingCastDistance = 12.0
```

`MaterialMinimumChance` is a percentage used only for proactive creature
selection; diagnostic source commands still expose lower-chance sources.
`MaterialReservedBagPercent` stops material farming before the configured
percentage of bag capacity is used. `ActionDelayMs` limits SBRPG controller
decisions and does not change stock playerbot timing.

`FishingBobberEntries` is a comma-separated list of accepted bobber entries.
`UseLures` enables best-effort lure application; missing or unusable lures
never block fishing. `FishingOpenWaterOnly` defaults to `1`; set it to `0` and
set `FishingPrioritizePools` to `1` to prioritize matching pools before
open-water fallback. `FishingSearchDistance` accepts 60–2000 yards.
`FishingCastDistance` controls shoreline cast-point selection; lower values
keep the bot closer to the water when suitable terrain is available.

The addon gear button exposes these settings. Values are remembered per
character in `SelfBotRPGDB.Settings`, can be reset to documented defaults, and
are applied through the capability-gated `SET_CONFIG` protocol. Settings can be
applied to the current run and future runs; `.conf.dist` values remain startup
defaults after a worldserver restart. The `.sbrpg set` controls remain
available for active node-run overrides:

```text
.sbrpg set attempts 3
.sbrpg set failedblacklist 120
.sbrpg set emptyblacklist 120
.sbrpg set zone 1
.sbrpg set settledelay 1000
```

## Addon UI

`/sbrpg` toggles the SelfBot RPG panel. It provides:

- node profession and resource pickers;
- searchable, scrollable material and fishing catalogs;
- resource icons and metadata tooltips;
- duration and quantity goals;
- **Start Farming**, **Start Selected**, **Fish This Zone**, and **Stop**;
- a gear-button settings window with **Apply**, **Close**, and **Reset Defaults**;
- persistent panel selections and settings;
- status and diagnostic feedback.

Targeted fishing requires a selected fishing catalog item. Zone fishing uses
the current zone and does not require a fish selection.

Press **Enter** in any text input to confirm the entry and clear focus. Press
**Escape** to close the current window. Leaving the settings window with
Escape, the Close button, or its close button automatically applies settings.

The panel uses bounded popups so long catalogs do not extend off-screen. It
fades when idle and returns to full opacity while the cursor is over the main
frame or an open picker/edit control.

The minimap launcher is stored in `SelfBotRPGDB`:

- Left-click toggles the farming panel.
- Right-click opens settings.
- Drag repositions the launcher.

## Addon protocol

The addon uses the isolated, versioned `JLYRPG2` protocol described in
[`docs/addon-protocol.md`](docs/addon-protocol.md).

Supported operations include:

- `HELLO` / `CAPABILITIES` negotiation;
- node `START`, `STATUS`, `SET`, and `STOP`;
- `MATERIAL_CATALOG`;
- `MATERIAL_SOURCES`;
- capability-gated `SET_CONFIG`;
- `START_MATERIAL`;
- `START_FISHING`;
- `MATERIAL_STATUS`.

Catalog and source responses use bounded indexed frames. Requests may use
party or raid transport, but replies are whispered to the requesting selfbot
to prevent cross-talk. Existing node protocol frames and the manual
`/sbrpgchat` fallback remain isolated.

## Diagnostics and metrics

Set `SelfBotRpg.Debug = 1` to emit route, target, corpse, chest-loot handoff,
timeout, blacklist, return, fishing, and strategy messages to selfbot system
chat and the worldserver debug log.

Status and summaries expose, where applicable:

- current phase and reason;
- selected material, source, or target;
- route or hotspot progress;
- kills and loot events;
- exact requested item units;
- item and kill rates;
- fishing casts, phases, and cataloged catches;
- skinning state and corpse timeouts;
- return reason and remaining time.

Player-facing durations are formatted for readability, for example `45s`,
`3m 12s`, or `2h 5m 8s`. Internal millisecond values remain internal timing
values and are not used as user-facing status text.

A persistent wait should have a specific reason such as `combat`, `corpse loot
pending`, `approaching loot`, `harvesting`, `recovering`, `no safe hotspot`, or
`returning`.

## Capability matrix

| Capability | Current behavior |
|---|---|
| Node farming | Mining, herbalism, both, named resources, or current-zone mode. |
| Material farming | Creature loot, skinning/corpse harvest, fishing, and item-linked node requests where supported by the catalog. |
| Fishing | Targeted-fish or current-zone sessions; open water by default, optional pools (not tested yet), cataloged-fish tracking, and best-effort lures. |
| Level-appropriate source filtering | Not implemented; source identity, chance, map/zone, attackability, LOS, range, and rank are checked. |
| Cross-zone travel | Not implemented; the character must already be in the map/zone containing sources. |
| Session controls | Duration, quantity goal, bag reserve, stop-and-return, combat recovery, and captured start position. |
| Addon settings | Current module settings are exposed, persisted, applied, and resettable through the gear window. |
| Travel shortcuts | Hearthstones, portals, flight paths, transports, and travel planning remain future work. |
| Automated questing | Not implemented; planned future work. |

The addon intentionally presents only implemented capabilities. It does not
claim level gating, automatic zone travel, or quest automation until those
features are implemented and verified.

## Validation status and remaining work

Implemented and build-validated:

- shared activity sessions and start-position capture;
- mmap-safe node and material route following;
- cloth, leather/hide, cooking, mote, and primal catalog metadata;
- reverse loot-source indexing with references and grouped rows;
- spawn loading and hotspot planning;
- selected gray-source combat;
- stock combat, chest/corpse loot, and normal-loot-before-skinning ordering;
- quantity, duration, bag-reserve, diagnostics, and return-home lifecycle;
- addon capability negotiation, catalog chunks, material status, icons,
  tooltips, pickers, keyboard confirmation, and settings persistence;
- fishing search, cast/bobber/reel/loot flow, configurable distances, pole
  equipment, equipment restoration, and cataloged-fish accounting;
- queued kill and chest approach during routing and return;
- automatic selfbot enable/disable ownership handling;
- required full worldserver build.

Still requiring broader runtime or custom-database validation:

- player/source level-appropriateness filtering and low-level preflight rejection;
- long material and fishing soak tests;
- solo/party/raid addon cross-talk testing;
- all corpse-harvest multi-yield and external-skinning cases;
- custom database validation of Herbalism, Mining, and Engineering corpse harvest;
- custom loot-condition and custom-database coverage;
- live lure application, cast-distance terrain edge cases, bag handling, and
  equipment restoration;
- adaptive hotspot scoring and respawn learning;
- automatic zone selection/travel, flight paths, portals, and vendors;
- transformation, reputation, bag-disposal, and autonomous questing pipelines;
- external catalog/config-file loading, which is intentionally deferred.

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
```

## AI assistance disclaimer

Parts of this module and documentation were developed with AI-assisted coding
and review. AI assistance does not replace human testing, code review, server
administration, or responsibility for the behavior of a live game server.
Review changes before deploying them, test on a backup or development realm,
and do not assume that an AI-generated explanation is correct without checking
it against your AzerothCore and `mod-playerbots` versions.

## Roadmap / Jelly's notes

The long-term goal is a safe, transparent selfbot RPG assistant that builds on
normal playerbot capabilities without introducing global behavior changes.
Planned work includes level-aware source safety, broader profession and custom
database coverage, adaptive hotspot and respawn handling, safe travel planning,
and eventually an automated quest controller.

The current priorities are runtime soak testing, better diagnostics, and
careful validation of fishing, corpse harvesting, chests, bag behavior, and
return-home cleanup. External catalog loading is intentionally deferred until
the compiled catalog and database-backed source validation are stable.

When extending the module, preserve the project principles: use existing core
and playerbot APIs, keep movement mmap-validated and bounded, keep protocol
replies whisper-isolated, avoid fabricating travel completion, and prefer a
clear refusal over unsafe or ambiguous automation.
