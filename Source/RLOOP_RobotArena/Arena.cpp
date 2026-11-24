// This is the class that accesses CoreArena.h, generates walls & obstacles, and place them into the world.
// All unreal dependencies should be placed here, while core logics are in CoreArena.h,
// which will be compiled with pybind11 for python RL training purpose.


#include "Arena.h"

// Sets default values
AArena::AArena()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AArena::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AArena::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

