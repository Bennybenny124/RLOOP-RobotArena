cmake_minimum_required(VERSION 3.16)
project(RLOOPHeadless LANGUAGES CXX)

add_library(rloop_core STATIC
    Source/RLOOP_RobotArena/CoreArena.cpp
    Source/RLOOP_RobotArena/CoreRobot.cpp
    Source/RLOOP_RobotArena/CoreKinematics.cpp)
target_include_directories(rloop_core PUBLIC Source/RLOOP_RobotArena)
target_compile_features(rloop_core PUBLIC cxx_std_17)
set_target_properties(rloop_core PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(rloop_core PRIVATE /W4 /fp:strict)
else()
    target_compile_options(rloop_core PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_world_simulation tests/test_world_simulation.cpp)
    target_link_libraries(test_world_simulation PRIVATE rloop_core)
    add_test(NAME world_simulation COMMAND test_world_simulation)
    add_executable(test_kinematics tests/test_kinematics.cpp)
    target_link_libraries(test_kinematics PRIVATE rloop_core)
    add_test(NAME kinematics COMMAND test_kinematics)
endif()
