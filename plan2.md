# Plan 2: Live herb and mining node awareness

## Objective

Make node farming choose and re-plan around nodes that are actually spawned and
available, instead of treating every database spawn point as an equally valid
route destination.

Today the node controller loads database spawn points into a circular route and
uses the live world scan mainly after it reaches the current route objective.
This creates the observed behavior:

1. A database spawn is selected.
2. The node is already despawned, but the controller cannot confirm that until
   it reaches the spawn location.
3. The bot pauses briefly at the empty location.
4. The route advances to another database point.

The desired behavior is to detect unavailable nodes as soon as the relevant
area becomes visible, remove or deprioritize them for the current pass, and
recalculate the next reachable objective while continuing normal movement.

## Current implementation findings

### Database route

`NodeRepository::LoadRoute()` queries `gameobject` rows for the selected map,
entry IDs, and optional starting zone. It stores database GUID, entry, and
coordinates as `RoutePoint` records. These records describe possible spawn
locations, not currently spawned gameobjects.

`NodeRepository::SelectNextRoute()` walks this route, using `visited` and
`blacklistedUntilMs`. It does not know whether a route point currently has a
spawned gameobject.

### Live scan

`NodeRepository::ScanLive()` scans the player's current object range and keeps
spawned gameobject GUIDs whose entries match the selected node entries. The
cache refreshes approximately every 1.5 seconds and is correctly GUID-based,
which avoids retaining unsafe raw pointers.

However, the live cache currently does not map a visible spawned object back to
its database route point or spawn ID. It is used to inspect nearby live nodes,
but it does not filter the database route before a distant route objective is
selected.

### Controller behavior

`SelfbotRpgFarmAction::Execute()` currently:

- refreshes the nearby live-node cache;
- gives nearby live nodes an opportunity to enter stock playerbot gathering;
- retains a database route objective in `currentSpawn`;
- only declares a route point empty when the bot is already within roughly five
  yards and no matching live node was found;
- blacklists an empty point only after arriving there;
- builds and follows bounded mmap route segments toward that point.

This explains why the bot can travel toward a known-but-currently-despawned
node. It also explains the short pause: the controller waits until the next
update/action cycle after reaching the empty coordinate before selecting again.

There is also unreachable legacy-looking logic after a `continue` in the live
node handoff loop. It should be reviewed during implementation and either
removed or deliberately restored; it must not be allowed to obscure the new
selection state machine.

### Important distinction

A database point that is not visible is not automatically despawned. It may be
outside the loaded grid, outside the configured sight range, phased differently,
or temporarily unavailable. The controller must only classify a point as
currently unavailable after it is within a defined observation radius and the
server confirms that no matching spawned object is present.

## Proposed design

### 1. Add explicit node observation data

Extend the node cache or add a companion live-node index containing, for each
visible spawned node:

- object GUID;
- gameobject entry;
- database spawn ID (`GetSpawnId()`);
- map and phase context;
- current coordinates;
- last-observed timestamp;
- spawned/selectable/lootable state where available.

Keep raw pointers out of persistent state. Resolve the object from its GUID only
for the duration of an update.

The database route remains the authoritative list of possible locations. The
live index becomes the authoritative list of currently observed objects.

### 2. Build a route-point/live-node association

When a live node is observed, associate it with its route point using the
stable database spawn ID where possible. Do not use
`ObjectGuid::GetCounter()` as a substitute for the database spawn ID.

For custom databases or unusual gameobject GUID behavior, use a conservative
fallback based on entry plus a small coordinate tolerance, and record that the
association is approximate. Never allow an ambiguous association to mark an
unrelated database point as spawned.

The association should support both directions:

- route point -> currently observed live node;
- live node -> route point/spawn ID.

### 3. Introduce observation states

Each route point should be treated as one of these states for the current route
pass:

- **Unknown:** not observed recently; it may or may not be spawned.
- **Visible and available:** matching spawned node is visible and can be
  gathered, subject to profession/tool/loot checks.
