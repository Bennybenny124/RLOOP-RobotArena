// Unreal presentation only. All authoritative layout and gameplay state live
// in CoreArena/CoreRobot, shared with headless C++ and future pybind11 bindings.

#include "Arena.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Robot.h"

namespace {
const TCHAR* AttackName(rloop::AttackResult Code)
{
    switch (Code) {
        case rloop::AttackResult::OnCooldown: return TEXT("on_cooldown");
        case rloop::AttackResult::OutOfRange: return TEXT("out_of_range");
        case rloop::AttackResult::Blocked: return TEXT("blocked");
        case rloop::AttackResult::Reduced: return TEXT("reduced");
        case rloop::AttackResult::Hit: return TEXT("hit");
        default: return TEXT("-");
    }
}
const TCHAR* ResultName(rloop::MatchResult Result)
{
    switch (Result) {
        case rloop::MatchResult::Player0Win: return TEXT("Player 0 wins");
        case rloop::MatchResult::Player1Win: return TEXT("Player 1 wins");
        case rloop::MatchResult::Draw: return TEXT("Draw");
        default: return TEXT("Running");
    }
}
const TCHAR* RoleName(rloop::RobotType Type)
{
    switch (Type) {
        case rloop::RobotType::Tank: return TEXT("Tank");
        case rloop::RobotType::Scout: return TEXT("Scout");
        case rloop::RobotType::Ranged: return TEXT("Ranged");
        default: return TEXT("?");
    }
}
}

AArena::AArena()
{
    PrimaryActorTick.bCanEverTick = true;
    DebugRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DebugRoot"));
    SetRootComponent(DebugRoot);
    DebugCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DebugCamera"));
    DebugCamera->SetupAttachment(DebugRoot);
}

void AArena::BeginPlay()
{
    Super::BeginPlay();

    MatchConfig = rloop::ArenaConfig{};
    FString AbilityError;
    const FRobotAbilitySet& Abilities = RobotAbilityAsset ? RobotAbilityAsset->Abilities : RobotAbilities;
    if (!Abilities.ToCore(MatchConfig.robotTypes, AbilityError))
    {
        UE_LOG(LogTemp, Error, TEXT("Robot abilities: %s"), *AbilityError);
        return;
    }
    FString MapError;
    const FArenaMapConfig& Layout = MapAsset ? MapAsset->Config : MapConfig;
    if (!Layout.ToCore(MatchConfig.map, MapError))
    {
        UE_LOG(LogTemp, Error, TEXT("Arena map: %s"), *MapError);
        return;
    }
    RestartMatch();
    if (bUseDebugCamera && bConfigReady)
    {
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            PC->SetViewTarget(this);
            PC->SetInputMode(FInputModeGameOnly{});
        }
    }
}

void AArena::RestartMatch()
{
    bConfigReady = Simulation.reset(0, MatchConfig) == rloop::ResetStatus::Ready;
    if (!bConfigReady)
    {
        UE_LOG(LogTemp, Error, TEXT("Arena config: %s"), UTF8_TO_TCHAR(Simulation.lastError().c_str()));
    }
    Accumulator = 0;
    LastAttack = {};
    LastRay = {};
    RaySeconds = {};
}

