#include "RobotAbilityConfig.h"
#include <cmath>

FRobotAbilityConfig FRobotAbilityConfig::FromCore(const rloop::RobotTypeConfig& Value)
{
    FRobotAbilityConfig Result;
    Result.MaxHP = static_cast<double>(Value.maxHp) / rloop::hpScale;
    Result.AttackDamage = static_cast<double>(Value.attackDamage) / rloop::hpScale;
    Result.CooldownMs = static_cast<int32>(Value.cooldownMs);
    Result.AttackRangeCm = Value.attackRange;
    Result.MoveSpeedCmPerSecond = Value.moveSpeed;
    Result.RegenHPPerSecond = static_cast<double>(Value.regenPerSecond) / rloop::hpScale;
    Result.HalfExtentCm = Value.halfExtent;
    return Result;
}

bool FRobotAbilityConfig::ToCore(rloop::RobotTypeConfig& Out, FString& Error) const
{
    const double Max = static_cast<double>(rloop::maxHpUnits) / rloop::hpScale;
    const auto ValidHP = [Max](double Value) { return std::isfinite(Value) && Value >= 0.0 && Value <= Max; };
    // Check before conversion to avoid invalid floating-point-to-integer casts.
    if (!ValidHP(MaxHP) || !ValidHP(AttackDamage) || !ValidHP(RegenHPPerSecond) ||
        MaxHP < 1.0 / rloop::hpScale || AttackDamage < 1.0 / rloop::hpScale || CooldownMs <= 0 ||
        !std::isfinite(AttackRangeCm) || AttackRangeCm <= 0 ||
        !std::isfinite(MoveSpeedCmPerSecond) || MoveSpeedCmPerSecond < 0 || MoveSpeedCmPerSecond > 1e7 ||
        !std::isfinite(HalfExtentCm) || HalfExtentCm <= 0)
    {
        Error = TEXT("Invalid HP, damage, cooldown, range, speed, regen or footprint");
        return false;
    }
    Out = {std::llround(MaxHP*rloop::hpScale), std::llround(AttackDamage*rloop::hpScale),
           static_cast<std::uint32_t>(CooldownMs), AttackRangeCm, MoveSpeedCmPerSecond,
           std::llround(RegenHPPerSecond*rloop::hpScale), HalfExtentCm};
    return true;
}

FRobotAbilitySet::FRobotAbilitySet()
{
    const auto Defaults = rloop::defaultRobotTypes();
    Tank = FRobotAbilityConfig::FromCore(Defaults[0]);
    Scout = FRobotAbilityConfig::FromCore(Defaults[1]);
    Ranged = FRobotAbilityConfig::FromCore(Defaults[2]);
}

bool FRobotAbilitySet::ToCore(std::array<rloop::RobotTypeConfig,3>& Out, FString& Error) const
{
    const FRobotAbilityConfig* Values[] = {&Tank, &Scout, &Ranged};
    const TCHAR* Names[] = {TEXT("Tank"), TEXT("Scout"), TEXT("Ranged")};
    std::array<rloop::RobotTypeConfig,3> Converted{};
    for (std::size_t Index=0; Index<Converted.size(); ++Index)
    {
        if (!Values[Index]->ToCore(Converted[Index], Error))
        {
            Error = FString(Names[Index]) + TEXT(": ") + Error;
            return false;
        }
    }
    Out = Converted;
    return true;
}
