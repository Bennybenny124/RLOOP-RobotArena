#include "ArenaMapConfig.h"
#include <utility>

FArenaMapConfig::FArenaMapConfig()
{
    const rloop::MapConfig Defaults;
    WidthCm = Defaults.width;
    HeightCm = Defaults.height;
    Player0.RobotType = static_cast<ERloopRobotType>(Defaults.robots[0].type);
    Player0.Position = FVector2D(Defaults.robots[0].spawn.x, Defaults.robots[0].spawn.y);
    Player1.RobotType = static_cast<ERloopRobotType>(Defaults.robots[1].type);
    Player1.Position = FVector2D(Defaults.robots[1].spawn.x, Defaults.robots[1].spawn.y);

    // Preserve the existing UE debug map as the default editable fixture.
    FArenaMapObstacle Fixed;
    Fixed.Id = 1;
    Fixed.Center = FVector2D(-150.0, 180.0);
    Fixed.HalfExtent = FVector2D(80.0, 50.0);
    Obstacles.Add(Fixed);
    FArenaMapObstacle Movable;
    Movable.Id = 2;
    Movable.bMovable = true;
    Movable.Center = FVector2D(150.0, -180.0);
    Movable.HalfExtent = FVector2D(60.0, 60.0);
    Obstacles.Add(Movable);
}

bool FArenaMapConfig::ToCore(rloop::MapConfig& Out, FString& Error) const
{
    rloop::MapConfig Converted;
    Converted.width = WidthCm;
    Converted.height = HeightCm;
    const FArenaRobotSpawn* Spawns[] = {&Player0, &Player1};
    for (std::size_t Index=0; Index<2; ++Index)
    {
        Converted.robots[Index] = {{Spawns[Index]->Position.X, Spawns[Index]->Position.Y},
                                    static_cast<rloop::RobotType>(Spawns[Index]->RobotType)};
    }
    if (Obstacles.Num() > 256)
    {
        Error = TEXT("At most 256 obstacles are supported");
        return false;
    }
    Converted.obstacles.reserve(Obstacles.Num());
    for (const FArenaMapObstacle& Obstacle : Obstacles)
    {
        if (Obstacle.Id < 0)
        {
            Error = TEXT("Obstacle IDs must be non-negative");
            return false;
        }
        Converted.obstacles.push_back({
            static_cast<std::uint32_t>(Obstacle.Id),
            Obstacle.bMovable ? rloop::ObstacleType::Movable : rloop::ObstacleType::Fixed,
            {Obstacle.Center.X, Obstacle.Center.Y},
            {Obstacle.HalfExtent.X, Obstacle.HalfExtent.Y}
        });
    }
    // Core validates geometry, IDs, role selections and spawn intersections.
    Out = std::move(Converted);
    return true;
}
