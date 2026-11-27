// Unreal presentation adapter. CoreArena is the only simulation authority.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoreArena.h"
#include "RobotAbilityConfig.h"
#include "ArenaMapConfig.h"
#include "Arena.generated.h"

class UCameraComponent;
class USceneComponent;

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

    // Asset takes precedence; inline MapConfig is convenient for one-off maps.
    UPROPERTY(EditAnywhere, Category = "Arena Map")
    TObjectPtr<UArenaMapData> MapAsset;

    UPROPERTY(EditAnywhere, Category = "Arena Map")
    FArenaMapConfig MapConfig;

    UPROPERTY(EditAnywhere, Category = "Local PvP")
    bool bLocalPvP = true;

    UPROPERTY(EditAnywhere, Category = "Local PvP")
    bool bUseDebugCamera = true;

    UPROPERTY(VisibleAnywhere, Category = "Local PvP")
    TObjectPtr<USceneComponent> DebugRoot;

    UPROPERTY(VisibleAnywhere, Category = "Local PvP")
    TObjectPtr<UCameraComponent> DebugCamera;

    // Optional shared role asset; inline values apply when no asset is assigned.
    UPROPERTY(EditAnywhere, Category = "Robot Abilities")
    TObjectPtr<URobotAbilityData> RobotAbilityAsset;

    UPROPERTY(EditAnywhere, Category = "Robot Abilities")
    FRobotAbilitySet RobotAbilities;

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
