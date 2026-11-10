#if WITH_DEV_AUTOMATION_TESTS
#include "Gimmick/DirectionalStairs.h"
#include "Misc/AutomationTest.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionalStairsPassageTest, "TheLastMessage.Gimmick.DirectionalStairs.Passage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDirectionalStairsPassageTest::RunTest(const FString& Parameters)
{
	using S = ADirectionalStairs;
	TestTrue(TEXT("Walk from upper-end underside"), S::ShouldAllowWalkThrough({-350,0,90},{200,0,0},true,false,false));
	TestTrue(TEXT("Walk right under the middle after landing below"), S::ShouldAllowWalkThrough({-125,0,-80},{200,0,0},true,false,false));
	TestTrue(TEXT("Walk left into a neighboring underside"), S::ShouldAllowWalkThrough({50,0,-80},{-200,0,0},true,false,false));
	TestTrue(TEXT("Stopped contact underneath can resume movement"), S::ShouldAllowWalkThrough({-125,0,-80},{0,0,0},true,false,false));
	TestFalse(TEXT("Uphill approach stays blocked"), S::ShouldAllowWalkThrough({100,0,90},{-200,0,0},true,false,false));
	TestFalse(TEXT("Jump cannot start passage"), S::ShouldAllowWalkThrough({-350,0,90},{200,0,0},false,false,false));
	TestTrue(TEXT("Rising jump can pass from below"), S::ShouldAllowWalkThrough({-150,0,20},{0,0,400},true,false,false));
	TestFalse(TEXT("Falling from above stays blocked"), S::ShouldAllowWalkThrough({-150,0,240},{0,0,-400},true,false,false));
	TestFalse(TEXT("Grapple revokes an existing exception"), S::ShouldAllowWalkThrough({-100,0,90},{200,0,0},true,true,true));
	TestTrue(TEXT("Walking passage stays latched below low steps"), S::ShouldAllowWalkThrough({70,0,90},{200,0,0},true,false,true));
	TestFalse(TEXT("Clearing low end restores collision"), S::ShouldAllowWalkThrough({210,0,90},{200,0,0},true,false,true));
	TestFalse(TEXT("Walking on top never passes through"), S::ShouldAllowWalkThrough({-350,0,400},{200,0,0},true,false,false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionalStairsCollisionTest, "TheLastMessage.Gimmick.DirectionalStairs.WeaponCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDirectionalStairsCollisionTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	ADirectionalStairs* Stairs = World->SpawnActor<ADirectionalStairs>();
	ACharacter* Walker = World->SpawnActor<ACharacter>(FVector(-500,0,90), FRotator::ZeroRotator);
	Walker->GetCapsuleComponent()->IgnoreActorWhenMoving(Stairs, true);
	for (const auto Channel : {ECC_Visibility, ECC_GameTraceChannel4, ECC_GameTraceChannel5})
	{
		for (const bool bComplex : {false, true})
		{
			FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(DirectionalStairsTest), bComplex, Walker);
			TestTrue(TEXT("Weapons still hit stairs while walker ignores them"), World->LineTraceSingleByChannel(
				Hit, FVector(-125,-500,93), FVector(-125,500,93), Channel, Query));
			TestEqual(TEXT("Permanent ramp is the blocking component"), Hit.GetComponent(), static_cast<UPrimitiveComponent*>(Stairs->SolidRamp));
		}
	}
	// Surface rises towards local -X, matching the original stair asset.
	FHitResult High, Low;
	World->LineTraceSingleByChannel(High, FVector(-250,0,400), FVector(-250,0,-100), ECC_Visibility);
	World->LineTraceSingleByChannel(Low, FVector(0,0,400), FVector(0,0,-100), ECC_Visibility);
	TestTrue(TEXT("Upper end is local negative X"), High.bBlockingHit && Low.bBlockingHit && High.ImpactPoint.Z > Low.ImpactPoint.Z);
	Walker->SetActorLocation(FVector(-125,0,220));
	Walker->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	FHitResult FloorHit(Stairs, Stairs->SolidRamp, FVector(-125,0,100), FVector::UpVector);
	Walker->GetCharacterMovement()->CurrentFloor.SetFromSweep(FloorHit, 0, true);
	TestTrue(TEXT("Drop input accepts the supporting stair"), Stairs->RequestDropThrough(Walker));
	TestTrue(TEXT("Drop starts falling"), Walker->GetCharacterMovement()->IsFalling());
	Stairs->Tick(0.016f);
	TestTrue(TEXT("Drop ignores only stair movement"), Walker->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Stairs));
	Walker->SetActorLocation(FVector(-125,0,-200));
	Stairs->Tick(0.016f);
	TestFalse(TEXT("Drop collision restored after capsule clears"), Walker->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Stairs));
	ADirectionalStairs* Neighbor = World->SpawnActor<ADirectionalStairs>(FVector(-350,0,200), FRotator::ZeroRotator);
	Walker->SetActorLocation(FVector(-290,0,290));
	Walker->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Walker->GetCharacterMovement()->CurrentFloor.SetFromSweep(FloorHit, 0, true);
	TestTrue(TEXT("Drop at a modular seam is accepted"), Stairs->RequestDropThrough(Walker));
	TestTrue(TEXT("Adjacent ramp is ignored at the seam"), Walker->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Neighbor));
	// The floor can be less than one capsule height below the ramp. Landing
	// there must not re-enable the ramp while the character still intersects it.
	AActor* LowerFloor = World->SpawnActor<AActor>();
	Walker->SetActorLocation(FVector(-290,0,90));
	Walker->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	FHitResult LowerHit(LowerFloor, nullptr, FVector(-290,0,2), FVector::UpVector);
	Walker->GetCharacterMovement()->CurrentFloor.SetFromSweep(LowerHit, 0, true);
	Stairs->Tick(0.016f);
	TestTrue(TEXT("Landing below does not trap an overlapping capsule"), Walker->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Stairs));
	Walker->SetActorLocation(FVector(1000,0,90));
	Stairs->Tick(0.016f);
	Neighbor->Tick(0.016f);
	TestFalse(TEXT("Moving clear restores the supporting ramp"), Walker->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Stairs));
	TestFalse(TEXT("Moving clear restores the adjacent ramp"), Walker->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Neighbor));
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
