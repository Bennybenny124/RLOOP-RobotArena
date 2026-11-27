# Core simulation v2 — Issues #3–#7

## Architecture and scope

`CoreArena` owns the authoritative tick, config, two `CoreRobot` instances, mutable obstacles and match outcome. CoreRobot owns individual position, type, HP, cooldown and recovery state. `CoreKinematics` resolves shared movement; `CoreCombat` evaluates attacks and exact results. These files are standard C++17, with no UE, Python, rendering or input dependencies. CMake and Unreal compile the same Core sources.

`Controller` in CoreController.h returns an ActionV2 from a const WorldStateV2 snapshot and player index. `stepControllers` obtains both actions from the same snapshot and calls CoreArena once. `ARobot` is the UE keyboard adapter; it does not advance its own simulation or duplicate gameplay rules. `AArena` owns the local match and presents Core geometry/events. UE actor transforms and physics are not the simulation authority.

Issue #8 replay format, Observation, pybind11 and DQN remain outside this slice.

## Configuration

All lengths are cm, velocities cm/s, world XY centered at (0,0); UE floor Z=0. All role values are **provisional fixtures**, not final balance.

| Type | HP | Damage/shot | Cooldown (ms) | Range (cm) | Speed (cm/s) | Regen (HP/s) | Square half-extent (cm) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Tank | 200 | 20 | 500 | 450 | 300 | 5 | 25 |
| Scout | 120 | 35 | 1800 | 300 | 420 | 15 | 25 |
| Ranged | 90 | 15 | 800 | 750 | 280 | 3 | 25 |

`ArenaConfig.map` (MapConfig) defaults: width 1300, height 1100, spawns (-390,0) tank and (390,0) scout, empty obstacles. ArenaConfig match defaults: 60 Hz, maxTicks 3600, pushSpeedMultiplier 0.5, boxSpeedCap 150 cm/s, outOfCombatDelayMs 3000. Tank preserves the earlier 300 cm/s fixture. Same-role debug/test matches are permitted; the six distinct-role training combinations are unchanged.

`CoreRobotConfig.h` defines the standalone RobotTypeConfig and defaultRobotTypes() table. `ArenaConfig.robotTypes` is indexed by Tank/Scout/Ranged; each `MapConfig.robots` entry selects a role and spawn. `MapConfig.obstacles` holds the layout (array length is the count). Reset copies both layout and role settings; live objects do not mutate the map or another arena instance. C++ callers may replace any field. HP quantities use integer micro-HP (see combat). Validation returns InvalidConfig and `lastError()` identifies the invalid field/category; rejected reset does not mutate the world. Default-constructed RobotTypeConfig has invalid/missing required fields; default ArenaConfig deliberately supplies all three fixtures. No external config parser is added. Arena dimensions and speeds bounded at 1e7, tick rate 1–1000, obstacle count ≤256, positive dimensions/footprints/range/cooldown/HP/damage, non-negative movement/regen, valid enum, unique obstacle IDs, all footprints inside arena and no initial interior overlap beyond contact tolerance. Changing tickRateHz does not change maxTicks automatically.

## Versioned interfaces

- `ActionV2{value}`: integer 0–17, move_id*2+attack, with move IDs 0=stop, 1=N(+Y), 2=NE, 3=E(+X), 4=SE, 5=S, 6=SW, 7=W, 8=NW. Diagonals use 1/sqrt(2). `encodeAction(x,y,attack)` maps sign axes to IDs; `decodeAction` rejects invalid values without writing the output.
- `ActionsV2`: two actions; `CoreArena::step(ActionsV2)` is the only public simulation step and validates both before updating. The legacy vector action schema and overload are removed. `DecodedAction`/`DecodedActions` are internal, unversioned movement intents used by the decoder and kinematics solver; tests may call the solver directly.
- `WorldStateV2`: value snapshot of seed/tick/rate/limit/bounds, two RobotStateV2 (position/type/hp/maxHp/halfExtent/cooldownTicks/quietTicks/regenRemainder), live obstacle list and match result.
- `EventV2`: Moved, ObstacleMoved (stable obstacleId), MovementLimited, Attack (player/resultCode/actualDamage/from/to), Healed (healed), MatchEnded. noPlayer=2. Attack codes are None/OnCooldown/OutOfRange/Blocked/Reduced/Hit; no ray on cooldown. Events ordered robot movement, obstacle movement in config order, movement limit, attacks by player, heals by player, terminal result. Display order is not gameplay priority.
- `StepResultV2`: step status, complete post-step state, events. The v1 world/event/step layouts are superseded; the obstacle layout is unchanged. Schema IDs are compile-time; serialized replay compatibility checking belongs to #8.
- Before reset, step returns NotInitialized. Invalid actions and terminal steps return unchanged snapshot and no events. Seed is stored; fixed spawn layouts use no RNG yet.

