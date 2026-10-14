#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Character/Player/SideScrollingCharacter.h"
#include "Gimmick/SideScrollingSoftPlatform.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"

namespace SideScrollingPlatformTests
{
struct FTestWorld
{
	UWorld* World;
	ASideScrollingCharacter* Character = nullptr;

	FTestWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		World->GetWorldSettings()->NotifyBeginPlay();
	}

	~FTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}

	UBoxComponent* AddFloor(float Height, ECollisionChannel Channel)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->AddInstanceComponent(Box);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(300, 100, 20));
		Box->SetWorldLocation(FVector(0, 0, Height));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(Channel);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		return Box;
	}

	bool SpawnPlayer(const FVector& Location)
	{
		UClass* Class = LoadClass<ASideScrollingCharacter>(nullptr, TEXT("/Game/Test_Jihong/BP_TestPlayer.BP_TestPlayer_C"));
		if (!Class) return false;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Character = World->SpawnActor<ASideScrollingCharacter>(Class, Location, FRotator::ZeroRotator, Params);
		if (!Character) return false;
		Character->GetMesh()->SetComponentTickEnabled(false);
		Character->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		return true;
	}

	double FeetZ() const
	{
		return Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}

	void Step(int32 Frames)
	{
		// A synchronous automation test stays in one engine frame. Tick movement
		// explicitly instead of relying on the world's once-per-frame tick scheduler.
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World->Tick(LEVELTICK_TimeOnly, 1.0f / 60.0f);
			Character->RefreshSoftPlatformCollision();
			Character->GetCharacterMovement()->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		}
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSolidFloorDropTest, "TheLastMessage.SideScrolling.SolidFloorDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSolidFloorDropTest::RunTest(const FString& Parameters)
{
	using namespace SideScrollingPlatformTests;
	FTestWorld Fixture;
	Fixture.AddFloor(0, ECC_GameTraceChannel1);
	UBoxComponent* Solid = Fixture.AddFloor(300, ECC_WorldStatic);
	if (!TestTrue(TEXT("Actual player BP loads"), Fixture.SpawnPlayer(FVector(0, 0, 450)))) return false;
	Fixture.Step(30);
	TestTrue(TEXT("Player settles on solid floor"), Fixture.Character->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent() == Solid);
	Fixture.Character->DoDrop(1);
	Fixture.Character->DoJumpStart();
	Fixture.Step(60);
	TestTrue(TEXT("Down+jump cannot pass solid floor even with soft floor below"), Fixture.FeetZ() >= 319);
	TestEqual(TEXT("WorldStatic remains blocking"), Fixture.Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Block);
	TestTrue(TEXT("Solid floor remains the movement base"), Fixture.Character->GetMovementBaseObject() == Solid);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOneWayPlatformPassageTest, "TheLastMessage.SideScrolling.OneWayPlatformPassage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOneWayPlatformPassageTest::RunTest(const FString& Parameters)
{
	using namespace SideScrollingPlatformTests;
	FTestWorld Fixture;
	Fixture.AddFloor(0, ECC_WorldStatic);
	UBoxComponent* Lower = Fixture.AddFloor(150, ECC_GameTraceChannel1);
	UBoxComponent* Upper = Fixture.AddFloor(500, ECC_GameTraceChannel1);
	if (!TestTrue(TEXT("Actual player BP loads"), Fixture.SpawnPlayer(FVector(0, 0, 350)))) return false;
	Fixture.Character->GetCharacterMovement()->Velocity.Z = 1400;
	double MaximumFeet = Fixture.FeetZ();
	for (int32 Frame = 0; Frame < 150; ++Frame)
	{
		Fixture.Step(1);
		MaximumFeet = FMath::Max(MaximumFeet, Fixture.FeetZ());
	}
	TestTrue(TEXT("Jump passes through underside"), MaximumFeet > 530);
	TestTrue(TEXT("Descending player lands on upper platform"), Fixture.Character->GetMovementBaseObject() == Upper);
	Fixture.Character->DoDrop(1);
	Fixture.Character->DoJumpStart();
	Fixture.Step(90);
	TestTrue(TEXT("Down+jump passes current platform and lands on lower platform"), Fixture.Character->GetMovementBaseObject() == Lower);
	TestTrue(TEXT("Lower platform catches player"), FMath::IsNearlyEqual(Fixture.FeetZ(), 172.15, 5.0));
	TestFalse(TEXT("Upper platform collision restored after passage"), Fixture.Character->GetCapsuleComponent()->CopyArrayOfMoveIgnoreComponents().Contains(Upper));
	TestEqual(TEXT("Soft channel remains blocking; passage is per component"), Fixture.Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Block);
	TestEqual(TEXT("Native platform uses dedicated soft channel"), GetDefault<ASideScrollingSoftPlatform>()->FindComponentByClass<UStaticMeshComponent>()->GetCollisionObjectType(), ECC_GameTraceChannel1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShortPlatformJumpTest, "TheLastMessage.SideScrolling.ShortPlatformJump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShortPlatformJumpTest::RunTest(const FString& Parameters)
{
	using namespace SideScrollingPlatformTests;
	FTestWorld Fixture;
	UBoxComponent* Ground = Fixture.AddFloor(150, ECC_WorldStatic);
	Fixture.AddFloor(500, ECC_GameTraceChannel1);
	if (!TestTrue(TEXT("Actual player BP loads"), Fixture.SpawnPlayer(FVector(0, 0, 350)))) return false;
	Fixture.Character->GetCharacterMovement()->Velocity.Z = 600;
	Fixture.Step(120);
	TestTrue(TEXT("A jump ending inside the platform falls back without getting trapped"), Fixture.Character->GetMovementBaseObject() == Ground);
	return !HasAnyErrors();
}
#endif
