# Runtime architecture

This is a behavior-preserving extraction of baseline `2655c3b0cf55f1809b8e081858fbbd00fa83604b` on branch `restructure`, not a new feature release. `plan3.md` remains a local ignored planning document.

## Ownership

- `SBRPG.cpp`: script registration only. `SBRPG.h`: compatibility API.
- `Core/`: registry storage, configuration, logging, activity transitions and return requests. Registry access remains world-thread-only; no worker threads were introduced.
- `Nodes/`: node state, database candidates, resource resolution, live-first selection, lifecycle, controller and interaction handling.
- `Awareness/`: loaded-object observations, cache and stable spawn association. Observations retain GUIDs, not persistent world-object pointers.
- `Movement/`: bounded path planning/following, travel movement, mount selection/fallback and return movement.
- `Materials/`: catalog, loot sources, hotspots, lifecycle and creature-farming action.
- `Fishing/`: source queries, equipment, state, startup and fishing controller. Fishing reuses shared travel movement rather than the creature attack controller.
- `Combat/`: recovery and target eligibility. Stock playerbots still owns execution of combat.
- `Loot/`: item/kill events and stock node-loot recovery. No dependency on junk-to-gold headers or symbols.
- `Safety/`: existing eligibility checks; future danger/blacklist contracts are explicitly unavailable.
- `Protocol/`: existing codec/negotiation/status, request handling/configuration, response publication.
- `Integration/`: playerbot ownership, strategy leases, action registration, player/world hooks and command adapter. `RuntimeDependencies.h` is a private implementation-only SDK import bundle, not a public service or state container.
- `Reputation/`, `Questing/`, `Travel/`: future request types and unavailable service skeletons. Travel here means future multi-stage transport, not current local movement.

Internal services have domain headers. Core registry storage has one owner. Existing public function signatures and model namespaces remain compatible. Node controller short-circuit helpers use `optional<bool>`: an engaged `false` means stop this tick, while `nullopt` means continue. This distinction preserves the original early returns.

Fishing fields retain their names and defaults in a `FishingState` base of `MaterialFarmState`; this is in-memory state, not a serialized wire format. The transient fishing controller contains no persistent activity state. Persistent state remains registry-owned.

## Unimplemented features

Skeleton `Availability()` results always report `implemented=false` with an explicit reason. They have no script registration, protocol advertisement, timers, queries, movement, or stock strategy changes. Danger results default to `Unknown`, not `Safe`. Adaptive scoring, new danger screening/blacklists, reputation farming, quest automation, and transport routing remain future work. Existing node blacklists and existing fishing behavior are not replaced by these skeletons.

## Compatibility and verification

Configuration keys/defaults, commands, wire fields, action names/priorities, stock strategy ownership, timers and thresholds are preserved. All new `.cpp` files are collected through the existing recursive CMake source collector; implementation files are never included as headers.

The pre-refactor compiler build passed. That does not establish the full Plan 2 live-play matrix: exhaustive runtime beta verification was not available for this refactor. No claim of production readiness or completed future capabilities is made.

Required runtime regression checks after deployment:

- Herb/mining live-first routes, missing nodes, reroutes and timed return.
- Mount selection/fallback, cast waits, unavailable mounts, interaction dismounts.
- Materials combat interruption, multiple corpses, skinning, stalled loot recovery, bag reserve and quantity/timer completion.
- Fishing zone/item sources, pools/open water, lure/pole handling, reeling and equipment restoration.
- Stop, death, map change, configuration disable and cleanup/strategy restoration.
- Addon negotiation/settings/status and equivalent chat commands.
- With and without optional junk-to-gold; immediate gray-item destruction must remain harmless.

No addon source, database schema, protocol revision or configuration format change is required.

### Final validation

- `python3 tests/test_restructure.py`: six checks pass, including token-equivalence checks for extracted algorithms, material/fishing controllers and the reconstructed node tick, unchanged protocol codec, disabled future services, and dependency/composition guards.
- Lua syntax checks pass for both standalone addon Lua files; addon repository is unchanged.
- `git diff --check`: passes.
- `/stuff/Source/azerothcore-wotlk/acore.sh compiler build`: passes, including linked `worldserver` and installation. The first final build exposed a template registration visibility issue; the corrected implementation keeps template instantiation inside Integration and exports a non-template registration function. Build log: `/tmp/restructure-final-build.log` (local, not versioned).
- No in-game test run was performed. Runtime behavior preservation still requires the matrix above.

### Remaining Plan 3 work

The source decomposition is implemented, but the full plan's exit criteria are not yet all met. Existing boolean mount decisions and registry access from adapters were retained for compatibility; typed mount-result APIs and further narrowing of adapter/state boundaries remain follow-up refactors. The complete live beta matrix and representative trace capture also remain outstanding. Future skeletons are extension locations, not implementations of their planned algorithms.
