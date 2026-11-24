// This is the class that either handles player input when controlled by a player,
// receives model output when controlled by agents, or reads data when replaying a recorded gameplay.
// This class will access CoreRobot.cpp for any robot related core logic.
// All unreal dependencies should be placed here, while core logics are in CoreRobot.h,
// which will be compiled with pybind11 for python RL training purpose.

#include "Robot.h"

// Sets default values
ARobot::ARobot()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ARobot::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARobot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ARobot::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

