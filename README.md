# mod-selfbot-rpg

Gathering-focused RPG behavior for **mod-playerbots self-bots**.

## Install

1. Place the module in `modules/mod-selfbot-rpg`.
2. Re-run CMake and rebuild worldserver.
3. Copy `addon/SelfBotRPG` to the WoW client's `Interface/AddOns`.
4. Enable self-bot mode: `.playerbots bot self`.

## Farming commands

```text
.sbrpg farm mining copper
.sbrpg farm mining fel iron
.sbrpg farm herbalism peacebloom
.sbrpg farm zone mining
.sbrpg farm zone herbalism
.sbrpg farm both zone
.sbrpg status
.sbrpg stop
```

Named modes resolve only the selected resource. Zone modes resolve known mining,
herbalism, or combined resource families, then retain spawn points in the zone
where farming started.

## Route behavior

- Route points are loaded once at farm start and visited in cycles.
- A nearby **currently spawned** selected node is preferred over a database route point.
- Candidate legs are scored using mmap `PathGenerator` cost for a bounded nearby shortlist.
- `PATHFIND_NOPATH`, empty nodes, and repeated no-progress nodes are temporarily blacklisted.
- Movement follows the reachable mmap endpoint, using normal self-bot pacing.
- The bot stops at the node and waits the configured settle delay before gathering.
- Normal playerbots `+loot` is enabled during a farm run when this module added it.

## Configuration

All defaults are in `conf/mod-selfbot-rpg.conf`:

```ini
SelfBotRpg.Debug = 0
SelfBotRpg.AttemptsBeforeBlacklist = 3
SelfBotRpg.FailedNodeBlacklistSeconds = 120
SelfBotRpg.EmptyNodeBlacklistSeconds = 120
SelfBotRpg.StayInCurrentZone = 1
SelfBotRpg.GatherSettleDelayMs = 1000
```

Active runs may override defaults:

```text
.sbrpg set attempts 3
.sbrpg set failedblacklist 120
.sbrpg set emptyblacklist 120
.sbrpg set zone 1
.sbrpg set settledelay 1000
```

## Addon

`/sbrpg` opens the farming panel. It provides profession/resource selection,
per-run override inputs, current status, and Start/Stop controls.

The minimap button persists its position in `SelfBotRPGDB`:

- **Left-click:** toggle the farming panel.
- **Right-click:** open the panel for settings.
- **Drag:** reposition the minimap button.

## Diagnostics and metrics

Set `SelfBotRpg.Debug = 1` for route-load, path, and blacklist messages in
self-bot system chat and worldserver debug logs. Status reports eligible route
nodes, gathers, items received from the active gathering node, item/gather
rates, and effective per-run settings.

## Current limits

The addon uses the versioned `SBRPG` addon-message protocol for commands,
structured status, setting acknowledgements, errors, and debug frames. See
`docs/addon-protocol.md`. Route planning is map/zone scoped—flight paths,
vendors, bags, leveling mode, and autonomous questing are future features.
