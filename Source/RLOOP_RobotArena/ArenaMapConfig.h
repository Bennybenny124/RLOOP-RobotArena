#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CoreTypes.h"
#include "RobotAbilityConfig.h"
#include "ArenaMapConfig.generated.h"

USTRUCT(BlueprintType)
struct FArenaMapObstacle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Obstacle", meta = (ClampMin = "0"))
    int32 Id = 0;

    UPROPERTY(EditAnywhere, Category = "Obstacle")
    bool bMovable = false;

    UPROPERTY(EditAnywhere, Category = "Obstacle")
    FVector2D Center = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Obstacle")
    FVector2D HalfExtent = FVector2D(50.0, 50.0);
};

USTRUCT(BlueprintType)
struct FArenaRobotSpawn
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Spawn")
    ERloopRobotType RobotType = ERloopRobotType::Tank;

    UPROPERTY(EditAnywhere, Category = "Spawn")
    FVector2D Position = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct FArenaMapConfig
{
    GENERATED_BODY()

    FArenaMapConfig();

    UPROPERTY(EditAnywhere, Category = "Map", meta = (ClampMin = "1"))
    double WidthCm = 1300.0;

    UPROPERTY(EditAnywhere, Category = "Map", meta = (ClampMin = "1"))
    double HeightCm = 1100.0;

    // Exactly two players; one struct for each spawn.
    UPROPERTY(EditAnywhere, Category = "Map")
    FArenaRobotSpawn Player0;

    UPROPERTY(EditAnywhere, Category = "Map")
    FArenaRobotSpawn Player1;

    // Array length determines the number of obstacles; no redundant count.
    UPROPERTY(EditAnywhere, Category = "Map")
    TArray<FArenaMapObstacle> Obstacles;

    bool ToCore(rloop::MapConfig& Out, FString& Error) const;
};

// One asset instance per map; many arenas can reference the same layout.
UCLASS(BlueprintType)
class RLOOP_ROBOTARENA_API UArenaMapData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Map")
    FArenaMapConfig Config;
};