- **Visible but unavailable:** matching object is visible but despawned,
  unspawned, not selectable, on cooldown, or otherwise not gatherable.
- **Travel candidate:** selected as a destination because it is the best known
  candidate under the current policy.
- **Gathering:** handed to stock playerbot gather/loot handling.
- **Temporarily skipped:** unavailable or failed during this route pass, with a
  bounded retry/blacklist time.

Unknown points must not be treated as confirmed empty merely because they are
not in the live cache.

### 4. Use live-first selection when nodes are observed

Selection policy should be:

1. Finish active combat, loot, and gathering lifecycle first.
2. Prefer visible, available nodes that match the profession, entry, phase,
   zone, and normal lootability rules.
3. Among available nodes, choose the best route candidate using current travel
   distance, mmap reachability, and existing route/hotspot ordering.
4. If no visible available node exists, select an Unknown database point only
   when it is necessary to discover new territory or when no better candidate
   exists.
5. Never deliberately travel to a point already classified as
   Visible-but-unavailable or Temporarily-skipped.

This preserves the ability to discover newly spawned nodes while avoiding
known-empty points.

### 5. Recalculate while travelling

While following a route segment, refresh live observations at the existing
bounded interval. At each refresh:

- detect newly visible available nodes;
- detect selected nodes that have despawned or become unavailable;
- compare the current objective with newly available alternatives;
- re-plan if the current objective is no longer valid or a materially better
  available node has appeared;
- preserve the current mmap movement constraints and movement priority.

Replanning must not happen every frame. Use a cooldown/debounce, for example
500–1500 ms, and do not interrupt an active cast, gather spell, combat, loot
window, or return-home completion step.

A nearby newly visible node should be allowed to win over a distant unknown
route point. A minor distance improvement should not cause oscillation.

### 6. Handle the selected node safely

Before moving toward a selected objective, confirm its state:

- If a matching live object is visible and available, route to that object or
  its validated interaction position.
- If only the database point is known, retain the existing bounded route to the
  point for discovery.
- If the point becomes visible and no matching spawned object exists, mark it
  unavailable immediately, clear `currentSpawn`, invalidate the current route
  step, and select another candidate without waiting at the coordinate.
- If the object despawns while approaching, clear the objective and re-plan on
  the next debounced scan.
- If the object is gathered and disappears, complete the existing gather
  accounting and re-plan without treating the old pointer as live.

The empty-node blacklist should begin when unavailability is confirmed, not
only after a full arrival-and-pause cycle.

### 7. Preserve stock playerbot ownership

SBRPG should continue to choose node objectives but leave opening, gathering,
loot windows, and multi-yield behavior to stock playerbot strategies.

The live-node work must not:

- directly open or close node loot windows;
- clear stock loot targets during normal gathering;
- repeatedly requeue a node already owned by stock gather;
- replace stock loot or gather strategies;
- change global sight distance or gray-target behavior.

The existing `pendingGatherNode`, `activeGatherNode`, and stock-ownership checks
should be retained but represented as explicit state transitions in the revised
selection flow.

### 8. Make return behavior separate and authoritative

When a run is returning:

- do not discover new database route points;
- finish current combat, corpse/chest loot, and gathering as currently
  required;
- allow queued live loot to be approached first;
- do not switch back to an empty node objective;
- do not let live-node discovery override the captured return route;
- continue using the existing bounded mmap return path.

The selected farming node state should be cleared when return begins, while
active combat and loot GUIDs remain protected until their normal lifecycle
finishes.

## Suggested implementation phases

### Phase A: instrumentation and tests first

1. Add debug-only observations for route spawn ID, live GUID, entry, distance,
   live-cache age, selected objective, and selection reason.
2. Log transitions between Unknown, Available, Unavailable, Gathering, and
   Skipped.
3. Reproduce with a short route containing both spawned and despawned nodes.
4. Confirm whether the client/core loads despawned gameobjects into the nearby
   grid and how `isSpawned()`, selectable flags, and `GetSpawnId()` behave.
