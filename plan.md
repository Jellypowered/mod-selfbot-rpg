# mod-selfbot-rpg rewrite plan

## Objective

Replace the current prototype with a small, testable farming controller that:

- travels naturally over AzerothCore mmaps, including connected caves and tunnels;
- retains its objective through combat and temporary movement failures;
- delegates all corpse and gathering loot lifecycle work to mod-playerbots;
- never teleports, clips, flies accidentally, or repeatedly resets movement splines;
- exposes an explicit, non-spammy activity state in the addon;
- remains a no-playerbots-source-edit module.

The rewrite should replace the implementation rather than incrementally preserve prototype behavior.

## Confirmed lessons from the prototype

1. Selecting a new route point every AI tick causes backtracking and destination loss.
2. Sorting nodes by distance from the starting point creates expanding rings, not a route.
3. A path-cost cache keyed only by spawn is invalid after the bot moves.
4. `PathGenerator::GetActualEndPosition()` is not a complete long-range route.
5. `GetPath()[0]` is normally the current position and is not a steering target.
6. Reissuing short `MoveTo` calls repeatedly resets splines and produces robotic movement/fall damage.
7. `PATHFIND_SHORTCUT` must never be treated as a safe corridor; it may cross geometry.
8. `PATHFIND_INCOMPLETE` can represent useful forward progress and is required for long/winding routes.
9. Caves are generally areas on the same map. Blanket indoor filtering prevents valid cave navigation.
10. Area/zone changes are normal; only a true map change invalidates map-local routes.
11. Manually assigning `loot target` and repeatedly calling `open loot` interferes with playerbots' loot lifecycle.
12. A successful `open loot` action is not proof that gathering or looting completed.
13. Corpse looting must remain owned by the stock `loot` strategy so corpses are fully emptied.
14. `ToggleAFK()` races playerbots and creates repeated Away messages. If AFK is cleared, remove the flag directly.
15. Addon messages require a unique prefix, exact envelope parsing, every relevant `OnPlayerCanUseChat` overload, and verified packet construction.
16. Do not mix chat-command fallback and addon transport automatically; ambiguous traffic caused inventory/trade side effects.

## Rewrite boundaries

### Keep

- No-upstream-source-edit context registration pattern from `mod-dungeon-clear`.
- Existing command names and configuration keys where practical.
- Exact mining/herbalism resource registry and profession-lock validation.
- Companion addon concept, minimap launcher, and separate settings panel.

### Delete and rebuild

- Current monolithic farm action.
- Manual `loot target`/`open loot` transaction handling.
- Radial/2-opt route implementation tied directly to movement ticks.
- Indoor-node exclusion heuristic.
- Protocol/chat translation layer.
- Per-tick object scans and mutable state spread across unrelated callbacks.

## Proposed source layout

```text
src/
  SBRPG.h                         public module API only
  SBRPG_loader.cpp
  SbrpgModule.cpp                 script registration and lifecycle
  Farm/FarmController.h/.cpp      state machine and transitions
  Farm/FarmState.h                one state object per active character
  Farm/ResourceRegistry.h/.cpp    exact profession/resource entries
  Farm/NodeRepository.h/.cpp      DB route loading and live-node cache
  Farm/RoutePlanner.h/.cpp        bounded candidate ordering
  Farm/RouteFollower.h/.cpp       cached mmap polyline follower
  Farm/GatherCoordinator.h/.cpp   handoff to stock playerbots loot stack
  Farm/StatusPublisher.h/.cpp     deduplicated telemetry
  Protocol/SbrpgProtocol.h/.cpp   isolated addon transport
  Commands/SbrpgCommand.cpp       explicit chat fallback
addon/SelfBotRPG/
  SelfBotRPG.toc
  Core.lua                        addon lifecycle and saved variables
  Bridge.lua                      protocol only
  FarmingPanel.lua
  SettingsPanel.lua
  Minimap.lua
```

No module should depend on headers from `mod-dungeon-clear`; copy/adapt only the minimal algorithmic ideas needed and keep attribution comments.

## Farm state machine

Use one authoritative enum, not inferred booleans:

```text
Stopped
Planning
SelectingNode
BuildingPath
Travelling
CombatPaused
ApproachingLiveNode
GatherPending
Looting
Recovering
Waiting
ReturningHome (roadmap)
Failed
```

`FarmState` owns:

- run ID and monotonic state revision;
- map, starting zone, and starting position;
- profession and allowed template entries;
- persistent objective spawn/GUID;
- planned route and route cursor;
- cached path segments and follower cursor;
- live-node cache with refresh timestamp;
- per-node failure/empty cooldowns;
- progress timestamps and last distance;
- metrics and last human-readable reason;
- next controller update time.

Every state transition records `state`, `reason`, and `changedAtMs`. The addon displays these directly.

## Navigation design

### 1. Candidate selection