```cpp
rloop::CoreArena arena;
rloop::ArenaConfig config;
if (arena.reset(42, config) == rloop::ResetStatus::Ready) {
    rloop::ActionsV2 actions{{{6}, {15}}}; // P0 east, P1 west+attack
    auto tick = arena.step(actions);
}
```

## Fixed tick phases and kinematics

1. Validate both actions, take the pre-tick snapshot.
2. Build simultaneous robot velocities; resolve all movement and box positions for one dt. Commit both robot positions.
3. Decrement prior cooldown timers, evaluate both attacks against the shared post-movement snapshot.
4. Apply firing cooldown and both damage/recovery transitions, then increment tick and decide result.
5. Return the snapshot/events. No per-player simulation tick and no wall-clock input to Core.

**Provisional collision contract (reviewable, not a final geometry decision):**

- Robots are axis-aligned squares, boxes are axis-aligned rectangles. The scene limits whole footprints, not centers. No rotation/inertia/bounce. Contacts use 1e-7 cm tolerance; initial overlaps larger than this on both axes are rejected. Corner-only grazing without relative inward velocity on both boundary axes is not penetration.
- Swept relative AABB/arena boundary time selects the earliest contact over the remaining tick, avoiding tunneling. Direct forward robot contact seeds that robot's full post-slowdown vector on the touched box; the slowdown applies once regardless of count. At each interval, synchronous compression propagation transfers source-axis bits only across touching box pairs with closing relative velocity, a transmitting box moving into the contact, and a source pointing into the contact. Only contact-normal components transfer; no tangential drag or tensile force. A box receives each robot/axis at most once even via multiple paths, sums the received vectors, then norm-clamps to boxSpeedCap. Propagation starts from scratch at each impact/release/tick, so contact never creates a persistent joint. A box without a source has zero velocity. The monotone source propagation is bounded, and the final velocity constraints prevent penetration if newly combined drives conflict.
- Robots and pushed boxes slide along arena walls and fixed cover: discard only inward normal components, retain the original tangent without renormalizing. Project robot intent before detecting pushes; norm-clamp each box's sum before static projection. A blocked downstream box constrains only the closing relative normal of its neighbors, never their independent tangents. At a corner with two inward components, both stop. Dynamic robot/robot, robot/box and box/box contacts use synchronous component-wise relative limits: opposing inward components stop; in same-direction travel only the faster follower is limited. These limits never add velocity or passively move a robot. Recompute at swept impacts and face-contact release, with no player or obstacle ID priority. Source paths discovered during an interval remain until the next geometry event/tick (conservative discrete contract); finite precision allows contact tolerance, not exact fixed-point physics.
- If constraint/event iteration cannot resolve safely, freeze the unprocessed interval and emit MovementLimited; never advance through an unresolved contact. This conservative fallback and the remaining dynamic contact rules are candidates for review.
- The original speed cap is independently configurable; it never increases with the number of pushers. Example (3,4)+(4,3) with cap 5 gives (5/sqrt(2),5/sqrt(2)). Equal opposite pushes cancel.

## Combat numerical contract

