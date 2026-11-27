// This is the class that either handles player input when controlled by a player,
// receives model output when controlled by agents, or reads data when replaying a recorded gameplay.
// This class will access CoreRobot.cpp for any robot related core logic.
// All unreal dependencies should be placed here, while core logics are in CoreRobot.h,
// which will be compiled with pybind11 for python RL training purpose.

#include "Robot.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

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


rloop::ActionV2 ARobot::ReadLocalAction(const APlayerController& Controller, std::size_t Player)
{
    // The debug camera looks down with screen-right +X and screen-up -Y.
    // Opposite keys cancel. Holding fire repeatedly requests attack (no masking).
    const bool bFirst = Player == 0;
    const int X = int(Controller.IsInputKeyDown(bFirst ? EKeys::D : EKeys::L))
                - int(Controller.IsInputKeyDown(bFirst ? EKeys::A : EKeys::J));
    const int Y = int(Controller.IsInputKeyDown(bFirst ? EKeys::S : EKeys::K))
                - int(Controller.IsInputKeyDown(bFirst ? EKeys::W : EKeys::I));
    const bool bAttack = Controller.IsInputKeyDown(bFirst ? EKeys::SpaceBar : EKeys::RightControl);
    return rloop::encodeAction(X, Y, bAttack);
}
