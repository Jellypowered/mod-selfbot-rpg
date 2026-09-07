# Plan 2: Live herb and mining node awareness

## Status

The main prototype is implemented and has passed the required Lua, whitespace,
and full AzerothCore worldserver build checks. Live node awareness is considered
**beta-ready for broader user testing**, not production-complete.

Implemented:

- Structured live observations containing GUID, entry, stable `GetSpawnId()`,
  map, coordinates, spawned state, selectable state, and observation time.
- GUID-only live caching; raw `GameObject*` pointers are never retained.
- Route-point association by database spawn ID, with a conservative entry and
  coordinate fallback that refuses ambiguous matches.
- Route-point states for unknown, available, unavailable, travel candidate,
  gathering, and temporarily skipped nodes.
- Live-first handling while travelling. A visible usable node can replace the
  current database coordinate and reports a reroute status.
- Nearby confirmed-empty route points are blacklisted and replanned without
  waiting at the empty coordinate.
- Existing mmap-validated, bounded movement and normal movement priority remain
  in use.
- The stock playerbot mount strategy is leased during node, creature hotspot,
  and fishing runs. Before travel moves, it automatically evaluates riding
  skill, available mounts, combat state, outdoor state, and ground or flight
  restrictions, then selects an appropriate mount. A learned-spell fallback
  detects mounted auras in every effect slot so hybrid and exotic mounts are
  supported when the stock collector misses them. Fishing dismounts at water
  before casting and may remount between pools. This is working well in beta
  testing and needs refinement rather than a replacement design.
- Stock playerbot gathering and loot ownership remains unchanged.
- Multiple post-combat corpse GUIDs are retained for stock loot after combat
  interruption.
- Stalled loot and gather-pending recovery is bounded to 10 seconds; active
  casts and open loot windows remain protected.
- Optional `mod-junk-to-gold` coexistence requires no dependency. SBRPG does not
  dereference the temporary loot-item pointer used by the loot hook.

Recent validation also covers the settings-window tooltips and Close-button fix.
No commit is created by this plan.

## Current behavior

The database route is still the authoritative set of possible spawn points.
The live cache is authoritative only for objects observed in the player's loaded
area. A point outside the observation radius remains **Unknown**; absence from
the cache alone never proves that a distant point is despawned.

Every live scan is bounded by the existing playerbot sight distance, with a
minimum radius of 30 yards, and refreshes no faster than every 1.5 seconds.
When a scan observes a route point's area and finds no matching spawned,
selectable object, that point becomes unavailable for the current observation
and receives the configured temporary blacklist when it is the active target.

Before farming resumes, SBRPG gives stock loot priority. It does not manually
open, close, replace, or clear normal loot/gather targets. A live node is handed
to the stock gather/loot stack only after normal interaction-distance movement
and the bounded pending-gather window.

## Remaining work

The following items remain beta follow-up work rather than blockers for the
prototype:

- Soak testing dense, sparse, mostly-despawned, phased, and custom-database
  routes for both herbalism and mining, including both-profession mode.
- Verify behavior when nodes spawn or despawn during movement, when several live
  nodes are visible at once, and when a node has multiple gathering yields.
- Confirm the core's loaded-grid behavior for unspawned gameobjects and unusual
  custom-database spawn IDs; ambiguous coordinate matches must remain skipped.
- Tune the minimum-improvement rule and objective switching if live nodes cause
  oscillation. The scan interval currently provides the primary debounce.
- Add isolated tests for association, duplicate observations, stale records,
  ambiguous coordinate matches, and route selection where practical.
- Improve debug summaries so selection, association, availability, blacklist,
  danger, and reroute reasons can be audited without excessive chat spam.
- Add a shared danger evaluator for nodes, material hotspots, live-node
  approaches, and travel corridors. Initially blacklist a candidate for 120
  seconds when three or more hostile creatures are within 15 yards, or when
  nearby two to three level higher enemies form a group unlikely to be
  soloable. Revalidate before entering the corridor and keep this separate
  from empty and no-path blacklists.
- Revisit live availability checks if a core/playerbot revision exposes a more
  authoritative selectable or gatherable state than the current flags and
  `LootObject` validation.
- Validate queued corpse/chest loot, combat interruption, return-home behavior,
  manual stop, timer completion, quantity completion, and bag reserve together
  with live-node rerouting.

## Safety and non-goals

This work does not add teleportation, clipping, fabricated visibility, unsafe
spline movement, GM relocation, cross-zone travel, or global playerbot changes.
It does not load an external catalog or configuration file. It does not give
SBRPG ownership of stock combat, loot, or gathering actions.

Return-home remains authoritative. Once return begins, live-node discovery does
not replace the captured start route or revive an empty farming objective.
Combat, active loot, corpse processing, and harvesting retain their existing
priority and bounded lifecycle.

## Beta test matrix

1. Mine and herb in dense spawn clusters with debug logging enabled.
2. Run routes containing both live and despawned database points.
3. Watch for a visible node replacing a distant database objective and confirm
   the addon reports `rerouting to live node`.
4. Move through areas where nodes spawn and despawn while travelling.
5. Test mining, herbalism, both-profession, resource mode, and zone mode.
6. Interrupt with combat, including multiple attackers and multiple corpses.
7. Confirm corpses and chests are approached and handed to stock loot before
   farming resumes.
8. Verify normal, flying, and exotic mount selection before node travel and
   live-node reroutes; verify walking fallback when no valid mount exists.
9. Test danger screening around dense camps, higher-level groups, and travel
   corridors, including 120-second rediscovery after cooldown.
10. Test normal node gathering, delayed/multiple yields, and gather-pending
    timeout recovery.
11. Test stop, timer, quantity, bag reserve, death/recovery, and return-home.
12. Repeat with `mod-junk-to-gold` installed and absent; compare actual item
    inventory gains rather than relying only on the brief loot-window display.

Record the current phase/reason, route spawn ID, live GUID/entry, distance, and
worldserver debug output for any failure. Report whether the node or corpse was
loaded, selectable, in range, owned by stock loot, gathering, or blacklisted.

## Required validation command

Run this as one command sequence from the module/core checkout:

```bash
set -e
luac -p /stuff/Source/azerothcore-wotlk/modules/mod-selfbot-rpg/addon/SelfBotRPG/SelfBotRPG.lua
git -C /stuff/Source/azerothcore-wotlk/modules/mod-selfbot-rpg diff --check
/stuff/Source/azerothcore-wotlk/acore.sh compiler build
```

## Architecture follow-up

The working beta implementation has been extracted into Nodes, Awareness, Movement, Materials, Fishing and shared service directories. See [architecture](docs/architecture.md). This does not change the historical beta outcomes or establish a new live-play test pass.
