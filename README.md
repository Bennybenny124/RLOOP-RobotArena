# RLOOP — Robot Arena

Two-robot arena with PvP, PvC, CvC; headless C++ gameplay core, Python DQN training, and Unreal Engine 5 presentation.

## Documentation

- [Gameplay and technical specification (v1.1)](docs/spec.md)
- [Core simulation v2: API, provisional geometry, combat and local PvP](docs/core-simulation-v2.md)
- [Contributing / Git conventions](CONTRIBUTING.md)
- [Historical specification v1.0](docs/archive/RLOOP_spec_v1.0.md)

## Development stage

The C++ Core provides fixed-step two-player simulation, three configurable robot archetypes, 18 discrete actions, kinematic box pushing/collision, simultaneous combat, cooldown/regen and exact HP-ratio timeout results. UE5 supplies a temporary local two-player debug viewer and keyboard adapter. Fixture values and collision details are provisional, not final gameplay balance. Replay serialization/regression integration (#8), observation, Python bindings and training are separate work.

## Headless build and test

Requires CMake 3.16+ and a C++17 compiler; no Unreal Engine, Python or third-party C++ libraries required.

```sh
cmake -S . -B build-headless -DCMAKE_BUILD_TYPE=Release
cmake --build build-headless --config Release
ctest --test-dir build-headless -C Release --output-on-failure
```

CTest: world_simulation, kinematics, combat, controllers. CMake and UE5 compile the same Core sources in `Source/RLOOP_RobotArena/`.

## Editor configuration (UE5.6)

Under Arena Map, edit the inline Map Config or select an ArenaMapData asset (one instance per map). Under Robot Abilities, edit Tank/Scout/Ranged inline or select a shared RobotAbilityData asset. Assigned assets take precedence. Restart PIE to load edits. See the [configuration guide](docs/core-simulation-v2.md#editor-configuration-and-reusable-instances) for creating assets and migrating older actor overrides.

## Local PvP debug (UE5.6)

Compile the project, place one Arena actor in a level, enable Local PvP, Use Debug Camera and Draw Debug Arena, start Play in Editor and focus the viewport. Player 0: WASD + Space; Player 1: IJKL + Right Ctrl; R restarts. The top-down viewer displays robot HP/cooldowns, live obstacles, laser events and match result. See the [debug guide and manual checklist](docs/core-simulation-v2.md#ue5-local-debug-pvp). The viewer is temporary debug presentation, not production UE assets.
