// Unreal presentation adapter. CoreArena is the only simulation authority.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoreArena.h"
#include "Arena.generated.h"

class UCameraComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class ERloopRobotType : uint8 { Tank, Scout, Ranged };

USTRUCT(BlueprintType)
struct FArenaDebugObstacle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Arena Debug")
    int32 Id = 0;

    UPROPERTY(EditAnywhere, Category = "Arena Debug")
    bool bMovable = false;

    UPROPERTY(EditAnywhere, Category = "Arena Debug")
    FVector2D Center = FVector2D::ZeroVector;

    // Half-width and half-height, in centimeters.
    UPROPERTY(EditAnywhere, Category = "Arena Debug")
    FVector2D HalfExtent = FVector2D(50.0, 50.0);
};

UCLASS()
class RLOOP_ROBOTARENA_API AArena : public AActor
{
    GENERATED_BODY()

public:
    AArena();
    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

private:
    // Temporary debug presentation. World XY plane at Z=0; ignore actor transform.
    UPROPERTY(EditAnywhere, Category = "Arena Debug")
    bool bDrawDebugArena = true;

    UPROPERTY(EditAnywhere, Category = "Arena Debug", meta = (ClampMin = "1.0"))
    double DebugShapeHeightCm = 100.0;

    // Optional editable fixtures; copied into Core at BeginPlay.
    UPROPERTY(EditAnywhere, Category = "Arena Debug")
    TArray<FArenaDebugObstacle> DebugObstacles;

    UPROPERTY(EditAnywhere, Category = "Local PvP")
    bool bLocalPvP = true;

    UPROPERTY(EditAnywhere, Category = "Local PvP")
    bool bUseDebugCamera = true;

    UPROPERTY(EditAnywhere, Category = "Local PvP")
    ERloopRobotType Player0Type = ERloopRobotType::Tank;

    UPROPERTY(EditAnywhere, Category = "Local PvP")
    ERloopRobotType Player1Type = ERloopRobotType::Scout;

    UPROPERTY(VisibleAnywhere, Category = "Local PvP")
    TObjectPtr<USceneComponent> DebugRoot;

    UPROPERTY(VisibleAnywhere, Category = "Local PvP")
    TObjectPtr<UCameraComponent> DebugCamera;

    rloop::ArenaConfig MatchConfig;
    rloop::CoreArena Simulation;
    double Accumulator = 0;
    bool bRestartHeld = false;
    bool bConfigReady = false;
    std::array<rloop::EventV2, 2> LastAttack{};
    std::array<rloop::EventV2, 2> LastRay{};
    std::array<double, 2> RaySeconds{};
    void RestartMatch();
    void DrawDebugArena() const;
};
