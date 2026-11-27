# Core simulation v1 — Issue #3 (historical)

> Historical implementation snapshot before #4–#7. The current API, geometry, combat and viewer are documented in [Core simulation v2](core-simulation-v2.md). Do not use the v1 world/event types for new consumers.

## Ownership and source layout

- `Source/RLOOP_RobotArena/CoreArena.*`: authoritative two-player simulation, layout, tick, reset, step and events. Holds two `CoreRobot` objects.
- `CoreRobot.*`: one robot's configuration and position; proposes movement without mutating the world.
- `CoreTypes.h`: standard C++ versioned value types; no UE dependencies.
- `Arena.*` (`AArena : AActor`): temporary visualization and editor fixture input; resets Core in BeginPlay, reads its snapshots in Tick. Does not advance simulation from frame delta.
- `Robot.*` (`ARobot : APawn`): unchanged UE input/presentation adapter. No authoritative state or per-robot simulation tick.
- `RL/`: future pybind11 consumers of the same Core, not a separate game implementation.

CMake builds the same two Core .cpp files as the Unreal module; C++17, no third-party dependencies. See the root README for build/CTest commands.

## Configuration and coordinates

All lengths are cm; speed is cm/s. Core coordinates are XY; UE maps directly to world XY with Z=0 as the floor, independent of the Arena actor transform. The rectangular bounds are [-width/2, width/2] × [-height/2, height/2]. Obstacles store a unique uint32 ID, kind (fixed/movable), center, and positive half-extents. Their XY min/max are center minus/plus half-extent. No collision or movement of obstacles is implemented in this slice.

Provisional defaults in `ArenaConfig` (not gameplay balance):

| Field | Value |
| --- | --- |
| width / height | 1300 / 1100 cm |
| tickRateHz | 60 |
| maxTicks | 3600 (60 seconds) |
| spawn positions | (-390, 0), (390, 0) cm |
| moveSpeed | 300 cm/s per robot |
| obstacles | empty |

Callers can override all fields. Changing tick rate does not automatically change maxTicks; set both when configuring duration. Configuration validation rejects non-finite/non-positive dimensions, zero tick rate/limit, invalid spawn/speed, invalid obstacle type/size/bounds or duplicate IDs. Speed zero is allowed. Obstacle overlap and robot radius are not defined here (#5).

## API and lifecycle

```cpp
#include "CoreArena.h"

rloop::CoreArena arena;
rloop::ArenaConfig config;
if (arena.reset(42, config) != rloop::ResetStatus::Ready) {
    // Invalid config; no state change.
    return;
}
rloop::ActionsV1 actions{};
actions[0].movement = {1.0, 0.0};
actions[1].movement = {-1.0, 0.0};
const auto result = arena.step(actions);
```

- `reset(seed, config)` copies valid config, initializes both robots, stores seed, sets tick 0 and Running. Rejected reset preserves previous state. The seed is recorded but no randomness is used yet: fixed spawns are intentional.
- `ActionV1` (schemaVersion=1): finite 2D movement vector of length ≤1, plus reserved attack bool (no effect yet). The 18-action discrete decoder and diagonal normalization belong to #4.
- `ActionsV1`: array of exactly two ActionV1 values, indexed by player 0 and 1.
- `WorldStateV1` (schemaVersion=1): value snapshot of seed, tick, tick rate, limit, bounds, robot positions, obstacle states and match result. Before reset the result is NotStarted.
- `StepResultV1` (schemaVersion=1): status, post-step snapshot, and ordered EventV1 values. Non-advanced steps return unchanged snapshot and empty events. Invalid actions are rejected together, without partial movement or tick advance. Step before reset returns NotInitialized; after termination returns Terminal.
- `EventV1` (schemaVersion=1): Moved events carry player index and old/new coordinates; only actual displacement emits Moved. MatchEnded carries match result and noPlayer=2. Events are emitted in player-index order, then MatchEnded; ordering is representational, not simulation priority. All events belong to the returned snapshot tick.
- Schema versions are compile-time identifiers; binary serialization and version compatibility checks are reserved for #8.

## Tick order and determinism

1. Validate both actions before mutation.
2. Compute both proposed positions from the pre-tick state using dt=1/tickRateHz; clamp each robot center to the arena rectangle.
3. Commit both positions together. Future collision (#5) and post-movement simultaneous attack/damage (#6) extend these phases; no robot or obstacle collision yet.
4. Emit movement events, increment integer tick once; at maxTicks emit MatchEnded and Draw (temporary timeout-only outcome, no HP changes).
5. Return snapshot and events; further steps do not advance terminal matches.

At default parameters, cardinal movement is 5 cm per tick. Floating positions and velocities use double. Repeatability is scoped to identical build/compiler/platform, floating-point environment, config, seed and action sequence. Headless CMake disables fast-math and FP contraction (MSVC uses /fp:strict). No wall-clock input, random draws, unordered iteration, or asynchronous state mutation. Cross-toolchain bitwise equality is not guaranteed. UE build flags may differ from headless; cross-build equivalence is not claimed. Unit tests compare same-build states/events tick by tick and swap player config/actions to test symmetry.

## UE5 debug visualization

Place an AArena actor in a level and run Play in Editor. The Arena Debug category exposes Draw Debug Arena, Debug Shape Height Cm and Debug Obstacles. Optional obstacle fixtures are copied into the Core config at BeginPlay; changing editor fixture values requires restarting play. Clear the array for an open arena. Default debug-only fixtures: fixed #1 at (-150,180), half-extent (80,50); movable #2 at (150,-180), half-extent (60,60). Neither is a final map.

- Yellow: arena ground-plane bounds, with min/max coordinate labels.
- Red: fixed obstacle wire box and XY footprint.
- Blue: movable obstacle wire box and XY footprint.
- Green / purple: player 0 / 1 spawn markers.
- XYZ axes and origin label: world center (0,0,0).
- Obstacle labels display ID and min/max X/Y in cm. Height (default 100 cm) is rendering-only.

Draws per frame while enabled, with zero lifetime and no persistent lines. Actor position/rotation does not offset the world-space coordinates. Invalid config logs an error and prevents drawing. No UE gameplay loop, physical walls, floor mesh, obstacle actors or collision are added by the debug helpers.

Manual UE5 verification: start PIE, confirm origin and arena min (-650,-550), max (650,550), both default obstacle footprints and labels, spawn positions, then toggle Draw Debug Arena off and restart with an empty Debug Obstacles array. Core tests do not verify UE rendering; editor compilation and manual checks are required.

## Scope remaining

#4: robot archetypes, HP/cooldown attributes and discrete action decoding; #5: kinematic collision and box pushing; #6: full combat, regen, exact HP-ratio timeout outcome; #7: gameplay input and debug viewer; #8: replay schema and integrated regression.
