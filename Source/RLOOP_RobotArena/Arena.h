// Unreal presentation adapter. CoreArena is the only simulation authority.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoreArena.h"
#include "Arena.generated.h"

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
    // Temporary visualization only. World XY plane at Z=0; ignore actor transform.
    UPROPERTY(EditAnywhere, Category = "Arena Debug")
    bool bDrawDebugArena = true;

    UPROPERTY(EditAnywhere, Category = "Arena Debug", meta = (ClampMin = "1.0"))
    double DebugShapeHeightCm = 100.0;

    // Optional visualization-only fixtures; these are copied into Core on reset.
    UPROPERTY(EditAnywhere, Category = "Arena Debug")
    TArray<FArenaDebugObstacle> DebugObstacles;

    rloop::CoreArena Simulation;
    void DrawDebugArena() const;
};
