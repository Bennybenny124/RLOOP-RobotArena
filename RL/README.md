# Folder for RL Training

Future pybind11 bindings will expose the authoritative C++ Core in `Source/RLOOP_RobotArena/`.
Use CoreArena::step(ActionsV2) (two discrete values, each 0–17), WorldStateV2, EventV2 and StepResultV2 as described in [Core simulation v2](../docs/core-simulation-v2.md).
Headless callers configure ArenaConfig.map and ArenaConfig.robotTypes; default role stats live in CoreRobotConfig.h. Unreal Data Assets are editor-side only, and export/import of their edited values for training is not yet implemented.
The same Core handles local PvP, collision and combat. Observation, bindings, replay serialization and training remain separate work; no Python game-rule implementation.
