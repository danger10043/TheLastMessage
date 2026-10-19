#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "DataAsset/WeaponDataAsset.h"
#include "GAS/Abilities/GA_AttackPrimary.h"
#include "GAS/Abilities/GA_AttackSecondary.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangedWeaponTest, "TheLastMessage.Weapon.RangedLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRangedWeaponTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	auto Advance = [World](float Seconds)
	{
		for (int32 Frame = 0; Frame < FMath::RoundToInt(Seconds / 0.01f); ++Frame)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 0.01f);
		}
	};
	APlayerBase* Player = World->SpawnActor<APlayerBase>(FVector(0, 0, 200), FRotator::ZeroRotator);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->Possess(Player);
	Player->GetCharacterMovement()->DisableMovement();
	Player->GetCombatAttributes()->SetMaxHealth(100);
	Player->GetCombatAttributes()->SetHealth(100);
	UClass* DamageEffect = LoadClass<UGameplayEffect>(nullptr,
		TEXT("/Game/Test_Inhyeok/GAS/Effect/GE_BP_CommonIncomingDamage.GE_BP_CommonIncomingDamage_C"));
	TestNotNull(TEXT("Common damage GE exists"), DamageEffect);
	FindFProperty<FClassProperty>(APlayerBase::StaticClass(), TEXT("WeaponDamageEffectClass"))->SetObjectPropertyValue_InContainer(Player, DamageEffect);
	UWeaponDataAsset* Data = NewObject<UWeaponDataAsset>(Player);
	Data->WeaponType = EWeaponType::LightRanged;
	Data->AttackPower = 10;
	Data->RPM = 600;
	Data->Range = 1000;
	Data->MaxEnergy = 100;
	Data->EnergyPerShot = 10;
	Data->EnergyRechargeDelay = 0.5f;
	Data->EnergyRechargeRate = 20;
	Data->ProjectileSpeed = 1000;
	Data->ProjectileClass = AProjectileBase::StaticClass();
	TestTrue(TEXT("Primary ranged equipment"), Player->EquipWeapon(EWeaponSlot::Primary, Data));
	TestTrue(TEXT("Secondary ranged equipment"), Player->EquipWeapon(EWeaponSlot::Secondary, Data));
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	UWeaponSlotAttributeSet* Energy = Player->GetWeaponSlotAttributes();
	TestEqual(TEXT("Initial energy"), Energy->GetPrimaryEnergy(), 100.0f);
	AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>(FVector(300, 0, 200), FRotator::ZeroRotator);
	Enemy->GetCharacterMovement()->DisableMovement();
	const UEnemyAttributeSet* EnemyStats = Enemy->GetAbilitySystemComponent()->GetSet<UEnemyAttributeSet>();
	const float HealthBefore = EnemyStats->GetHealth();
	TestTrue(TEXT("Shot activates without a montage"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	TestEqual(TEXT("Shot costs primary energy once"), Energy->GetPrimaryEnergy(), 90.0f);
	TestEqual(TEXT("Other slot unchanged"), Energy->GetSecondaryEnergy(), 100.0f);
	TestFalse(TEXT("RPM cooldown blocks immediate repeated shot"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	Advance(0.4f);
	TestEqual(TEXT("Projectile hits through common damage GE exactly once"), EnemyStats->GetHealth(), HealthBefore - 10.0f);
	TestEqual(TEXT("No recharge before delay"), Energy->GetPrimaryEnergy(), 90.0f);
	Advance(0.3f);
	TestTrue(TEXT("Recharge uses only time after delay"), FMath::IsNearlyEqual(Energy->GetPrimaryEnergy(), 94.0f, 1.1f));
	Advance(0.4f);
	TestEqual(TEXT("Recharge stops at maximum"), Energy->GetPrimaryEnergy(), 100.0f);
	Energy->SetPrimaryEnergy(5);
	TestFalse(TEXT("Insufficient energy rejects shot"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	TestEqual(TEXT("Rejected shot preserves energy"), Energy->GetPrimaryEnergy(), 5.0f);
	TestTrue(TEXT("Secondary can still fire"), ASC->TryActivateAbilityByClass(UGA_AttackSecondary::StaticClass()));
	TestEqual(TEXT("Secondary has independent cost"), Energy->GetSecondaryEnergy(), 90.0f);
	Advance(0.4f);
	// A real blocking component must stop a new shot before it reaches the enemy.
	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Wall->AddInstanceComponent(Box);
	Box->SetBoxExtent(FVector(10, 100, 100));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	Wall->SetActorLocation(FVector(150, 0, 200));
	const float BeforeWallShot = EnemyStats->GetHealth();
	Energy->SetPrimaryEnergy(100);
	TestTrue(TEXT("Shot toward wall activates"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	Advance(0.4f);
	TestEqual(TEXT("Wall prevents damage"), EnemyStats->GetHealth(), BeforeWallShot);
	Wall->Destroy();
	Data->bSupportsChargeAttack = true;
	Data->EnergyRechargeRate = 0;
	Data->MaxChargeTime = 0.2f;
	Data->MaxChargeDamageMultiplier = 2;
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UGA_AttackPrimary::StaticClass());
	Spec->InputPressed = true;
	const float BeforeChargeEnergy = Energy->GetPrimaryEnergy();
	TestTrue(TEXT("Charge starts"), ASC->TryActivateAbility(Spec->Handle));
	TestTrue(TEXT("Charge is exposed to HUD"), Player->IsChargingWeapon());
	Advance(0.2f);
	TestEqual(TEXT("Holding charge does not spend energy"), Energy->GetPrimaryEnergy(), BeforeChargeEnergy);
	ASC->AbilitySpecInputReleased(*Spec);
	TestFalse(TEXT("Released charge hides HUD presentation"), Player->IsChargingWeapon());
	Advance(0.4f);
	TestEqual(TEXT("Charged shot applies multiplier"), EnemyStats->GetHealth(), BeforeWallShot - 20.0f);
	Spec->InputPressed = true;
	TestTrue(TEXT("Another charge starts"), ASC->TryActivateAbility(Spec->Handle));
	const float BeforeCancel = Energy->GetPrimaryEnergy();
	ASC->CancelAbilityHandle(Spec->Handle);
	TestFalse(TEXT("Canceled charge hides HUD presentation"), Player->IsChargingWeapon());
	TestEqual(TEXT("Canceled charge has no cost"), Energy->GetPrimaryEnergy(), BeforeCancel);
	TestFalse(TEXT("Cancel clears attack state"), ASC->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking));
	// Exercise the authoritative input route used by local input and the client RPC.
	auto SendInput = [Player](FGameplayTag Tag, uint8 State)
	{
		struct FInputParameters
		{
			FGameplayTag InputTag;
			uint8 InputState;
			FVector_NetQuantize AimPoint;
		};
		FInputParameters Input{Tag, State, FVector(2000, 0, 200)};
		Player->ProcessEvent(Player->FindFunctionChecked(TEXT("ServerAbilityInput")), &Input);
	};
	const float PrimaryBeforeSwitch = Energy->GetPrimaryEnergy();
	const float SecondaryBeforeSwitch = Energy->GetSecondaryEnergy();
	SendInput(WeaponGameplayTags::Input_Primary, 1);
	TestEqual(TEXT("Primary input starts charging"), Player->GetChargingWeaponSlot(), EWeaponSlot::Primary);
	Advance(0.1f);
	SendInput(WeaponGameplayTags::Input_Secondary, 1);
	TestEqual(TEXT("Opposite input switches charge to secondary"), Player->GetChargingWeaponSlot(), EWeaponSlot::Secondary);
	TestFalse(TEXT("Old primary ability was canceled"), Spec->IsActive());
	TestTrue(TEXT("New slot starts charge from zero"), FMath::IsNearlyZero(Player->GetWeaponChargeRatio()));
	TestFalse(TEXT("Canceled primary charge has no cooldown"), ASC->HasMatchingGameplayTag(WeaponGameplayTags::Cooldown_Primary));
	Advance(0.1f);
	const float SecondaryCharge = Player->GetWeaponChargeRatio();
	SendInput(WeaponGameplayTags::Input_Secondary, 1);
	TestEqual(TEXT("Same slot input preserves charge"), Player->GetWeaponChargeRatio(), SecondaryCharge);
	SendInput(WeaponGameplayTags::Input_Primary, 0);
	TestEqual(TEXT("Releasing old primary key preserves secondary charge"), Player->GetChargingWeaponSlot(), EWeaponSlot::Secondary);
	SendInput(WeaponGameplayTags::Input_Primary, 1);
	TestEqual(TEXT("Charge also switches back to primary"), Player->GetChargingWeaponSlot(), EWeaponSlot::Primary);
	TestFalse(TEXT("Canceled secondary charge has no cooldown"), ASC->HasMatchingGameplayTag(WeaponGameplayTags::Cooldown_Secondary));
	SendInput(WeaponGameplayTags::Input_Secondary, 0);
	TestEqual(TEXT("Releasing old secondary key preserves primary charge"), Player->GetChargingWeaponSlot(), EWeaponSlot::Primary);
	TestEqual(TEXT("Switches preserve primary energy"), Energy->GetPrimaryEnergy(), PrimaryBeforeSwitch);
	TestEqual(TEXT("Switches preserve secondary energy"), Energy->GetSecondaryEnergy(), SecondaryBeforeSwitch);
	SendInput(WeaponGameplayTags::Input_Primary, 0);
	TestFalse(TEXT("New charge releases normally"), Player->IsChargingWeapon());
	TestEqual(TEXT("Only released primary attack consumes primary energy"), Energy->GetPrimaryEnergy(), PrimaryBeforeSwitch - Data->EnergyPerShot);
	TestEqual(TEXT("Canceled secondary never consumes energy"), Energy->GetSecondaryEnergy(), SecondaryBeforeSwitch);
	Advance(0.4f);
	UWeaponDataAsset* SecondaryData = DuplicateObject<UWeaponDataAsset>(Data, Player);
	SecondaryData->bSupportsChargeAttack = false;
	TestTrue(TEXT("Equip uncharged secondary"), Player->EquipWeapon(EWeaponSlot::Secondary, SecondaryData));
	SendInput(WeaponGameplayTags::Input_Primary, 1);
	const float BeforeImmediateSwitch = Energy->GetPrimaryEnergy();
	SendInput(WeaponGameplayTags::Input_Secondary, 1);
	TestFalse(TEXT("Switch to uncharged attack clears charge UI"), Player->IsChargingWeapon());
	TestEqual(TEXT("Immediate switch preserves canceled slot energy"), Energy->GetPrimaryEnergy(), BeforeImmediateSwitch);
	TestEqual(TEXT("Uncharged secondary fires on switch"), Energy->GetSecondaryEnergy(), SecondaryData->MaxEnergy - SecondaryData->EnergyPerShot);
	TestTrue(TEXT("Uncharged switched attack applies its cooldown"), ASC->HasMatchingGameplayTag(WeaponGameplayTags::Cooldown_Secondary));
	SendInput(WeaponGameplayTags::Input_Primary, 0);
	SendInput(WeaponGameplayTags::Input_Secondary, 0);
	Data->bSupportsChargeAttack = false;
	Enemy->SetActorLocation(FVector(1500, 0, 200));
	const float BeforeRangeTest = EnemyStats->GetHealth();
	TestTrue(TEXT("Range test shot activates"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	Advance(1.6f);
	TestEqual(TEXT("Target beyond maximum range is unharmed"), EnemyStats->GetHealth(), BeforeRangeTest);
	int32 RemainingProjectiles = 0;
	for (TActorIterator<AProjectileBase> It(World); It; ++It) ++RemainingProjectiles;
	TestEqual(TEXT("Expired projectiles are removed"), RemainingProjectiles, 0);
	Data->RPM = 0;
	TestFalse(TEXT("Invalid RPM rejects firing"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	// Maximum changes clamp the active slot without touching the other slot.
	Energy->SetPrimaryMaxEnergy(15);
	TestTrue(TEXT("Maximum reduction clamps energy"), Energy->GetPrimaryEnergy() <= 15.0f);
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