- Hitscan targets opponent center from the post-movement state. Range boundary is inclusive. Out-of-range consumes cooldown and emits a ray clipped to range. Closed segment/AABB intersection includes tangency; fixed cover takes priority and truncates the ray at the nearest fixed intersection. Each movable cover counts once, and damage is divided by 4 for each cover. No collision or combat geometry is recomputed in renderer.
- HP, damage and regen rate are integer units of 1e-6 HP. Each cover division rounds down in micro-HP (equivalent to flooring the final positive attenuated damage). actualDamage is capped by target remaining HP. A fully quantized zero-damage hit is not received damage. HP/max HP are bounded by 1e9 units (1000 HP) so cross-products are ≤1e18 and fit in signed 64-bit integers.
- A successful shot sets cooldown to ceil(cooldownMs*Hz/1000), even if missed/blocked. Prior cooldown ticks decrement before evaluating attacks; on-cooldown requests do not fire or reset quiet time.
- Received positive damage or successful firing resets quietTicks and fractional healing remainder, and prevents healing in that tick. Otherwise quiet time advances by one tick. Recovery credits only the portion of the tick strictly after the configured delay, using integer time scaled by Hz; HP/s fractions accumulate in a remainder with denominator 1000*Hz. Full HP discards remainder. Death precludes healing.
- Both attacks are decided before either robot loses HP, allowing double KO. Death takes precedence over timeout. At maxTicks, compare hp0*maxHp1 with hp1*maxHp0 exactly, no tolerance. Equal products are a draw; larger ratio wins.

Same build/compiler/platform/floating environment and same input/config/seed reproduce state and events. Headless compilation disables fast-math and FP contraction (/fp:strict on MSVC). No cross-build bitwise guarantee; cross-platform replay integration belongs to #8.

## UE5 local debug PvP

Compile UE5.6 project, place one Arena actor in a level, start PIE and focus the game viewport. Enable Arena Debug > Draw Debug Arena, Local PvP and Use Debug Camera. The orthographic debug camera looks down at the world origin. The camera is screen-right +X, screen-up -Y; keyboard maps to screen directions (W/I = -Y). The discrete world-space N remains +Y. Turn off Use Debug Camera to use a custom camera; keys still use the same world mapping.

| Player | Move | Attack |
| --- | --- | --- |
| 0 | W A S D | Space |
| 1 | I J K L | Right Ctrl |
| Both | R (edge-triggered restart) | Hold to repeatedly request fire |

Opposite direction keys cancel; diagonal movement is normalized in Core. One UE player controller supplies both logical keyboard controllers, no split-screen or physical ARobot pawn required. AArena accumulates frame time and invokes 60 fixed ticks/s by default; frame gaps above 0.25 s are capped to avoid excessive catch-up, so wall-clock match duration may stretch under stalls. Core never receives frame delta. At terminal it stops stepping; R resets the same config. Selected map and ability settings are converted to Core at BeginPlay, and R resets that same captured configuration. Stop/restart PIE to apply editor edits. The default white fixed #1 at (-150,180) half extent (80,50) and orange movable #2 at (150,-180) half extent (60,60) are provisional.

DrawDebug: white arena boundary wire box spanning Z=0–200 cm (2 m high, half extent Z=100 cm), white fixed obstacle footprints/wire boxes, orange movable obstacles, blue/red player footprints, gray attack-range wire cylinders (RGB 192,192,192; radius from the captured role config; Z=0–50 cm, equal to robot debug height; visual only, not line of sight), HP bar, HP/cooldown/quiet time, latest attack result; ray for 0.18s (yellow blocked, red reduced, gray out-of-range, player color hit), match result/time remaining. Boundary height is fixed at 200 cm and is visual only; DebugShapeHeightCm controls obstacle visualization separately. Draws absolute world coordinates, independent of actor transform. Debug-only, not production meshes or UMG.

### Editor configuration and reusable instances

The UE adapter is intentionally separate from the pure C++ Core:

- `RobotAbilityConfig.h/.cpp` defines `FRobotAbilityConfig` (ordinary HP, damage, cooldown ms, range/speed/regen/half extent), `FRobotAbilitySet` (Tank, Scout, Ranged), and `URobotAbilityData` (Data Asset). Defaults come from the C++ table in CoreRobotConfig.h. Values entered in HP are checked and rounded to nearest micro-HP before reset; the Core still validates the configuration.
- `ArenaMapConfig.h/.cpp` defines `FArenaMapConfig`, two explicit `FArenaRobotSpawn` entries and an editable array of `FArenaMapObstacle`. `UArenaMapData` is a reusable Data Asset containing one map layout. Width/height, player types/positions, and each obstacle's ID, movability, center and half extent belong to the map. Obstacle count follows the array size.
- In the Content Browser, use Add > Miscellaneous > Data Asset and select ArenaMapData (one asset per map, e.g. DA_Map_Open or DA_Map_Cover). Edit Config, then assign it to the Arena actor's Arena Map > Map Asset. For a quick one-off layout, leave Map Asset empty and edit the inline Map Config instead.
- Create RobotAbilityData similarly (e.g. DA_RobotAbilities), expand Abilities > Tank/Scout/Ranged, edit values, and assign Robot Abilities > Robot Ability Asset on any Arena. If no asset is assigned, the actor's inline Robot Abilities values apply. A selected asset takes precedence over inline fields. This allows different maps to share the same balance asset.
- No binary .uasset instances are committed; create them in the UE editor. Existing per-actor overrides of the removed DebugObstacles/Player0Type/Player1Type properties are not automatically migrated: copy any customized values into the new Map Config or asset. The C++ default layout and roles are preserved.
- Headless callers pass `ArenaConfig.map` and `ArenaConfig.robotTypes` directly. UE assets are not directly readable by the headless target; exporting edited assets to a portable training config is future work.

### Manual verification (UE5 editor required)

- [ ] Create two ArenaMapData assets with different dimensions, obstacles and spawns, select each in turn; verify the world and camera follow the selected layout and restarting restores the original obstacle positions.
- [ ] Create one RobotAbilityData asset, change Tank HP and movement speed, share it across both maps, and verify values in PIE. Verify inline fallback with both asset pointers empty, and invalid settings produce a useful log instead of starting a match.

- [ ] Compile module and open PIE with one Arena; confirm overhead view, origin, boundaries ±650 and ±550 with a 200 cm tall wire box, both obstacles and robot positions.
- [ ] Move both players simultaneously, diagonally, into walls/each other. Confirm no penetration and no per-robot ticks. At the right boundary hold W+D: the robot must slide upward at speed/sqrt(2); at a corner with both components blocked it stops. Repeat along all fixed-cover faces, then push a box or touching chain diagonally against a wall/fixed obstacle: directly pushed boxes retain permitted tangents without dragging side neighbors; downstream boxes receive only compression normals; no penetration or rotation. For robot/robot collisions, verify diagonal face sliding, opposite tangents sliding past each other, same-direction following without pushing, and cardinal head-on stopping followed by separation.
- [ ] Push orange box, release to stop, push a chain (add a second adjacent movable box in editor), oppose pushes from both sides, and push into wall/fixed cover. After two boxes move together, push only one along the shared face and verify the other stays still and the boxes separate; check diagonal normal-only transmission and independent side pushes.
- [ ] Align robots with white fixed box between them; fire and confirm blocked ray, consumed cooldown, zero damage.
- [ ] Confirm both range cylinders follow their robots, have the same height as robot debug boxes, and reflect edited role attack ranges; gray matches the out-of-range ray. Range is center-to-center, not a cover/visibility indicator.
- [ ] Align with one/two movable boxes; verify reduced damage (1/4, 1/16) and ray. Out-of-range consumes cooldown; holding fire on cooldown shows no new ray.
- [ ] Take damage, disengage, verify quiet time and healing; fire while out-of-range to reset recovery, press while on cooldown without resetting.
- [ ] Finish a 60-second match; compare HP ratios, verify draw at equal full HP and no stepping after terminal; R restarts. Double KO is tested headlessly (manual symmetric same-role low-HP fixture can be configured in C++).
- [ ] Toggle Draw Debug Arena and restart with an empty MapConfig.Obstacles array; verify open-arena match. Verify no duplicate simulation from ARobot.

Run the four CTest suites using the root README. Headless tests cover the action codec, validation, lifecycle, movement/chain/cap/symmetry, combat/regen/outcomes and controller loop. UE compilation and PIE checks are independent of headless CI.