5. Verify behavior for mining, herbalism, both-profession runs, zone mode, and
   phased/custom database rows.

### Phase B: live index and association

1. Extend `LiveNodeCache` with structured observation records.
2. Add stable spawn-ID association to route points.
3. Add freshness and observation-radius rules.
4. Add unit-level or isolated tests for association, stale records, duplicate
   entries, and ambiguous coordinate matches.

### Phase C: live-aware candidate selection

1. Refactor `SelectNextRoute()` or introduce a live-aware selector rather than
   embedding all policy in `SBRPG.cpp`.
2. Prefer visible available nodes.
3. Skip confirmed unavailable nodes immediately.
4. Retain Unknown points as discovery candidates.
5. Preserve existing blacklist, visited-pass, current-zone, and profession
   filtering rules.

### Phase D: debounced in-flight replanning

1. Add a replan reason and cooldown to `FarmState`.
2. Re-scan while travelling at the existing bounded interval.
3. Replan when the selected node disappears or a better available node is
   found.
4. Invalidate only SBRPG movement steps; never interrupt active stock actions.
5. Ensure no oscillation between two nearby nodes.

### Phase E: cleanup and return integration

1. Remove unreachable legacy node-handoff code after the existing `continue`.
2. Consolidate selected-node clearing and blacklist transitions.
3. Confirm queued chest/corpse loot still takes priority.
4. Confirm return-home never waits on a confirmed-empty gathering node.
5. Confirm strategy leases and selfbot lifecycle are unaffected.

### Phase F: live soak validation

1. Mine and herb in zones with dense spawn clusters.
2. Test mostly despawned routes and sparse routes.
3. Test nodes despawning while approaching.
4. Test nodes spawning while travelling.
5. Test nodes with multiple yields and delayed disappearance.
6. Test occupied, tapped, unselectable, phased, and unreachable nodes.
7. Test manual stop, timer completion, bag reserve, combat interruption, and
   return-home while a node is visible or becomes unavailable.
8. Test custom-database spawn IDs and missing zone/area metadata.
9. Verify no inventory, loot-target, strategy, or shutdown regressions.

## Configuration and tuning considerations

Avoid adding many user-facing controls initially. The first implementation
should reuse existing values where possible:

- `sPlayerbotAIConfig.sightDistance` for observation range;
- existing action delay for controller throttling;
- existing node blacklist settings for confirmed unavailable nodes;
- existing mmap route validation and movement priorities.

If tuning proves necessary, consider internal constants or later configuration
for:

- live observation refresh interval;
- objective-replan cooldown;
- coordinate association tolerance;
- minimum improvement required before switching objectives;
- unknown-point discovery radius.

Any new settings should be documented in the addon only after runtime behavior
is stable.

## Acceptance criteria

The implementation is complete when:

- The bot does not intentionally travel to a node already confirmed despawned
  within the observation range.
- A selected node that despawns while approaching is cleared and replanned
  without a one-second arrival pause.
- Newly visible available nodes can replace distant unknown database points,
  subject to a debounce and meaningful-improvement threshold.
- Unknown database points remain discoverable when no live candidate is known.
- Stock loot/gather behavior remains responsible for interaction and multi-yield
  handling.
- Chests, combat corpses, and fishing loot retain priority during movement and
  return.
- Return-home movement is never overridden by node discovery.
- Node, material, fishing, addon, selfbot lifecycle, and shutdown behavior remain
  stable.
- Debug output explains why a node was selected, skipped, replanned, or
  blacklisted.
- Lua syntax, `git diff --check`, full compiler build, and live soak tests pass.

## Explicit non-goals

This plan does not introduce:

- teleportation or unsafe movement;
- client-side fabricated node visibility;
- global changes to playerbot target selection;
- a requirement to load an external catalog;
- cross-zone travel;
- direct SBRPG ownership of stock gather or loot actions.
