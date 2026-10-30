#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Camera/SideScrollingCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdaptiveCameraTest, "TheLastMessage.Camera.AdaptiveFraming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAdaptiveCameraTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	ACharacter* Pawn = World->SpawnActor<ACharacter>(FVector(0, 0, 200), FRotator::ZeroRotator);
	ASideScrollingCameraManager* Camera = World->SpawnActor<ASideScrollingCameraManager>();
	FTViewTarget View;
	View.Target = Pawn;
	Camera->UpdateViewTarget(View, 0.016f);
	const float InitialZ = View.POV.Location.Z;
	TestTrue(TEXT("Normal gravity shows more space above the player"), InitialZ > Pawn->GetActorLocation().Z + 100);
	for (int32 Index = 0; Index < 240; ++Index) Camera->UpdateViewTarget(View, 0.016f);
	TestTrue(TEXT("Framing offset persists after initialization"), FMath::IsNearlyEqual(View.POV.Location.Z, InitialZ, 0.01f));
	Pawn->SetActorLocation(Pawn->GetActorLocation() + FVector(0, 0, 30));
	Camera->UpdateViewTarget(View, 0.016f);
	TestTrue(TEXT("Small airborne motion stays inside dead zone"), FMath::IsNearlyEqual(View.POV.Location.Z, InitialZ, 0.01f));
	Pawn->GetCharacterMovement()->SetGravityDirection(FVector(0, 0, 1));
	Camera->UpdateViewTarget(View, 0.016f);
	TestTrue(TEXT("Gravity reversal blends rather than snapping"), FMath::Abs(View.POV.Location.Z - InitialZ) < 10.0f);
	for (int32 Index = 0; Index < 500; ++Index) Camera->UpdateViewTarget(View, 0.016f);
	TestTrue(TEXT("Inverted gravity shows more space below the player"), View.POV.Location.Z < Pawn->GetActorLocation().Z - 100);
	Pawn->SetActorLocation(FVector(0, 0, 10000));
	Camera->UpdateViewTarget(View, 0.016f);
	TestTrue(TEXT("Teleport resets follow history"), View.POV.Location.Z > 9000);
	World->DestroyWorld(false);
	return true;
}
#endif
