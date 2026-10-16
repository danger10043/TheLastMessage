#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/GrapplingHookComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace GrapplingProbeTest
{
struct FWorld
{
	UWorld* World;
	FWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		World->GetWorldSettings()->NotifyBeginPlay();
	}
	~FWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	UBoxComponent* MakeSurface(float X)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Actor->AddInstanceComponent(Box);
		Box->SetBoxExtent(FVector(25, 50, 50));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Actor->SetActorLocation(FVector(X, 0, 0));
		return Box;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingProbeTest, "TheLastMessage.Grappling.ProbeAndFixedAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingProbeTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.World->SpawnActor<APlayerBase>();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	if (!TestNotNull(TEXT("Player owns grappling component"), Grapple)) return false;
	Grapple->bDrawDebug = false;
	UBoxComponent* Surface = Fixture.MakeSurface(400);
	Player->UpdateAimFromWorldRay(FVector(800,500,0), FVector(0,-1,0));
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Untagged surface blocks"), Grapple->ProbeState.Result == EGrapplingProbeResult::Blocked);
	TestFalse(TEXT("Untagged surface does not attach"), Grapple->HasGrappleAnchor());
	Surface->GetOwner()->Tags.Add(TEXT("Grappleable"));
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Actor tag permits attachment"), Grapple->HasGrappleAnchor());
	TestTrue(TEXT("Anchor is the impact point, not target center"), Grapple->ProbeState.AnchorPoint.Equals(FVector(375,0,0), 0.1f));
	const FVector Anchor = Grapple->ProbeState.AnchorPoint;
	Player->UpdateAimFromWorldRay(FVector(-800,500,0), FVector(0,-1,0));
	Player->SetActorLocation(FVector(-100,0,0));
	TestTrue(TEXT("Changing aim and moving player preserve fixed anchor"), Grapple->ProbeState.AnchorPoint.Equals(Anchor));
	Player->SetActorLocation(FVector::ZeroVector);
	Player->UpdateAimFromWorldRay(FVector(800,500,0), FVector(0,-1,0));
	Surface->GetOwner()->Tags.Reset();
	Surface->ComponentTags.Add(TEXT("Grappleable"));
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Component tag permits attachment"), Grapple->HasGrappleAnchor());
	UBoxComponent* Blocker = Fixture.MakeSurface(200);
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Foreground wall blocks tagged surface behind it"), Grapple->ProbeState.Result == EGrapplingProbeResult::Blocked);
	TestFalse(TEXT("Failed new attempt clears previous anchor"), Grapple->HasGrappleAnchor());
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Grapple->MaxGrappleDistance = 100;
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Range limit enforced"), Grapple->ProbeState.Result == EGrapplingProbeResult::Miss);
	Grapple->MaxGrappleDistance = 1500;
	Player->UpdateAimFromWorldRay(FVector(0,500,800), FVector(0,-1,0));
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Empty-space attempt misses"), Grapple->ProbeState.Result == EGrapplingProbeResult::Miss);
	Player->UpdateAimFromWorldRay(FVector(800,500,0), FVector(0,-1,0));
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Next attempt can reconnect"), Grapple->HasGrappleAnchor());
	TestTrue(TEXT("Probe never moves character"), Player->GetActorLocation().IsNearlyZero());
	Player->HandleDeath();
	TestTrue(TEXT("Death clears probe state"), Grapple->ProbeState.Result == EGrapplingProbeResult::None);
	Player->RequestGrapplingHook();
	TestFalse(TEXT("Dead player cannot reconnect"), Grapple->HasGrappleAnchor());
	return !HasAnyErrors();
}
#endif