- Load only exact registered mining/herbalism entries.
- Keep same-map nodes; optionally constrain the starting zone.
- Do not exclude caves or indoor areas.
- Prefer a spawned, gatherable live node only when a safe mmap route exists.
- Otherwise evaluate a bounded nearest set (for example 12–20 geometric candidates) using actual mmap path cost.
- Persist the selected objective until success, confirmed empty, hard path failure, or timeout.
- Never advance the route cursor merely because the controller tick ran.

### 2. Route planning

Build an initial nearest-neighbor order once. Cost entries must include an origin cell/revision, not only a destination spawn. Optional 2-opt refinement is bounded and performed off the movement hot path. Route order is advisory; live-node and blacklist state may skip points.

### 3. Cave/tunnel and long-range paths

Adapt the verified ideas from `mod-dungeon-clear`'s `ChunkedPathfinder`/`StridedPathfinder`:

- build a chain of `PathGenerator` results from the bot toward the objective;
- accept `PATHFIND_NORMAL` as complete only when the actual endpoint lands near the requested destination;
- accept `PATHFIND_INCOMPLETE` as a safe forward segment, then continue planning from its endpoint;
- reject `PATHFIND_NOPATH`, `PATHFIND_SHORTCUT`, and far-from-poly destination failures;
- retain each returned smooth polyline, dropping its leading current-position point;
- cap chained depth and total planning work;
- expose exact failure reasons (`no start poly`, `no target poly`, `no progress`, `depth limit`).

This allows routes through cave entrances because navigation follows connected mmap polygons instead of drawing a straight line to an underground coordinate.

### 4. Route follower

- Cache the built polyline/segments for the active objective.
- Walk points sequentially using terrain-generating `MoveTo`/`MovePoint` with `forceDestination=false`.
- Do not issue a new movement command while the current spline is active and making progress.
- Use short-enough segments to avoid `PATHFIND_SHORT` and long enough segments to avoid robotic micro-steering.
- Rebuild only after combat displacement, meaningful deviation, a stale path revision, or a progress timeout.
- Never teleport or use a non-path-generated straight spline.
- Disable/dismount flying state for ground gathering; mounting, if retained, must be ground-only.

### 5. Recovery

Adapt dungeon-clear's conservative far-from-poly recovery:

- test four short cardinal offsets;
- Z-snap each with `UpdateAllowedPositionZ`;
- require a real path and reject excessive detour ratios;
- issue one short terrain-generated recovery move;
- invalidate and rebuild the cached route afterward;
- blacklist only after bounded recovery attempts fail.

## Combat behavior

Combat pauses the route follower but does not clear the objective or route cursor. After combat:

1. retain the same node;
2. invalidate only the cached path if displacement exceeds a threshold;
3. rebuild from the current position;
4. resume travel;
5. abandon the node only on a hard failure or configured timeout.

Normal playerbots combat strategies remain authoritative.

## Gathering and corpse looting

### Core rule

SBRPG selects destinations; playerbots owns loot.

At farm start, enable the stock `loot` and `gather` non-combat strategies only if absent, remembering ownership for cleanup. Do not directly set `loot target` and do not invoke `open loot` from SBRPG.

When a selected live node is nearby:

- add its GUID through the existing `add gathering loot`/`LootObjectStack` flow;
- transition to `GatherPending`;
- let stock triggers execute `loot` → `move to loot` → `open loot`;
- observe loot/spawn state hooks for completion;
- do not suppress ordinary corpse entries in `available loot`;
- wait until the stock loot stack/target has resolved before resuming route travel.

This preserves complete corpse looting and avoids hung loot-window retries.

Completion must be based on one of:

- item-loot hook attributed to the gathering object;
- node loot state/despawn change;
- stock loot target/stack no longer containing the node after a successful interaction.

Timeouts release the pending transaction and cooldown the node; they never repeatedly invoke `open loot`.

## Module-only activity lease

Use `PlayerScript::OnPlayerUpdate` for active runs:

- directly remove `PLAYER_FLAGS_AFK` if present—never call `ToggleAFK()`;
- stand only when the controller intends to travel/gather and the character is not eating/drinking/casting;
- drive the farm controller at a bounded cadence (for example 250–500 ms) so playerbots passive scheduling cannot strand it;
- skip controller movement during combat, death, taxi, teleport, loading, vehicle, or logout states;
- stop immediately when the run ends.

This remains local to this module and changes no playerbots source/configuration.

## Live-node cache

- Scan at most every 1–2 seconds and only while selecting/approaching.
- Cache GUIDs, never raw pointers.
- Validate `IsInWorld`, spawned state, entry, profession lock, and loot possibility on use.
- Prioritize by safe path cost, not straight distance alone.
- Remove entries immediately on loot-state/despawn notification.
- Do not scan while a cached route spline is progressing unless a nearby-node preemption interval expires.

## Addon protocol

Implement the protocol only after server tests pass. Until then, keep chat fallback explicitly selectable for development rather than automatic.

Protocol requirements:

