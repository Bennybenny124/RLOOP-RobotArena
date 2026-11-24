# Folder for RL Training

Future pybind11 bindings will expose the authoritative C++ Core in `Source/RLOOP_RobotArena/`.
Use ActionV2 (18 discrete values), WorldStateV2, EventV2 and StepResultV2 as described in [Core simulation v2](../docs/core-simulation-v2.md).
The same Core handles local PvP, collision and combat. Observation, bindings, replay serialization and training remain separate work; no Python game-rule implementation.
