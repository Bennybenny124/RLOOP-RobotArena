#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CoreRobotConfig.h"
#include "RobotAbilityConfig.generated.h"

UENUM(BlueprintType)
enum class ERloopRobotType : uint8 { Tank, Scout, Ranged };

USTRUCT(BlueprintType)
struct FRobotAbilityConfig
{
    GENERATED_BODY()

    // Editor values are ordinary HP, not internal micro-HP units.
    UPROPERTY(EditAnywhere, Category = "Abilities", meta = (ClampMin = "0.000001", ClampMax = "1000"))
    double MaxHP = 1.0;

    UPROPERTY(EditAnywhere, Category = "Abilities", meta = (ClampMin = "0.000001", ClampMax = "1000"))
    double AttackDamage = 1.0;

    UPROPERTY(EditAnywhere, Category = "Abilities", meta = (ClampMin = "1"))
    int32 CooldownMs = 500;

    UPROPERTY(EditAnywhere, Category = "Abilities", meta = (ClampMin = "0.000001"))
    double AttackRangeCm = 450.0;

    UPROPERTY(EditAnywhere, Category = "Abilities", meta = (ClampMin = "0"))
    double MoveSpeedCmPerSecond = 300.0;

    UPROPERTY(EditAnywhere, Category = "Abilities", meta = (ClampMin = "0", ClampMax = "1000"))
    double RegenHPPerSecond = 0.0;

    UPROPERTY(EditAnywhere, Category = "Abilities", meta = (ClampMin = "0.000001"))
    double HalfExtentCm = 25.0;

    static FRobotAbilityConfig FromCore(const rloop::RobotTypeConfig& Value);
    bool ToCore(rloop::RobotTypeConfig& Out, FString& Error) const;
};

USTRUCT(BlueprintType)
struct FRobotAbilitySet
{
    GENERATED_BODY()

    FRobotAbilitySet();

    UPROPERTY(EditAnywhere, Category = "Robot Abilities")
    FRobotAbilityConfig Tank;

    UPROPERTY(EditAnywhere, Category = "Robot Abilities")
    FRobotAbilityConfig Scout;

    UPROPERTY(EditAnywhere, Category = "Robot Abilities")
    FRobotAbilityConfig Ranged;

    bool ToCore(std::array<rloop::RobotTypeConfig,3>& Out, FString& Error) const;
};

// Create a Data Asset instance to share one balance set across arena maps.
UCLASS(BlueprintType)
class RLOOP_ROBOTARENA_API URobotAbilityData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Robot Abilities")
    FRobotAbilitySet Abilities;
};