- unique prefix, e.g. `JLYRPG2`;
- envelope: `version~opcode~requestId~payload`;
- URL-encode payload fields, following MultiBotBridge conventions;
- `HELLO`/`HELLO_ACK` capability handshake before any command;
- request IDs and one ACK/ERROR per mutation;
- server consumes only exact prefix/version/opcode traffic through all relevant `OnPlayerCanUseChat` overloads;
- server replies using the same incoming distribution, as MultiBotBridge does;
- no normal whisper/system-chat parsing in bridge mode;
- no automatic fallback if handshake fails—the panel shows `Bridge unavailable` and offers a manual chat fallback button.

Split addon code so protocol code cannot trigger inventory/trade UI code. Register the prefix and validate sender is the local character/server echo expected by the transport.

## Telemetry and perceived efficiency

The panel is the source of truth. Publish only when state revision changes, plus a low-rate heartbeat while visible.

Status payload:

```text
runId, revision, state, reason, profession, objectiveName, objectiveSpawn,
routeIndex, routeCount, distance, gathers, items, itemsPerMinute, elapsed
```

Examples:

- `Building cave route (segment 2/6)`
- `Travelling to Copper Vein — 84 yd`
- `Combat paused — resuming Copper Vein afterward`
- `Gather pending — stock loot action`
- `Recovering — start is off navmesh`
- `Waiting — 14 nodes temporarily unavailable`

Do not echo settings in recurring status. Send effective settings once in `START_ACK` and on explicit settings request/change.

## Performance limits

- Controller cadence: 250–500 ms.
- Live scan: 1–2 seconds.
- Status heartbeat: 2 seconds only while panel is visible.
- Candidate mmap probes: bounded count per planning slice.
- Long-route chain: bounded depth and wall-clock budget.
- Route refinement: bounded nodes/passes and outside movement ticks.
- Database route query: once per start/map/zone change, never per tick.
- No raw-pointer caches across updates.

## Implementation phases

### Phase 0 — Freeze and baseline

1. Tag the current prototype before replacement.
2. Add a deterministic test command that dumps state/path decisions to the server log.
3. Capture Jasperlode, Fargodeep, open-terrain, combat-interrupt, and corpse-loot reproduction cases.

**Exit:** each known failure has a reproducible scenario and expected result.

### Phase 1 — State/controller skeleton

1. Replace `SBRPG.cpp` with split components and the explicit state machine.
2. Implement lifecycle, commands, state revisions, activity lease, and status text.
3. No autonomous movement yet.

**Exit:** start/stop/status are deterministic; AFK does not sit/spam; no loot behavior changes.

### Phase 2 — Stock loot integration

1. Enable/restore `loot` and `gather` strategies safely.
2. Add selected nodes through `available loot` only.
3. Implement completion observation and bounded timeout.
4. Verify corpse looting remains complete.

**Exit:** corpses disappear after full loot; nodes gather once; no hung-window loop.

### Phase 3 — Path builder/follower

1. Implement endpoint-validated direct paths.
2. Implement chained incomplete-path segments.
3. Implement cached point follower with no spline spam.
4. Add conservative off-poly recovery.

**Exit:** open terrain is smooth; no fall damage/clipping; cave routes use entrances.

### Phase 4 — Route planner/live nodes

1. Add bounded path-cost candidate selection.
2. Add persistent objective and route cursor.
3. Add live-node cache/preemption and blacklist reasons.
4. Add combat resume.

**Exit:** 30-minute farm continues beyond sight range, resumes after combat, and does not bounce.

### Phase 5 — Protocol/addon

1. Implement and packet-test `JLYRPG2` handshake and request/response protocol.
2. Split addon into core/bridge/panels/minimap files.
3. Add revision-driven telemetry and manual fallback.
4. Validate minimap SavedVariables across reload/login.

**Exit:** no inventory/trade/whisper side effects solo, party, or raid.

### Phase 6 — Documentation and timed farming

1. Update configuration/command/protocol docs.
2. Add timed farming and return-home state using the same path follower.
3. Keep combat active during return travel.

## Verification matrix

### Automated

- Compile every new C++ translation unit with `compile_commands.json`.
- `luac -p addon/SelfBotRPG/*.lua`.
- `git diff --check`.
- Route tests: endpoint validation, incomplete-chain progress, shortcut rejection, cursor persistence, blacklist expiry.
- State tests: combat pause/resume, map-change stop, gather timeout, AFK lease cleanup.
- Protocol tests: malformed frames, wrong prefix/version, duplicate request ID, sender/distribution checks.

### In game

1. Elwynn open terrain: smooth travel, no fall damage.
2. Jasperlode and Fargodeep: enter through cave mouth; no mountain clipping.
3. Combat interruption: same objective resumes.
4. Node unavailable: visible reason, cooldown, next node selected.
5. Hung gather: one attempt, timeout, route continues.
6. Corpse loot: every lootable item removed; corpse becomes non-lootable/disappears normally.
7. Beyond sight range: route continues for at least 30 minutes.
8. Solo/party/raid addon operation: no trade windows or inventory whispers.
9. AFK duration: character remains standing/active without Away-message loops.
10. `/reload` and relog: minimap position persists.

## Definition of done

The rewrite is complete only when all verification cases pass, debug/status always explains stationary behavior, no manual loot-window ownership remains, and no movement path type capable of crossing geometry is accepted.
