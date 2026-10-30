#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/GrapplingHookComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "GameplayEffect.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "CableComponent.h"
#include "Materials/MaterialInterface.h"
#include "Gimmick/SideScrollingSoftPlatform.h"

namespace GrapplingProbeTest
{
const APlayerBase* PlayerAnimationDefaults()
{
	UClass* Class = LoadClass<APlayerBase>(nullptr,
		TEXT("/Game/Test_Jihong/Blueprints/Player/BP_TestPlayer.BP_TestPlayer_C"));
	return Class ? Class->GetDefaultObject<APlayerBase>() : nullptr;
}

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
	UStaticMeshComponent* MakeSurface(float X, AActor* Owner = nullptr)
	{
		AActor* Actor = Owner ? Owner : World->SpawnActor<AActor>();
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor);
		Actor->SetRootComponent(Mesh);
		Actor->AddInstanceComponent(Mesh);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Mesh->SetRelativeScale3D(FVector(0.5, 1, 1));
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block);
		Mesh->RegisterComponent();
		Actor->SetActorLocation(FVector(X, 0, 0));
		return Mesh;
	}
	UBoxComponent* MakeBlocker(float X)
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
	void TickFrames(int32 Frames, float DeltaTime = 0.02f)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, DeltaTime);
		}
	}
	APlayerBase* SpawnPlayer()
	{
		APlayerBase* Player = World->SpawnActorDeferred<APlayerBase>(APlayerBase::StaticClass(), FTransform::Identity);
		Player->FindComponentByClass<UGrapplingHookComponent>()->CableMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Test_Jihong/Blueprints/GrapplingCable/Materials/M_GrappleCable.M_GrappleCable"));
		if (const APlayerBase* Defaults = PlayerAnimationDefaults())
		{
			Player->RangedAimPostProcessClass = Defaults->RangedAimPostProcessClass;
			const UGrapplingHookComponent* Config = Defaults->FindComponentByClass<UGrapplingHookComponent>();
			UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
			Grapple->PullMontage = Config->PullMontage;
			Grapple->LandMontage = Config->LandMontage;
		}
		Player->FinishSpawning(FTransform::Identity);
		// 기존 훅 동작 검사는 사용 권한을 획득한 플레이어를 기준으로 한다.
		Player->GetAbilitySystemComponent()->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Unlock.GrapplingHook")));
		// The fixture has no controller; explicitly enable the real movement simulation.
		Player->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		// Movement-only cases deliberately omit the cosmetic launch delay.
		Player->FindComponentByClass<UGrapplingHookComponent>()->ThrowMontage = nullptr;
		// 기존 이동 검사는 대기 시간을 제외하고 별도 쿨타임 검사에서 제한 검증
		Player->FindComponentByClass<UGrapplingHookComponent>()->CooldownDuration = 0;
		Player->FindComponentByClass<UGrapplingHookComponent>()->FailedCooldownDuration = 0;
		return Player;
	}
	UAnimInstance* EnableGrappleAnimations(APlayerBase* Player)
	{
		UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
		const APlayerBase* Defaults = PlayerAnimationDefaults();
		if (!Defaults) return nullptr;
		Grapple->ThrowMontage = Defaults->FindComponentByClass<UGrapplingHookComponent>()->ThrowMontage;
		Player->GetMesh()->SetSkeletalMesh(Defaults->GetMesh()->GetSkeletalMeshAsset());
		Player->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Player->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());
		return Player->GetMesh()->GetAnimInstance();
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingProbeTest, "TheLastMessage.Grappling.ProbeAndFixedAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingProbeTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	if (!TestNotNull(TEXT("Player owns grappling component"), Grapple)) return false;
	Grapple->bDrawDebug = false;
	UStaticMeshComponent* Surface = Fixture.MakeSurface(400);
	Player->UpdateAimFromWorldRay(FVector(800,500,0), FVector(0,-1,0));
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Visible mesh attaches without any tag"), Grapple->HasGrappleAnchor());
	TestTrue(TEXT("Anchor is the impact point, not target center"), Grapple->ProbeState.AnchorPoint.Equals(FVector(375,0,0), 0.1f));
	const FVector Anchor = Grapple->ProbeState.AnchorPoint;
	Player->UpdateAimFromWorldRay(FVector(-800,500,0), FVector(0,-1,0));
	Player->SetActorLocation(FVector(-100,0,0));
	TestTrue(TEXT("Changing aim and moving player preserve fixed anchor"), Grapple->ProbeState.AnchorPoint.Equals(Anchor));
	Player->SetActorLocation(FVector::ZeroVector);
	Player->UpdateAimFromWorldRay(FVector(800,500,0), FVector(0,-1,0));
	Grapple->ReleaseGrapple();
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Untagged mesh reconnects"), Grapple->HasGrappleAnchor());
	UBoxComponent* Blocker = Fixture.MakeBlocker(200);
	Grapple->ReleaseGrapple();
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Collision-only wall is skipped"), Grapple->HasGrappleAnchor() && Grapple->ProbeState.AnchorPoint.Equals(Anchor));
	Grapple->ReleaseGrapple();
	Surface->SetHiddenInGame(true);
	Player->RequestGrapplingHook();
	TestFalse(TEXT("Hidden mesh cannot attach"), Grapple->HasGrappleAnchor());
	Surface->SetHiddenInGame(false);
	APawn* Enemy = Fixture.World->SpawnActor<APawn>();
	UStaticMeshComponent* EnemyMesh = Fixture.MakeSurface(200, Enemy);
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Enemy mesh is skipped"), Grapple->HasGrappleAnchor() && Grapple->ProbeState.AnchorPoint.Equals(Anchor));
	Grapple->ReleaseGrapple();
	Surface->SetHiddenInGame(true);
	Player->RequestGrapplingHook();
	TestFalse(TEXT("Enemy mesh alone cannot attach"), Grapple->HasGrappleAnchor());
	Surface->SetHiddenInGame(false);
	EnemyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UStaticMeshComponent* InvisibleWall = Fixture.MakeSurface(200);
	InvisibleWall->SetCollisionProfileName(TEXT("InvisibleWall"));
	Player->RequestGrapplingHook();
	TestTrue(TEXT("InvisibleWall profile cannot become the anchor"), Grapple->HasGrappleAnchor() && Grapple->ProbeState.AnchorPoint.Equals(Anchor));
	Grapple->ReleaseGrapple();
	InvisibleWall->SetCollisionProfileName(TEXT("BlockAll"));
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Nearest visible mesh wins"), Grapple->HasGrappleAnchor() && Grapple->ProbeState.AnchorPoint.Equals(FVector(175,0,0), 0.1f));
	Grapple->ReleaseGrapple();
	InvisibleWall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingPullTest, "TheLastMessage.Grappling.PullMovementAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingPullTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->bRunPhysicsWithNoController = true;
	Movement->MaxFlySpeed = 777.0f;
	const float OriginalGravity = Movement->GravityScale;
	Grapple->bDrawDebug = false;
	Grapple->PullSpeed = 500;
	UStaticMeshComponent* Surface = Fixture.MakeSurface(400);
	Surface->ComponentTags.Add(TEXT("Grappleable"));
	Player->UpdateAimFromWorldRay(FVector(800,500,0), FVector(0,-1,0));
	Player->RequestGrapplingHook();
	TestTrue(TEXT("Connection starts pulling"), Grapple->IsPulling() && Player->IsGrappling());
	TestTrue(TEXT("Pull uses gravity-free movement"), Movement->MovementMode == MOVE_Flying);
	TestTrue(TEXT("GAS blocks attacks while pulling"), Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(WeaponGameplayTags::State_Grappling));
	Player->DoMove(-1);
	TestTrue(TEXT("New movement input is blocked"), Player->GetPendingMovementInputVector().IsNearlyZero());
	Fixture.TickFrames(5);
	TestTrue(TEXT("Character moves towards anchor"), Player->GetActorLocation().X > 20);
	TestTrue(TEXT("Pull stays on side-scrolling plane"), FMath::Abs(Player->GetActorLocation().Y) < 0.01f);
	TestTrue(TEXT("Pull suspends gravity"), FMath::Abs(Player->GetActorLocation().Z) < 0.01f);
	Player->ReleaseGrapplingHook();
	TestFalse(TEXT("Release stops pulling and clears anchor"), Grapple->IsPulling() || Grapple->HasGrappleAnchor());
	TestTrue(TEXT("Release restores falling"), Movement->MovementMode == MOVE_Falling);
	TestEqual(TEXT("Release restores previous fly speed"), Movement->MaxFlySpeed, 777.0f);
	TestEqual(TEXT("Gravity scale preserved"), Movement->GravityScale, OriginalGravity);
	TestFalse(TEXT("Release clears grappling tag"), Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(WeaponGameplayTags::State_Grappling));
	Fixture.TickFrames(1);
	TestTrue(TEXT("Gravity resumes after release"), Player->GetActorLocation().Z < 0);
	Player->SetActorLocation(FVector::ZeroVector);
	Player->UpdateAimFromWorldRay(FVector(800,500,0), FVector(0,-1,0));
	Player->RequestGrapplingHook();
	Fixture.TickFrames(45);
	TestFalse(TEXT("Arrival automatically detaches"), Grapple->HasGrappleAnchor() || Grapple->IsPulling());
	TestTrue(TEXT("Arrival stops outside target capsule collision"), Player->GetActorLocation().X > 300 && Player->GetActorLocation().X < 340);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingInterruptionTest, "TheLastMessage.Grappling.CollisionDamageAndTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingInterruptionTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->bRunPhysicsWithNoController = true;
	Grapple->bDrawDebug = false;
	Fixture.MakeSurface(800)->ComponentTags.Add(TEXT("Grappleable"));
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	auto StartPull = [&]()
	{
		Player->SetActorLocation(FVector::ZeroVector);
		Player->UpdateAimFromWorldRay(FVector(1200,500,0), FVector(0,-1,0));
		Player->RequestGrapplingHook();
	};
	ASC->AddLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	StartPull();
	TestFalse(TEXT("Attack or charge blocks new grappling"), Grapple->IsPulling());
	ASC->RemoveLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	StartPull();
	TestTrue(TEXT("Pull starts after attack ends"), Grapple->IsPulling());
	// Obstacle appears after the connection so the initial ray can see the anchor.
	UBoxComponent* Blocker = Fixture.MakeBlocker(200);
	Fixture.TickFrames(15);
	TestFalse(TEXT("Blocking capsule collision detaches"), Grapple->IsPulling());
	TestTrue(TEXT("Character cannot cross the obstacle"), Player->GetActorLocation().X < 170);
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StartPull();
	ASC->SetNumericAttributeBase(UCombatAttributeSet::GetMaxHealthAttribute(), 100);
	ASC->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), 100);
	UGameplayEffect* Damage = NewObject<UGameplayEffect>();
	Damage->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Mod;
	Mod.Attribute = UCombatAttributeSet::GetIncomingDamageAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10));
	Damage->Modifiers.Add(Mod);
	ASC->ApplyGameplayEffectToSelf(Damage, 1, ASC->MakeEffectContext());
	TestFalse(TEXT("Real damage interrupts even without hit montage"), Grapple->IsPulling() || Grapple->HasGrappleAnchor());
	TestTrue(TEXT("Damage restores gravity movement"), Movement->MovementMode == MOVE_Falling);
	Grapple->MaxPullDuration = 0.1f;
	Grapple->PullSpeed = 10;
	StartPull();
	Fixture.TickFrames(10);
	TestFalse(TEXT("Timeout always releases"), Grapple->IsPulling());
	Grapple->MaxPullDuration = 3;
	StartPull();
	Player->HandleDeath();
	TestFalse(TEXT("Death clears pulling state"), Grapple->IsPulling());
	TestTrue(TEXT("Death keeps gravity active and prevents further grappling"), Player->IsDead() && Movement->MovementMode == MOVE_Falling);
	TestFalse(TEXT("Death clears grappling tag"), ASC->HasMatchingGameplayTag(WeaponGameplayTags::State_Grappling));
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingAnimationTest, "TheLastMessage.Grappling.AnimationStagesAndLanding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingAnimationTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	UAnimInstance* Anim = Fixture.EnableGrappleAnimations(Player);
	if (!TestNotNull(TEXT("Animation instance created on real player skeleton"), Anim)
		|| !TestNotNull(TEXT("Throw montage configured"), Grapple->ThrowMontage.Get())
		|| !TestNotNull(TEXT("Pull montage configured"), Grapple->PullMontage.Get())
		|| !TestNotNull(TEXT("Land montage configured"), Grapple->LandMontage.Get())) return false;
	for (UAnimMontage* Montage : { Grapple->ThrowMontage.Get(), Grapple->PullMontage.Get(), Grapple->LandMontage.Get() })
	{
		const UAnimSequence* Sequence = Cast<UAnimSequence>(Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference());
		TestTrue(TEXT("Grappling sequences lock the root without driving capsule movement"), Sequence && Sequence->bForceRootLock && !Sequence->bEnableRootMotion);
	}
	Grapple->bDrawDebug = false;
	Grapple->PullSpeed = 100;
	Grapple->MaxPullDuration = 15;
	Fixture.MakeSurface(1000)->ComponentTags.Add(TEXT("Grappleable"));
	auto Start = [&]()
	{
		Player->UpdateAimFromWorldRay(FVector(1400,500,0), FVector(0,-1,0));
		Player->RequestGrapplingHook();
	};
	Start();
	TestTrue(TEXT("Throw montage starts first"), Grapple->AnimationPhase == EGrappleAnimationPhase::Throw && Anim->Montage_IsPlaying(Grapple->ThrowMontage));
	Fixture.TickFrames(2);
	TestTrue(TEXT("Launch completes before pulling starts"), Player->GetActorLocation().IsNearlyZero(0.1f));
	Fixture.TickFrames(8);
	TestTrue(TEXT("Short launch delay starts movement within 0.2 seconds"), Player->GetActorLocation().X > 0);
	Fixture.TickFrames(70);
	TestTrue(TEXT("Throw transitions into pull montage"), Grapple->AnimationPhase == EGrappleAnimationPhase::Pull && Anim->Montage_IsActive(Grapple->PullMontage));
	TestTrue(TEXT("Pull pose remains active beyond clip end"), Anim->Montage_GetPosition(Grapple->PullMontage) >= Grapple->PullMontage->GetPlayLength() - 0.05f);
	TestTrue(TEXT("Movement continues after pull clip finishes"), Grapple->IsPulling() && Player->GetActorLocation().X > 50);
	Player->ReleaseGrapplingHook();
	Fixture.TickFrames(8);
	TestTrue(TEXT("Cancel blends back to locomotion without a landing pose"), Grapple->AnimationPhase == EGrappleAnimationPhase::None && !Anim->Montage_IsActive(Grapple->PullMontage));
	Player->SetActorLocation(FVector::ZeroVector);
	Grapple->PullSpeed = 2000;
	Start();
	Fixture.TickFrames(54);
	TestTrue(TEXT("Air arrival returns to falling instead of playing landing"), !Grapple->IsPulling() && Grapple->AnimationPhase == EGrappleAnimationPhase::None);
	UBoxComponent* Floor = Fixture.MakeBlocker(-2000);
	Floor->SetBoxExtent(FVector(3000,100,50));
	Floor->GetOwner()->SetActorLocation(FVector(0,0,-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-50));
	Player->SetActorLocation(FVector::ZeroVector);
	Start();
	Fixture.TickFrames(45);
	TestTrue(TEXT("Real floor contact plays landing montage"), Player->GetCharacterMovement()->IsMovingOnGround()
		&& Grapple->AnimationPhase == EGrappleAnimationPhase::Land && Anim->Montage_IsActive(Grapple->LandMontage));
	Player->HandleDeath();
	TestTrue(TEXT("Death clears any grappling animation"), Grapple->AnimationPhase == EGrappleAnimationPhase::None);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingCancelJumpCableTest, "TheLastMessage.Grappling.CancelBonusJumpAndCable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingCancelJumpCableTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	const FGameplayTag DoubleJump = FGameplayTag::RequestGameplayTag(TEXT("Unlock.DoubleJump"));
	ASC->AddLooseGameplayTag(DoubleJump);
	Player->JumpCurrentCount = Player->JumpMaxCount;
	TestFalse(TEXT("Both jumps already spent before grappling"), Player->CanJump());
	Fixture.MakeSurface(400)->ComponentTags.Add(TEXT("Grappleable"));
	auto Start = [&]()
	{
		Player->UpdateAimFromWorldRay(FVector(800,500,0), FVector(0,-1,0));
		Player->RequestGrapplingHook();
	};
	Start();
	if (!TestNotNull(TEXT("Cable component created"), Grapple->Cable.Get())) return false;
	TestTrue(TEXT("Real cable shown on connection"), Grapple->Cable->IsVisible() && Grapple->Cable->IsComponentTickEnabled());
	TestNotNull(TEXT("Cable uses assigned material"), Grapple->Cable->GetMaterial(0));
	Fixture.TickFrames(2);
	TArray<FVector> Particles;
	Grapple->Cable->GetCableParticleLocations(Particles);
	TestTrue(TEXT("Cable simulates a rendered rope"), Particles.Num() > 2);
	TestTrue(TEXT("Cable endpoint stays at fixed anchor"), !Particles.IsEmpty() && Particles.Last().Equals(Grapple->ProbeState.AnchorPoint, 0.1f));
	// Second E press cancels, independently of mouse aim and spent jumps.
	Player->RequestGrapplingHook();
	TestFalse(TEXT("Second E cancels"), Grapple->IsPulling() || Grapple->HasGrappleAnchor());
	TestTrue(TEXT("Cancelled cable hidden and simulation stopped"), !Grapple->Cable->IsVisible() && !Grapple->Cable->IsComponentTickEnabled());
	TestTrue(TEXT("Cancel grants a jump even after double jump was spent"), Player->CanJump());
	Player->DoJumpStart();
	Fixture.TickFrames(1);
	TestTrue(TEXT("Bonus jump really launches upward"), Player->GetCharacterMovement()->Velocity.Z > 0);
	Player->DoJumpEnd();
	TestFalse(TEXT("Bonus is consumed after exactly one jump"), Player->CanJump());
	// Space during grappling cancels and consumes the guaranteed jump in the same press.
	Player->SetActorLocation(FVector::ZeroVector);
	Start();
	Player->DoJumpStart();
	Fixture.TickFrames(1);
	TestTrue(TEXT("Space detaches and jumps immediately"), !Grapple->IsPulling() && Player->GetCharacterMovement()->Velocity.Z > 0);
	Player->DoJumpEnd();
	TestFalse(TEXT("Space cancel also grants only one jump"), Player->CanJump());
	// The guarantee also works with the base single-jump limit.
	ASC->RemoveLooseGameplayTag(DoubleJump);
	Player->JumpCurrentCount = Player->JumpMaxCount;
	Player->SetActorLocation(FVector::ZeroVector);
	Start();
	Player->RequestGrapplingHook();
	Player->DoJumpStart();
	Fixture.TickFrames(1);
	TestTrue(TEXT("Cancel guarantees one air jump even without normal double-jump unlock"), Player->GetCharacterMovement()->Velocity.Z > 0);
	Player->DoJumpEnd();
	TestFalse(TEXT("Single-jump character cannot reuse the bonus"), Player->CanJump());
	// Automatic arrival must preserve exhausted jumps, rather than granting the cancel bonus.
	ASC->AddLooseGameplayTag(DoubleJump);
	Player->JumpCurrentCount = Player->JumpMaxCount;
	Player->SetActorLocation(FVector::ZeroVector);
	Start();
	Fixture.TickFrames(25);
	TestFalse(TEXT("Arrival preserves exhausted jump budget"), Grapple->IsPulling() || Player->CanJump());
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingCooldownTest, "TheLastMessage.Grappling.CooldownAndRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingCooldownTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	Grapple->CooldownDuration = 1;
	Grapple->FailedCooldownDuration = 0.2f;
	auto Request = [&]() { Grapple->RequestGrapple(FVector(800,0,0)); };
	TestTrue(TEXT("Unlocked component is visible to HUD"), Grapple->IsGrappleUnlocked());
	Request();
	TestTrue(TEXT("Miss starts short retry cooldown"), FMath::IsNearlyEqual(Grapple->GetCooldownRemaining(), 0.2f, 0.01f));
	TestTrue(TEXT("Cooldown ring starts empty"), Grapple->GetCooldownProgress() < 0.01f);
	Fixture.MakeSurface(400);
	Request();
	TestFalse(TEXT("Immediate retry cannot connect"), Grapple->HasGrappleAnchor());
	Fixture.TickFrames(6);
	TestTrue(TEXT("Cooldown ring advances with game time"), Grapple->GetCooldownProgress() > 0.5f && Grapple->GetCooldownProgress() < 1);
	Fixture.TickFrames(6);
	Player->SetActorLocation(FVector::ZeroVector);
	Request();
	TestTrue(TEXT("Retry connects when short cooldown expires"), Grapple->HasGrappleAnchor());
	TestEqual(TEXT("Active hook has no running cooldown"), Grapple->GetCooldownRemaining(), 0.0f);
	Grapple->CancelGrapple();
	TestFalse(TEXT("Cancellation remains available"), Grapple->HasGrappleAnchor());
	TestTrue(TEXT("Cancellation starts one-second cooldown"), FMath::IsNearlyEqual(Grapple->GetCooldownRemaining(), 1.0f, 0.01f));
	Fixture.TickFrames(10);
	const float Remaining = Grapple->GetCooldownRemaining();
	Grapple->ClearAnchor();
	Request();
	TestFalse(TEXT("Repeated request cannot bypass cooldown"), Grapple->HasGrappleAnchor());
	TestEqual(TEXT("Cleanup and blocked input do not restart cooldown"), Grapple->GetCooldownRemaining(), Remaining);
	Fixture.TickFrames(42);
	Player->SetActorLocation(FVector::ZeroVector);
	Request();
	TestTrue(TEXT("Hook becomes usable again"), Grapple->HasGrappleAnchor());
	Grapple->ReleaseGrapple();
	TestTrue(TEXT("Normal release also starts full cooldown"), Grapple->GetCooldownRemaining() > 0.99f);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingDecorationTest, "TheLastMessage.Grappling.UntaggedDecorationMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingDecorationTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	UStaticMeshComponent* Decoration = Fixture.MakeSurface(400);
	Decoration->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Decoration->SetCollisionResponseToAllChannels(ECR_Ignore);
	UStaticMeshComponent* HiddenWall = Fixture.MakeSurface(200);
	HiddenWall->SetHiddenInGame(true);
	HiddenWall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	APawn* Enemy = Fixture.World->SpawnActor<APawn>();
	UStaticMeshComponent* EnemyMesh = Fixture.MakeSurface(300, Enemy);
	EnemyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Grapple->RequestGrapple(FVector(800,0,0));
	TestTrue(TEXT("Untagged NoCollision decoration attaches without editor setup"), Grapple->HasGrappleAnchor()
		&& Grapple->ProbeState.AnchorPoint.Equals(FVector(375,0,0), 0.1f));
	TestTrue(TEXT("Decoration only participates in queries"), Decoration->GetCollisionEnabled() == ECollisionEnabled::QueryOnly);
	TestTrue(TEXT("Decoration still ignores movement and weapon visibility traces"),
		Decoration->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore
		&& Decoration->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Ignore
		&& Decoration->GetCollisionResponseToChannel(ECC_GameTraceChannel3) == ECR_Ignore);
	TestTrue(TEXT("Hidden wall stays disabled"), HiddenWall->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Enemy mesh stays disabled"), EnemyMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	Grapple->ReleaseGrapple();
	Decoration->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Ignore);
	Decoration->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Grapple->RequestGrapple(FVector(800,0,0));
	TestTrue(TEXT("Existing mesh with ignored grapple channel also attaches"), Grapple->HasGrappleAnchor());
	TestTrue(TEXT("Existing movement response is preserved"), Decoration->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingTerrainTest, "TheLastMessage.Grappling.FloorAndLintelSurfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingTerrainTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	UStaticMeshComponent* Floor = Fixture.MakeSurface(0);
	Floor->SetCollisionProfileName(TEXT("GrappleTerrain"));
	Floor->SetWorldScale3D(FVector(8,4,1));
	Floor->GetOwner()->SetActorLocation(FVector(0,0,-150));
	Floor->SetVisibility(false);
	Floor->GetOwner()->SetActorHiddenInGame(true);
	Grapple->RequestGrapple(FVector(0,0,-800));
	TestTrue(TEXT("Hidden floor collision surface attaches without a tag"), Grapple->HasGrappleAnchor()
		&& Grapple->ProbeState.AnchorPoint.Equals(FVector(0,0,-100), 0.1f));
	Grapple->ReleaseGrapple();
	UStaticMeshComponent* Lintel = Fixture.MakeSurface(400);
	Lintel->SetCollisionProfileName(TEXT("GrappleTerrain"));
	Lintel->SetWorldScale3D(FVector(0.5,4,3));
	Lintel->GetOwner()->SetActorLocation(FVector(400,0,150));
	Lintel->SetVisibility(false);
	Lintel->GetOwner()->SetActorHiddenInGame(true);
	UStaticMeshComponent* Boundary = Fixture.MakeSurface(200);
	Boundary->SetCollisionProfileName(TEXT("InvisibleWall"));
	Boundary->SetWorldScale3D(FVector(0.5,4,8));
	Boundary->GetOwner()->SetActorHiddenInGame(true);
	Grapple->RequestGrapple(FVector(800,0,300));
	TestTrue(TEXT("Lintel surface attaches while transparent boundary is excluded"), Grapple->HasGrappleAnchor()
		&& FMath::IsNearlyEqual(Grapple->ProbeState.AnchorPoint.X, 375.0, 0.1));
	Grapple->ReleaseGrapple();
	Lintel->SetCollisionProfileName(TEXT("BlockAll"));
	Grapple->RequestGrapple(FVector(800,0,300));
	TestFalse(TEXT("Ordinary hidden blockers remain excluded"), Grapple->HasGrappleAnchor());
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingSoftPlatformFacesTest, "TheLastMessage.Grappling.SoftPlatformBothFaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingSoftPlatformFacesTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	UClass* PlatformClass = LoadClass<ASideScrollingSoftPlatform>(nullptr,
		TEXT("/Game/Test_Jihong/Blueprints/BP_SoftPlatform_Metroidvania.BP_SoftPlatform_Metroidvania_C"));
	if (!TestNotNull(TEXT("Metroidvania platform class exists"), PlatformClass)) return false;
	ASideScrollingSoftPlatform* Platform = Fixture.World->SpawnActor<ASideScrollingSoftPlatform>(
		PlatformClass, FVector(0,0,500), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Platform spawned"), Platform)) return false;
	// Reproduce separate art plus a hidden movement collider. Disable kit art so
	// the test must use the collider, independent of art triangle sidedness.
	TArray<UStaticMeshComponent*> Meshes;
	Platform->GetComponents(Meshes);
	UStaticMeshComponent* Collider = Platform->FindComponentByClass<UStaticMeshComponent>();
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Mesh != Collider) Mesh->SetStaticMesh(nullptr);
	}
	Collider->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Collider->SetRelativeLocation(FVector::ZeroVector);
	Collider->SetRelativeRotation(FRotator::ZeroRotator);
	Collider->SetRelativeScale3D(FVector(4,4,0.5));
	Collider->SetVisibility(false);
	Collider->SetHiddenInGame(true);
	Collider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collider->SetCollisionObjectType(ECC_GameTraceChannel1);
	Collider->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Ignore);
	Grapple->RequestGrapple(FVector(0,0,1000));
	TestTrue(TEXT("Upward hook connects to hidden platform underside"), Grapple->HasGrappleAnchor()
		&& Grapple->ProbeState.SurfaceNormal.Z < -0.9 && FMath::IsNearlyEqual(Grapple->ProbeState.AnchorPoint.Z, 475., 1.));
	Grapple->ReleaseGrapple();
	Player->SetActorLocation(FVector(0,0,1000));
	Grapple->RequestGrapple(FVector(0,0,0));
	TestTrue(TEXT("Downward hook still connects to platform top"), Grapple->HasGrappleAnchor()
		&& Grapple->ProbeState.SurfaceNormal.Z > 0.9);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingNamedWallTest, "TheLastMessage.Grappling.NamedWallProxy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingNamedWallTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = TEXT("Wall_GrappleRegression");
	AActor* Wall = Fixture.World->SpawnActor<AActor>(SpawnParams);
	UStaticMeshComponent* Mesh = Fixture.MakeSurface(400, Wall);
	Mesh->SetVisibility(false);
	Mesh->SetHiddenInGame(true);
	Wall->SetActorHiddenInGame(true);
	Grapple->RequestGrapple(FVector(800,0,0));
	TestTrue(TEXT("Wall_ terrain proxy connects despite hidden art proxy"), Grapple->HasGrappleAnchor()
		&& FMath::IsNearlyEqual(Grapple->ProbeState.AnchorPoint.X,375.,1.));
	Grapple->ReleaseGrapple();
	Mesh->SetCollisionProfileName(TEXT("InvisibleWall"));
	Grapple->RequestGrapple(FVector(800,0,0));
	TestFalse(TEXT("Explicit invisible boundary remains excluded even with Wall_ name"), Grapple->HasGrappleAnchor());
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingAuthoredCollisionTest, "TheLastMessage.Grappling.AuthoredCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingAuthoredCollisionTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	Fixture.World->GetWorldSettings()->Tags.Add(TEXT("GrappleCollisionAuthored"));
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	UStaticMeshComponent* Surface = Fixture.MakeSurface(400);
	UStaticMeshComponent* Disabled = Fixture.MakeSurface(200);
	Disabled->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	APawn* Enemy = Fixture.World->SpawnActor<APawn>();
	Fixture.MakeSurface(300, Enemy);
	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		Grapple->RequestGrapple(FVector(800, 0, 0));
		TestTrue(TEXT("Authored surface is hit past enemy and disabled decoration"),
			Grapple->HasGrappleAnchor() && FMath::IsNearlyEqual(Grapple->ProbeState.AnchorPoint.X, 375., 1.));
		TestTrue(TEXT("Probe never enables disabled mesh collision"), Disabled->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestTrue(TEXT("Movement collision stays enabled"), Surface->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
		Grapple->ReleaseGrapple();
	}
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingAuthoredFactoryMeshTest, "TheLastMessage.Grappling.AuthoredFactoryMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingAuthoredFactoryMeshTest::RunTest(const FString& Parameters)
{
	GrapplingProbeTest::FWorld Fixture;
	Fixture.World->GetWorldSettings()->Tags.Add(TEXT("GrappleCollisionAuthored"));
	APlayerBase* Player = Fixture.SpawnPlayer();
	UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
	UStaticMesh* FactoryFloor = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Test_Jihong/Prototype/CollisionMeshes/SM_Floor_4x4_A_Collision.SM_Floor_4x4_A_Collision"));
	if (!TestNotNull(TEXT("Authored factory floor asset exists"), FactoryFloor)) return false;
	UStaticMeshComponent* Surface = Fixture.MakeSurface(400);
	Surface->SetStaticMesh(FactoryFloor);
	Surface->GetOwner()->SetActorLocation(FVector(400, 0, 5));
	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		Grapple->RequestGrapple(FVector(800, 0, 0));
		TestTrue(TEXT("Actual factory terrain accepts hook using its movement collision"),
			Grapple->HasGrappleAnchor() && FMath::IsNearlyEqual(Grapple->ProbeState.AnchorPoint.X, 300., 1.));
		TestTrue(TEXT("Hook does not change terrain collision"),
			Surface->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
		Grapple->ReleaseGrapple();
	}
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrapplingGravityOrientationTest, "TheLastMessage.Grappling.GravityOrientation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGrapplingGravityOrientationTest::RunTest(const FString& Parameters)
{
	for (const float GravitySign : { -1.0f, 1.0f })
	{
		for (const float FacingSign : { -1.0f, 1.0f })
		{
			GrapplingProbeTest::FWorld Fixture;
			APlayerBase* Player = Fixture.SpawnPlayer();
			UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
			UGrapplingHookComponent* Grapple = Player->FindComponentByClass<UGrapplingHookComponent>();
			Movement->SetGravityDirection(FVector(0, 0, GravitySign));
			Grapple->bDrawDebug = false;
			Fixture.MakeSurface(FacingSign * 800)->ComponentTags.Add(TEXT("Grappleable"));
			Player->UpdateAimFromWorldRay(FVector(FacingSign * 800, 500, 0), FVector(0, -1, 0));
			Player->RequestGrapplingHook();
			TestTrue(TEXT("Hook starts in either gravity and facing direction"), Grapple->IsPulling());
			const FVector ExpectedUp(0, 0, -GravitySign);
			TestTrue(TEXT("Throw preserves gravity-aligned animation basis"), Player->GetActorUpVector().Equals(ExpectedUp, 0.001f));
			TestTrue(TEXT("Hook faces the target"), Player->GetActorForwardVector().Equals(FVector(FacingSign, 0, 0), 0.001f));
			Fixture.TickFrames(3);
			TestTrue(TEXT("Pull keeps animations gravity-aligned"), Player->GetActorUpVector().Equals(ExpectedUp, 0.001f));
			Player->ReleaseGrapplingHook();
			TestTrue(TEXT("Release keeps gravity alignment for subsequent animations"), Player->GetActorUpVector().Equals(ExpectedUp, 0.001f));
			Player->GrantGrappleCancelJump();
			const FVector BeforeJump = Movement->Velocity;
			Player->DoJumpStart();
			Fixture.TickFrames(1);
			TestTrue(TEXT("Cancel jump launches against the current gravity"),
				FVector::DotProduct(Movement->Velocity - BeforeJump, ExpectedUp) > 0.0f);
			TestTrue(TEXT("Jump animation basis stays inverted when gravity is reversed"),
				Player->GetActorUpVector().Equals(ExpectedUp, 0.001f));
		}
	}
	return !HasAnyErrors();
}
#endif
