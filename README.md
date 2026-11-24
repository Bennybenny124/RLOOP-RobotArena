# RLOOP — Robot Arena

Two-robot arena with PvP, PvC, CvC; headless C++ gameplay core, Python DQN training, and Unreal Engine 5 presentation.

## Documentation

- [Gameplay and technical specification (v1.1)](docs/spec.md)
- [Core simulation v1: API, numerical contract and debug draw](docs/core-simulation-v1.md)
- [Contributing / Git conventions](CONTRIBUTING.md)
- [Historical specification v1.0](docs/archive/RLOOP_spec_v1.0.md) (superseded by v1.1; kept for reference only)

## Development stage

The headless Core now supports fixed-step, deterministic two-player movement and a minimal timeout-only open-arena match. The existing UE5 project has a temporary arena debug visualization. Robot action decoding, full collision/pushing, combat, input and replay remain subsequent issues. Fixture values are configurable and are not final character balance.

## Headless build and test

Requires CMake 3.16+ and a C++17 compiler; no Unreal Engine, Python or third-party C++ libraries required. From the repository root:

```sh
cmake -S . -B build-headless -DCMAKE_BUILD_TYPE=Release
cmake --build build-headless --config Release
ctest --test-dir build-headless -C Release --output-on-failure
```

Both the CMake target and the UE5 module compile the same CoreArena/CoreRobot source files in `Source/RLOOP_RobotArena/`. To view temporary debug shapes in UE5 5.6, place an Arena actor in a level, enable Arena Debug > Draw Debug Arena and start Play in Editor. See the Core simulation guide for the visual legend and limits.