void AArena::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    for (double& Remaining : RaySeconds)
        Remaining = FMath::Max(0.0, Remaining - static_cast<double>(DeltaTime));

    if (bLocalPvP && bConfigReady)
    {
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            const bool bRestart = PC->IsInputKeyDown(EKeys::R);
            if (bRestart && !bRestartHeld) RestartMatch();
            bRestartHeld = bRestart;

            if (!Simulation.isTerminal())
            {
                // Accumulate wall time only in this adapter. Every Core step
                // consumes exactly one configured tick; cap catch-up after stalls.
                Accumulator += FMath::Clamp(static_cast<double>(DeltaTime), 0.0, 0.25);
                const double Dt = 1.0 / MatchConfig.tickRateHz;
                FLocalRobotController P0(*PC), P1(*PC);
                while (Accumulator >= Dt && !Simulation.isTerminal())
                {
                    const auto Step = rloop::stepControllers(Simulation, P0, P1);
                    if (Step.status != rloop::StepStatus::Advanced) { Accumulator = 0; break; }
                    Accumulator -= Dt;
                    for (const auto& Event : Step.events)
                    {
                        if (Event.kind != rloop::EventKind::Attack || Event.player >= 2) continue;
                        LastAttack[Event.player] = Event;
                        if (Event.resultCode != rloop::AttackResult::OnCooldown)
                        {
                            LastRay[Event.player] = Event;
                            RaySeconds[Event.player] = 0.18;
                        }
                    }
                }
            }
            else Accumulator = 0;
        }
    }
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
    // Visual-only arena boundary: floor Z=0, top Z=200 cm.
    // DrawDebugBox takes half extents, so its center is at Z=100 cm.
    constexpr double BoundaryHeightCm = 200.0;
    DrawDebugBox(World, FVector(0.0, 0.0, BoundaryHeightCm / 2.0),
                 FVector(HalfWidth, HalfHeight, BoundaryHeightCm / 2.0),
                 FColor::White, false, 0.0f, 0, 3.0f);
    DrawDebugCoordinateSystem(World, FVector::ZeroVector, FRotator::ZeroRotator,
                              100.0f, false, 0.0f, 0, 2.0f);
    DrawDebugString(World, FVector(0.0, 0.0, 25.0), TEXT("Origin (0, 0, 0)"),
                    nullptr, FColor::White, 0.0f);

    const double Height = FMath::Max(1.0, DebugShapeHeightCm);
    for (const auto& Obstacle : State.obstacles)
    {
        const bool bMovable = Obstacle.type == rloop::ObstacleType::Movable;
        const FColor Color = bMovable ? FColor::Orange : FColor::White;
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
    }

    for (std::size_t Index = 0; Index < State.robots.size(); ++Index)
    {
        const auto& Robot = State.robots[Index];
        const FColor Color = Index == 0 ? FColor::Blue : FColor::Red;
        const FVector Center(Robot.position.x, Robot.position.y, 25.0);
        DrawDebugBox(World, Center, FVector(Robot.halfExtent, Robot.halfExtent, 25.0),
                     Color, false, 0.0f, 0, 3.0f);
        const FVector BarStart = Center + FVector(-Robot.halfExtent, -Robot.halfExtent-10, 35);
        const FVector BarEnd = BarStart + FVector(2*Robot.halfExtent,0,0);
        DrawDebugLine(World, BarStart, BarEnd, FColor::Red, false, 0.0f, 0, 6.0f);
        const double HpRatio = static_cast<double>(Robot.hp)/Robot.maxHp;
        DrawDebugLine(World, BarStart, BarStart + (BarEnd-BarStart)*HpRatio, FColor::Green,
                      false, 0.0f, 0, 6.0f);
        DrawDebugString(World, Center + FVector(0,0,60),
            FString::Printf(TEXT("P%d %s | HP %.2f/%.2f | CD %.2fs | quiet %.2fs"),
                static_cast<int32>(Index), RoleName(Robot.type),
                static_cast<double>(Robot.hp)/rloop::hpScale, static_cast<double>(Robot.maxHp)/rloop::hpScale,
                static_cast<double>(Robot.cooldownTicks)/State.tickRateHz,
                static_cast<double>(Robot.quietTicks)/State.tickRateHz),
            nullptr, Color, 0.0f);
        DrawDebugString(World, Center + FVector(0,50,60),
            FString::Printf(TEXT("%s | damage %.2f"), AttackName(LastAttack[Index].resultCode),
                static_cast<double>(LastAttack[Index].actualDamage)/rloop::hpScale),
            nullptr, Color, 0.0f);
        if (RaySeconds[Index] > 0)
        {
            const auto& Ray = LastRay[Index];
            const FColor RayColor = Ray.resultCode == rloop::AttackResult::Blocked ? FColor::Yellow:
                Ray.resultCode == rloop::AttackResult::Reduced ? FColor::Red :
                Ray.resultCode == rloop::AttackResult::OutOfRange ? FColor(192,192,192) : Color;
            DrawDebugLine(World, FVector(Ray.from.x,Ray.from.y,60),
                FVector(Ray.to.x,Ray.to.y,60), RayColor, false, 0.0f, 0, 5.0f);
        }
    }
    DrawDebugString(World, FVector(-HalfWidth,-HalfHeight-65,80),
        FString::Printf(TEXT("%s | %.2fs remaining | R: restart"), ResultName(State.result),
            static_cast<double>(State.maxTicks-State.tick)/State.tickRateHz),
        nullptr, FColor::White, 0.0f);
    DrawDebugString(World, FVector(-HalfWidth,HalfHeight+45,80),
        TEXT("P0 WASD + Space | P1 IJKL + Right Ctrl"), nullptr, FColor::White, 0.0f);
}
