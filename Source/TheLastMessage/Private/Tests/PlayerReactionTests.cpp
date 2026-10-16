#include "Character/Player/PlayerBase.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/Abilities/GA_AttackPrimary.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace PlayerReactionTests
{
struct FTestWorld
{
	UWorld* World;
	FTestWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		FURL URL;
		URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
		World->SetGameMode(URL);
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		World->GetWorldSettings()->NotifyBeginPlay();
	}
	~FTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
};

void SetObject(UObject* Object, const TCHAR* Name, UObject* Value)
{
	FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name);
	check(Property);
	Property->SetObjectPropertyValue_InContainer(Object, Value);
}

void ApplyModifier(APlayerBase* Player, const FGameplayAttribute& Attribute, float Magnitude)
{
	UGameplayEffect* Effect = NewObject<UGameplayEffect>();
	Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
	Effect->Modifiers.Add(Modifier);
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	FGameplayEffectSpec Spec(Effect, ASC->MakeEffectContext(), 1.0f);
	ASC->ApplyGameplayEffectSpecToSelf(Spec);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerDamageReactionTest, "TheLastMessage.PlayerBase.DamageReactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerDamageReactionTest::RunTest(const FString& Parameters)
{
	using namespace PlayerReactionTests;
	FTestWorld Fixture;
	APlayerBase* Player = Fixture.World->SpawnActorDeferred<APlayerBase>(APlayerBase::StaticClass(), FTransform::Identity,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Player spawns"), Player)) return false;
	UWeaponDataAsset* WeaponData = NewObject<UWeaponDataAsset>(Player);
	WeaponData->AttackRange = 150.0f;
	WeaponData->bSupportsChargeAttack = true;
	WeaponData->AttackMontage = NewObject<UAnimMontage>(Player);
	SetObject(Player, TEXT("InitialPrimaryWeaponData"), WeaponData);
	FindFProperty<FClassProperty>(Player->GetClass(), TEXT("WeaponDamageEffectClass"))
		->SetObjectPropertyValue_InContainer(Player, UGameplayEffect::StaticClass());
	Player->FinishSpawning(FTransform::Identity);
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	const UCombatAttributeSet* Stats = Player->GetCombatAttributes();
	TestEqual(TEXT("Engine damage reaches health"), UGameplayStatics::ApplyDamage(Player, 10.0f, nullptr, nullptr, nullptr), 10.0f);
	TestEqual(TEXT("Engine damage applies once"), Stats->GetHealth(), 90.0f);
	TestFalse(TEXT("Unassigned montage does not lock controls"), Player->IsHitReacting());
	UAnimMontage* HitMontage = NewObject<UAnimMontage>(Player);
	HitMontage->SetCompositeLength(0.8f);
	HitMontage->AddAnimCompositeSection(TEXT("Backward"), 0.0f);
	HitMontage->AddAnimCompositeSection(TEXT("Forward"), 0.3f);
	SetObject(Player, TEXT("HitReactMontage"), HitMontage);
	SetObject(Player->GetMesh(), TEXT("AnimScriptInstance"), NewObject<UAnimInstance>(Player->GetMesh()));
	FGameplayAbilitySpec* Attack = ASC->FindAbilitySpecFromClass(UGA_AttackPrimary::StaticClass());
	if (!TestNotNull(TEXT("Primary attack exists"), Attack)) return false;
	ASC->AbilitySpecInputPressed(*Attack);
	TestTrue(TEXT("Charge starts before hit"), ASC->TryActivateAbility(Attack->Handle));
	// Simulate a server without an animation instance: recovery must still finish on its timer.
	SetObject(Player->GetMesh(), TEXT("AnimScriptInstance"), nullptr);
	ApplyModifier(Player, UCombatAttributeSet::GetIncomingDamageAttribute(), 10.0f);
	TestEqual(TEXT("GAS damage applies once"), Stats->GetHealth(), 80.0f);
	TestEqual(TEXT("Incoming damage cleared"), Stats->GetIncomingDamage(), 0.0f);
	TestTrue(TEXT("Nonlethal damage starts hit reaction"), Player->IsHitReacting());
	TestFalse(TEXT("Hit cancels charge"), Attack->IsActive());
	Player->DoMove(1.0f);
	Player->DoJumpStart();
	TestTrue(TEXT("Hit blocks new movement"), Player->GetPendingMovementInputVector().IsNearlyZero());
	TestFalse(TEXT("Hit blocks jump"), Player->bPressedJump);
	ApplyModifier(Player, UCombatAttributeSet::GetIncomingDamageAttribute(), 5.0f);
	TestEqual(TEXT("Repeated hits still damage health"), Stats->GetHealth(), 75.0f);
	TestEqual(TEXT("Repeated hits do not stack reaction state"), ASC->GetGameplayTagCount(WeaponGameplayTags::State_HitReacting), 1);
	ASC->AbilitySpecInputReleased(*Attack);
	TestFalse(TEXT("Release after interruption does not consume attack cooldown"), ASC->HasMatchingGameplayTag(WeaponGameplayTags::Cooldown_Primary));
	for (int32 Frame = 0; Frame < 6; ++Frame)
	{
		++GFrameCounter;
		Fixture.World->Tick(LEVELTICK_All, 0.1f);
	}
	TestFalse(TEXT("Reaction recovers without an anim instance"), Player->IsHitReacting());
	ApplyModifier(Player, UCombatAttributeSet::GetHealthAttribute(), 5.0f);
	TestFalse(TEXT("Healing does not cause hit reaction"), Player->IsHitReacting());
	ApplyModifier(Player, UCombatAttributeSet::GetIncomingDamageAttribute(), 5.0f);
	TestTrue(TEXT("A new hit can react after recovery"), Player->IsHitReacting());
	ApplyModifier(Player, UCombatAttributeSet::GetIncomingDamageAttribute(), 500.0f);
	TestTrue(TEXT("Lethal damage enters death state"), Player->IsDead());
	TestEqual(TEXT("Lethal damage clamps health"), Stats->GetHealth(), 0.0f);
	TestFalse(TEXT("Death clears hit reaction"), Player->IsHitReacting());
	TestTrue(TEXT("Death stops movement"), Player->GetCharacterMovement()->MovementMode == MOVE_None);
	TestEqual(TEXT("Dead player ignores engine damage"), UGameplayStatics::ApplyDamage(Player, 10.0f, nullptr, nullptr, nullptr), 0.0f);
	Player->HandleDeath();
	TestEqual(TEXT("Death state does not stack"), ASC->GetGameplayTagCount(WeaponGameplayTags::State_Dead), 1);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerDirectionalReactionTest, "TheLastMessage.PlayerBase.DirectionalReactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerDirectionalReactionTest::RunTest(const FString& Parameters)
{
	PlayerReactionTests::FTestWorld Fixture;
	APlayerBase* Player = Fixture.World->SpawnActor<APlayerBase>();
	APlayerBase* Source = Fixture.World->SpawnActor<APlayerBase>();
	if (!TestNotNull(TEXT("Target spawns"), Player) || !TestNotNull(TEXT("Source spawns"), Source)) return false;
	FGameplayEffectContextHandle Context = Source->GetAbilitySystemComponent()->MakeEffectContext();
	Context.AddInstigator(Source, Source);
	Player->SetActorRotation(FRotator::ZeroRotator);
	Source->SetActorLocation(Player->GetActorLocation() + FVector(200.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Frontal hit selects Backward"), Player->GetDamageReactionSection(Context), FName(TEXT("Backward")));
	Source->SetActorLocation(Player->GetActorLocation() - FVector(200.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Rear hit selects Forward"), Player->GetDamageReactionSection(Context), FName(TEXT("Forward")));
	Player->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
	TestEqual(TEXT("Facing left reverses the front side"), Player->GetDamageReactionSection(Context), FName(TEXT("Backward")));
	Source->SetActorLocation(Player->GetActorLocation() + FVector(200.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Facing left reverses the rear side"), Player->GetDamageReactionSection(Context), FName(TEXT("Forward")));
	FHitResult Hit;
	Hit.TraceStart = Player->GetActorLocation() - FVector(200.0f, 0.0f, 0.0f);
	Hit.TraceEnd = Player->GetActorLocation();
	Context.AddHitResult(Hit, true);
	TestEqual(TEXT("Actual projectile direction overrides shooter position"), Player->GetDamageReactionSection(Context), FName(TEXT("Backward")));
	FGameplayEffectContextHandle Unknown = Player->GetAbilitySystemComponent()->MakeEffectContext();
	TestEqual(TEXT("Unknown damage defaults to Backward"), Player->GetDamageReactionSection(Unknown), FName(TEXT("Backward")));
	return !HasAnyErrors();
}
#endif
