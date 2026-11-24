// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CoreController.h"
#include "Robot.generated.h"

class APlayerController;

UCLASS()
class RLOOP_ROBOTARENA_API ARobot : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ARobot();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // UE keyboard adapter only; Core owns robot state and gameplay rules.
    static rloop::ActionV2 ReadLocalAction(const APlayerController& Controller, std::size_t Player);

};

// Two logical controllers share a local keyboard and one UE player controller.
class FLocalRobotController final : public rloop::Controller
{
public:
    explicit FLocalRobotController(const APlayerController& InController) : Input(InController) {}
    rloop::ActionV2 action(const rloop::WorldStateV2&, std::size_t Player) override
    {
        return ARobot::ReadLocalAction(Input, Player);
    }
private:
    const APlayerController& Input;
};
