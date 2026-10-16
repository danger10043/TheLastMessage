#include "Character/Player/PlayerBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "DataAsset/WeaponDataAsset.h"
#include "GAS/Abilities/GA_AttackPrimary.h"
#include "GAS/Abilities/GA_AttackSecondary.h"
#include "GAS/Effects/GE_WeaponMeleeDamage.h"
#include "GAS/WeaponGameplayTags.h"
#include "Weapon/WeaponBase.h"

namespace WeaponChargeTests
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
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerDualWeaponChargeTest, "TheLastMessage.PlayerBase.DualWeaponCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerDualWeaponChargeTest::RunTest(const FString& Parameters)
{
	WeaponChargeTests::FTestWorld Fixture;
	APlayerBase* Player = Fixture.World->SpawnActorDeferred<APlayerBase>(APlayerBase::StaticClass(), FTransform::Identity,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Player spawns"), Player)) return false;
	auto SetObject = [](UObject* Object, const TCHAR* Name, UObject* Value)
	{
		FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name);
		check(Property);
		Property->SetObjectPropertyValue_InContainer(Object, Value);
	};
	UWeaponDataAsset* Data = NewObject<UWeaponDataAsset>(Player);
	Data->AttackRange = 150.0f;
	Data->AttackPower = 20.0f;
	Data->bSupportsChargeAttack = true;
	Data->MaxChargeTime = 1.0f;
	Data->MaxChargeDamageMultiplier = 3.0f;
	Data->AttackMontage = NewObject<UAnimMontage>(Data);
	SetObject(Player, TEXT("InitialPrimaryWeaponData"), Data);
	SetObject(Player, TEXT("InitialSecondaryWeaponData"), Data);
	FClassProperty* DamageEffectProperty = FindFProperty<FClassProperty>(Player->GetClass(), TEXT("WeaponDamageEffectClass"));
	if (!TestNotNull(TEXT("Damage effect property exists"), DamageEffectProperty)) return false;
	DamageEffectProperty->SetObjectPropertyValue_InContainer(Player, UGE_WeaponMeleeDamage::StaticClass());
	Player->FinishSpawning(FTransform::Identity);
	// A transient anim instance lets us test charging without depending on content assets.
	SetObject(Player->GetMesh(), TEXT("AnimScriptInstance"), NewObject<UAnimInstance>(Player->GetMesh()));
	if (!TestNotNull(TEXT("Primary equipped"), Player->GetPrimaryWeapon()) ||
		!TestNotNull(TEXT("Secondary equipped"), Player->GetSecondaryWeapon())) return false;
	TestTrue(TEXT("Slots own independent weapon instances"), Player->GetPrimaryWeapon() != Player->GetSecondaryWeapon());
	TestNull(TEXT("None slot is empty"), Player->GetWeaponInSlot(EWeaponSlot::None));
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	const TSubclassOf<UGameplayAbility> Classes[] = { UGA_AttackPrimary::StaticClass(), UGA_AttackSecondary::StaticClass() };
	const FGameplayTag Cooldowns[] = { WeaponGameplayTags::Cooldown_Primary, WeaponGameplayTags::Cooldown_Secondary };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Classes[Index]);
		if (!TestNotNull(TEXT("Slot attack granted"), Spec)) return false;
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		const FVector JumpVelocity(450.0f, 0.0f, 300.0f);
		Player->GetCharacterMovement()->Velocity = JumpVelocity;
		Player->AddMovementInput(FVector::ForwardVector);
		ASC->AbilitySpecInputPressed(*Spec);
		TestTrue(TEXT("Slot charge activates"), ASC->TryActivateAbility(Spec->Handle));
		const EWeaponSlot ExpectedSlot = Index == 0 ? EWeaponSlot::Primary : EWeaponSlot::Secondary;
		TestEqual(TEXT("Charge shows its own weapon slot"), Player->GetVisibleWeaponSlot(), ExpectedSlot);
		auto* Attack = Cast<UGA_WeaponAttackBase>(Spec->GetPrimaryInstance());
		if (!TestNotNull(TEXT("Attack instance exists"), Attack)) return false;
		TestTrue(TEXT("Jump velocity survives charge start"), Player->GetCharacterMovement()->Velocity.Equals(JumpVelocity));
		TestTrue(TEXT("Queued movement is discarded"), Player->GetPendingMovementInputVector().IsNearlyZero());
		Player->DoMove(-1.0f);
		TestTrue(TEXT("New direction input is blocked"), Player->GetPendingMovementInputVector().IsNearlyZero());
		TestFalse(TEXT("Other slot is blocked while charging"), ASC->TryActivateAbility(ASC->FindAbilitySpecFromClass(Classes[1 - Index])->Handle));
		TestEqual(TEXT("Blocked attack does not switch displayed weapon"), Player->GetVisibleWeaponSlot(), ExpectedSlot);
		TestFalse(TEXT("Charging does not consume cooldown"), ASC->HasMatchingGameplayTag(Cooldowns[Index]));
		// 여러 프레임으로 진행해야 엔진의 프레임당 DeltaTime 제한에 걸리지 않는다.
		for (int32 Frame = 0; Frame < 5; ++Frame) Fixture.World->Tick(LEVELTICK_All, 0.1f);
		TestTrue(TEXT("Half hold gives half charge"), FMath::IsNearlyEqual(Attack->GetChargeRatio(), 0.5f, 0.01f));
		for (int32 Frame = 0; Frame < 10; ++Frame) Fixture.World->Tick(LEVELTICK_All, 0.1f);
		TestEqual(TEXT("Charge caps at full"), Attack->GetChargeRatio(), 1.0f);
		ASC->CancelAbilityHandle(Spec->Handle);
		TestFalse(TEXT("Cancel clears attack state"), ASC->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking));
		TestEqual(TEXT("Cancel clears charge"), Attack->GetChargeRatio(), 0.0f);
		TestFalse(TEXT("Cancel has no cooldown"), ASC->HasMatchingGameplayTag(Cooldowns[Index]));
		ASC->AbilitySpecInputReleased(*Spec);
		TestFalse(TEXT("Release after cancel does not attack"), Spec->IsActive());
		ASC->AbilitySpecInputPressed(*Spec);
		TestTrue(TEXT("Charge can restart after cancel"), ASC->TryActivateAbility(Spec->Handle));
		const int32 OtherCooldownCount = ASC->GetGameplayTagCount(Cooldowns[1 - Index]);
		// The asset-free montage cannot play; the release must still commit exactly this slot's cooldown.
		ASC->AbilitySpecInputReleased(*Spec);
		TestTrue(TEXT("Release commits own cooldown"), ASC->HasMatchingGameplayTag(Cooldowns[Index]));
		TestEqual(TEXT("Release does not commit other slot cooldown"), ASC->GetGameplayTagCount(Cooldowns[1 - Index]), OtherCooldownCount);
		ASC->CancelAbilityHandle(Spec->Handle);
	}
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerWeaponVisualTest, "TheLastMessage.PlayerBase.WeaponVisual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerWeaponVisualTest::RunTest(const FString& Parameters)
{
	WeaponChargeTests::FTestWorld Fixture;
	APlayerBase* Player = Fixture.World->SpawnActorDeferred<APlayerBase>(APlayerBase::StaticClass(), FTransform::Identity,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Player spawns"), Player)) return false;
	USkeletalMesh* CharacterMesh = NewObject<USkeletalMesh>(Player);
	USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(CharacterMesh);
	Socket->SocketName = TEXT("SK_Weapon");
	Socket->BoneName = TEXT("root");
	CharacterMesh->GetMeshOnlySocketList().Add(Socket);
	CharacterMesh->RebuildSocketMap();
	Player->GetMesh()->SetSkeletalMeshAsset(CharacterMesh);
	UWeaponDataAsset* PrimaryData = NewObject<UWeaponDataAsset>(Player);
	PrimaryData->WeaponMesh = NewObject<UStaticMesh>(Player);
	PrimaryData->AttachSocketName = TEXT("SK_Weapon");
	UWeaponDataAsset* SecondaryData = NewObject<UWeaponDataAsset>(Player);
	SecondaryData->WeaponMesh = NewObject<UStaticMesh>(Player);
	SecondaryData->AttachSocketName = TEXT("TwoHandWeapon");
	SecondaryData->AttachOffset.SetScale3D(FVector(15.0f));
	FindFProperty<FObjectPropertyBase>(Player->GetClass(), TEXT("InitialPrimaryWeaponData"))
		->SetObjectPropertyValue_InContainer(Player, PrimaryData);
	FindFProperty<FObjectPropertyBase>(Player->GetClass(), TEXT("InitialSecondaryWeaponData"))
		->SetObjectPropertyValue_InContainer(Player, SecondaryData);
	Player->FinishSpawning(FTransform::Identity);
	auto* PrimaryMesh = Cast<UStaticMeshComponent>(Player->GetDefaultSubobjectByName(TEXT("PrimaryWeaponMesh")));
	auto* SecondaryMesh = Cast<UStaticMeshComponent>(Player->GetDefaultSubobjectByName(TEXT("SecondaryWeaponMesh")));
	if (!TestNotNull(TEXT("Primary mesh component exists"), PrimaryMesh) ||
		!TestNotNull(TEXT("Secondary mesh component exists"), SecondaryMesh)) return false;
	TestFalse(TEXT("Primary initially shown"), PrimaryMesh->bHiddenInGame);
	TestTrue(TEXT("Secondary initially hidden"), SecondaryMesh->bHiddenInGame);
	Player->ShowWeaponInSlot(EWeaponSlot::Secondary);
	TestTrue(TEXT("Primary hidden when using secondary"), PrimaryMesh->bHiddenInGame);
	TestFalse(TEXT("Secondary shown when selected"), SecondaryMesh->bHiddenInGame);
	TestTrue(TEXT("Secondary uses its own mesh"), SecondaryMesh->GetStaticMesh() == SecondaryData->WeaponMesh);
	TestEqual(TEXT("Missing weapon socket uses shared hand socket"), SecondaryMesh->GetAttachSocketName(), FName(TEXT("SK_Weapon")));
	TestTrue(TEXT("Secondary offset preserved"), SecondaryMesh->GetRelativeTransform().Equals(SecondaryData->AttachOffset));
	Player->InitializePlayer();
	TestFalse(TEXT("Reinitialization keeps secondary visible"), SecondaryMesh->bHiddenInGame);
	Player->ShowWeaponInSlot(EWeaponSlot::Primary);
	TestFalse(TEXT("Switch back shows primary"), PrimaryMesh->bHiddenInGame);
	TestTrue(TEXT("Switch back hides secondary"), SecondaryMesh->bHiddenInGame);
	return !HasAnyErrors();
}
#endif
