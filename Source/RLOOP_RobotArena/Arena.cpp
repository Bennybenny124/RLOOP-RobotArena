// Unreal presentation only. All authoritative layout and gameplay state live
// in CoreArena/CoreRobot, shared with headless C++ and future pybind11 bindings.

#include "Arena.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

AArena::AArena()
{
    PrimaryActorTick.bCanEverTick = true;

    // Provisional fixtures for verifying debug shapes, not a gameplay map.
    FArenaDebugObstacle Fixed;
    Fixed.Id = 1;
    Fixed.Center = FVector2D(-150.0, 180.0);
    Fixed.HalfExtent = FVector2D(80.0, 50.0);
    DebugObstacles.Add(Fixed);

    FArenaDebugObstacle Movable;
    Movable.Id = 2;
    Movable.bMovable = true;
    Movable.Center = FVector2D(150.0, -180.0);
    Movable.HalfExtent = FVector2D(60.0, 60.0);
    DebugObstacles.Add(Movable);
}

void AArena::BeginPlay()
{
    Super::BeginPlay();

    rloop::ArenaConfig Config;
    for (const FArenaDebugObstacle& Obstacle : DebugObstacles)
    {
        if (Obstacle.Id < 0)
        {
            UE_LOG(LogTemp, Error, TEXT("Arena debug obstacle IDs must be non-negative."));
            return;
        }
        Config.obstacles.push_back({
            static_cast<std::uint32_t>(Obstacle.Id),
            Obstacle.bMovable ? rloop::ObstacleType::Movable : rloop::ObstacleType::Fixed,
            {Obstacle.Center.X, Obstacle.Center.Y},
            {Obstacle.HalfExtent.X, Obstacle.HalfExtent.Y}
        });
    }
    if (Simulation.reset(0, Config) != rloop::ResetStatus::Ready)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid arena debug configuration; check dimensions, IDs and bounds."));
    }
}

void AArena::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Never step the simulation from variable frame time. Input collection and
    // the fixed-step UE match loop are outside Issue #3.
    if (bDrawDebugArena)
    {
        DrawDebugArena();
    }
}

void AArena::DrawDebugArena() const
{
    UWorld* World = GetWorld();
    if (!World) return;
    const rloop::WorldStateV2 State = Simulation.state();
    if (State.result == rloop::MatchResult::NotStarted) return;

    const double HalfWidth = State.width / 2.0;
    const double HalfHeight = State.height / 2.0;
    const FVector Corners[] = {
        FVector(-HalfWidth, -HalfHeight, 0.0),
        FVector( HalfWidth, -HalfHeight, 0.0),
        FVector( HalfWidth,  HalfHeight, 0.0),
        FVector(-HalfWidth,  HalfHeight, 0.0)
    };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        DrawDebugLine(World, Corners[Index], Corners[(Index + 1) % 4],
                      FColor::Yellow, false, 0.0f, 0, 3.0f);
    }
    DrawDebugCoordinateSystem(World, FVector::ZeroVector, FRotator::ZeroRotator,
                              100.0f, false, 0.0f, 0, 2.0f);
    DrawDebugString(World, FVector(0.0, 0.0, 25.0), TEXT("Origin (0, 0, 0)"),
                    nullptr, FColor::White, 0.0f);
    DrawDebugString(World, FVector(-HalfWidth, -HalfHeight, 25.0),
                    FString::Printf(TEXT("Arena min (%.1f, %.1f) cm"), -HalfWidth, -HalfHeight),
                    nullptr, FColor::Yellow, 0.0f);
    DrawDebugString(World, FVector(HalfWidth, HalfHeight, 25.0),
                    FString::Printf(TEXT("Arena max (%.1f, %.1f) cm"), HalfWidth, HalfHeight),
                    nullptr, FColor::Yellow, 0.0f);

    const double Height = FMath::Max(1.0, DebugShapeHeightCm);
    for (const auto& Obstacle : State.obstacles)
    {
        const bool bMovable = Obstacle.type == rloop::ObstacleType::Movable;
        const FColor Color = bMovable ? FColor::Blue : FColor::Red;
        const FVector Center(Obstacle.center.x, Obstacle.center.y, Height / 2.0);
        const FVector Extent(Obstacle.halfExtent.x, Obstacle.halfExtent.y, Height / 2.0);
        DrawDebugBox(World, Center, Extent, Color, false, 0.0f, 0, 3.0f);
        // Explicit ground-plane footprint, independent of visual height.
        const FVector Min(Center.X - Extent.X, Center.Y - Extent.Y, 0.0);
        const FVector Max(Center.X + Extent.X, Center.Y + Extent.Y, 0.0);
        const FVector Footprint[] = {Min, FVector(Max.X, Min.Y, 0.0),
                                    Max, FVector(Min.X, Max.Y, 0.0)};
        for (int32 Index = 0; Index < 4; ++Index)
        {
            DrawDebugLine(World, Footprint[Index], Footprint[(Index + 1) % 4],
                          Color, false, 0.0f, 0, 3.0f);
        }
        DrawDebugString(World, FVector(Center.X, Center.Y, Height + 20.0),
                        FString::Printf(TEXT("%s #%u | X [%.1f, %.1f] Y [%.1f, %.1f] cm"),
                            bMovable ? TEXT("Movable") : TEXT("Fixed"),
                            static_cast<unsigned int>(Obstacle.id), Min.X, Max.X, Min.Y, Max.Y),
                        nullptr, FColor::White, 0.0f);
    }

    for (std::size_t Index = 0; Index < State.robots.size(); ++Index)
    {
        const auto Position = State.robots[Index].position;
        const FColor Color = Index == 0 ? FColor::Green : FColor(180, 80, 230);
        const FVector Center(Position.x, Position.y, 0.0);
        DrawDebugSphere(World, Center, 20.0f, 12, Color, false, 0.0f, 0, 2.0f);
        DrawDebugString(World, Center + FVector(0.0, 0.0, 35.0),
                        FString::Printf(TEXT("Player %d (%.1f, %.1f)"), static_cast<int32>(Index),
                                        Position.x, Position.y),
                        nullptr, Color, 0.0f);
    }
}
