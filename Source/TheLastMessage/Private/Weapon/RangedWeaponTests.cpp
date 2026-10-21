#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Player/PlayerAimAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "DataAsset/WeaponDataAsset.h"

#include "Weapon/EnergyMuzzleFlash.h"
#include "Components/AudioComponent.h"
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
	Data->Range = 1000;
	Data->EnergyPerShot = 10;
	FindFProperty<FFloatProperty>(APlayerBase::StaticClass(), TEXT("SharedEnergyRechargeDelay"))->SetPropertyValue_InContainer(Player, 0.5f);
	FindFProperty<FFloatProperty>(APlayerBase::StaticClass(), TEXT("SharedEnergyRechargeRate"))->SetPropertyValue_InContainer(Player, 20.0f);
	Data->ProjectileSpeed = 1000;
	Data->ProjectileClass = AProjectileBase::StaticClass();
	TestTrue(TEXT("Primary ranged equipment"), Player->EquipWeapon(EWeaponSlot::Primary, Data));
	TestTrue(TEXT("Secondary ranged equipment"), Player->EquipWeapon(EWeaponSlot::Secondary, Data));
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	UWeaponSlotAttributeSet* Energy = Player->GetWeaponSlotAttributes();
	UWeaponDataAsset* PlasmaFeedback = LoadObject<UWeaponDataAsset>(nullptr, TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Plasmapistol.DA_Weapon_Plasmapistol"));
	UWeaponDataAsset* RailFeedback = LoadObject<UWeaponDataAsset>(nullptr, TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Railgun.DA_Weapon_Railgun"));
	TestNotNull(TEXT("Plasma weapon data exists"), PlasmaFeedback);
	TestNotNull(TEXT("Railgun weapon data exists"), RailFeedback);
	if (PlasmaFeedback)
	{
		TestTrue(TEXT("Plasma uses energy flash"), PlasmaFeedback->bUseEnergyFlash);
		TestNotNull(TEXT("Plasma attack sound migrated"), PlasmaFeedback->AttackSound.Get());
		TestTrue(TEXT("Flash duration migrated"), PlasmaFeedback->MuzzleFlashDuration > 0.0f && PlasmaFeedback->MuzzleFlashDuration < 0.3f);
	}
	if (RailFeedback)
	{
		TestNotNull(TEXT("Railgun charge sound migrated"), RailFeedback->ChargeSound.Get());
		TestTrue(TEXT("Local charge audio avoids attenuation"), RailFeedback->bLocalChargeSound2D);
		TestTrue(TEXT("Local charge volume multiplier supports user tuning"),
			FMath::IsFinite(RailFeedback->LocalChargeSoundVolumeMultiplier) && RailFeedback->LocalChargeSoundVolumeMultiplier >= 0.0f);
	}
	TestEqual(TEXT("Initial energy"), Energy->GetSharedEnergy(), 100.0f);
	AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>(FVector(300, 0, 200), FRotator::ZeroRotator);
	Enemy->GetCharacterMovement()->DisableMovement();
	const UEnemyAttributeSet* EnemyStats = Enemy->GetAbilitySystemComponent()->GetSet<UEnemyAttributeSet>();
	const float HealthBefore = EnemyStats->GetHealth();
	const double ShotStartTime = World->GetTimeSeconds();
	TestTrue(TEXT("Shot activates without a montage"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	TestEqual(TEXT("Shot costs primary energy once"), Energy->GetSharedEnergy(), 90.0f);
	TestEqual(TEXT("Both slots consume the same pool"), Energy->GetSharedEnergy(), 90.0f);
	TestFalse(TEXT("Animationless safety interval blocks immediate repeated shot"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	Advance(0.4f);
	TestEqual(TEXT("Projectile hits through common damage GE exactly once"), EnemyStats->GetHealth(), HealthBefore - 10.0f);
	TestEqual(TEXT("No recharge before delay"), Energy->GetSharedEnergy(), 90.0f);
	Advance(0.3f);
	// Hit stop intentionally delays gameplay clocks; recharge follows game time, not wall time.
	const float ExpectedRecharge = 90.0f + FMath::Max(0.0, World->GetTimeSeconds() - ShotStartTime - 0.5f) * 20.0f;
	TestTrue(TEXT("Recharge uses only game time after delay"), FMath::IsNearlyEqual(Energy->GetSharedEnergy(), ExpectedRecharge, 1.1f));
	Advance(0.4f);
	TestEqual(TEXT("Recharge stops at maximum"), Energy->GetSharedEnergy(), 100.0f);
	Energy->SetSharedEnergy(5);
	TestFalse(TEXT("Insufficient energy rejects shot"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	TestEqual(TEXT("Rejected shot preserves energy"), Energy->GetSharedEnergy(), 5.0f);
	TestFalse(TEXT("Insufficient shared energy also blocks secondary"), ASC->TryActivateAbilityByClass(UGA_AttackSecondary::StaticClass()));
	TestEqual(TEXT("Rejected secondary preserves shared energy"), Energy->GetSharedEnergy(), 5.0f);
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
	Energy->SetSharedEnergy(100);
	TestTrue(TEXT("Shot toward wall activates"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	Advance(0.4f);
	TestEqual(TEXT("Wall prevents damage"), EnemyStats->GetHealth(), BeforeWallShot);
	Wall->Destroy();
	Data->bSupportsChargeAttack = true;
	FindFProperty<FFloatProperty>(APlayerBase::StaticClass(), TEXT("SharedEnergyRechargeRate"))->SetPropertyValue_InContainer(Player, 0.0f);
	Data->MaxChargeTime = 0.2f;
	Data->MaxChargeDamageMultiplier = 2;
	Data->ChargeStartDelay = 0.1f;
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UGA_AttackPrimary::StaticClass());
	Spec->InputPressed = true;
	TestTrue(TEXT("Short press starts waiting for release"), ASC->TryActivateAbility(Spec->Handle));
	Advance(0.05f);
	TestFalse(TEXT("Short press does not show charge HUD"), Player->IsChargingWeapon());
	TestEqual(TEXT("Short press has zero charge"), Player->GetWeaponChargeRatio(), 0.0f);
	const float BeforeTapEnergy = Energy->GetSharedEnergy();
	ASC->AbilitySpecInputReleased(*Spec);
	TestEqual(TEXT("Tap consumes one normal shot cost"), Energy->GetSharedEnergy(), BeforeTapEnergy - Data->EnergyPerShot);
	Advance(0.4f);
	TestEqual(TEXT("Tap applies normal damage without partial charge bonus"), EnemyStats->GetHealth(), BeforeWallShot - 10.0f);
	TestFalse(TEXT("Released tap cannot start delayed charge"), Player->IsChargingWeapon());
	Spec->InputPressed = true;
	const float BeforeFullChargeHealth = EnemyStats->GetHealth();
	const float BeforeChargeEnergy = Energy->GetSharedEnergy();
	TestTrue(TEXT("Charge starts"), ASC->TryActivateAbility(Spec->Handle));
	TestFalse(TEXT("Charge presentation waits for hold threshold"), Player->IsChargingWeapon());
	Advance(0.11f);
	TestTrue(TEXT("Charge is exposed to HUD"), Player->IsChargingWeapon());
	TestTrue(TEXT("Threshold waiting time is excluded from charge progress"), Player->GetWeaponChargeRatio() < 0.1f);
	Advance(0.2f);
	TestEqual(TEXT("Holding charge does not spend energy"), Energy->GetSharedEnergy(), BeforeChargeEnergy);
	ASC->AbilitySpecInputReleased(*Spec);
	TestFalse(TEXT("Released charge hides HUD presentation"), Player->IsChargingWeapon());
	Advance(0.4f);
	TestEqual(TEXT("Charged shot applies multiplier"), EnemyStats->GetHealth(), BeforeFullChargeHealth - 20.0f);
	Spec->InputPressed = true;
	TestTrue(TEXT("Cancelable precharge hold starts"), ASC->TryActivateAbility(Spec->Handle));
	const float BeforePrechargeCancel = Energy->GetSharedEnergy();
	ASC->CancelAbilityHandle(Spec->Handle);
	Advance(0.2f);
	TestFalse(TEXT("Cancel before threshold prevents late charge HUD/audio"), Player->IsChargingWeapon());
	TestEqual(TEXT("Cancel before threshold does not consume energy"), Energy->GetSharedEnergy(), BeforePrechargeCancel);
	// Legacy switching tests below explicitly exercise the zero-delay configuration.
	Data->ChargeStartDelay = 0.0f;
	Spec->InputPressed = true;
	TestTrue(TEXT("Another charge starts"), ASC->TryActivateAbility(Spec->Handle));
	// Audio playback is disabled in this world; inject a tracked component to verify cancellation cleanup.
	UAudioComponent* TrackedChargeAudio = NewObject<UAudioComponent>(Player);
	TrackedChargeAudio->RegisterComponent();
	FObjectPropertyBase* ChargeAudioProperty = FindFProperty<FObjectPropertyBase>(APlayerBase::StaticClass(), TEXT("WeaponChargeAudio"));
	ChargeAudioProperty->SetObjectPropertyValue_InContainer(Player, TrackedChargeAudio);
	const float BeforeCancel = Energy->GetSharedEnergy();
	ASC->CancelAbilityHandle(Spec->Handle);
	TestNull(TEXT("Canceled attack clears the charge audio component"), ChargeAudioProperty->GetObjectPropertyValue_InContainer(Player));
	TestFalse(TEXT("Canceled charge hides HUD presentation"), Player->IsChargingWeapon());
	TestEqual(TEXT("Canceled charge has no cost"), Energy->GetSharedEnergy(), BeforeCancel);
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
	Data->ChargeStartDelay = 0.1f;
	SendInput(WeaponGameplayTags::Input_Primary, 1);
	Advance(0.03f);
	SendInput(WeaponGameplayTags::Input_Secondary, 1);
	TestFalse(TEXT("Opposite slot cancels previous hold before charge begins"), Spec->IsActive());
	TestFalse(TEXT("New slot waits for its own hold threshold"), Player->IsChargingWeapon());
	Advance(0.06f);
	SendInput(WeaponGameplayTags::Input_Secondary, 1);
	Advance(0.05f);
	TestEqual(TEXT("Repeated same input does not restart hold threshold"), Player->GetChargingWeaponSlot(), EWeaponSlot::Secondary);
	SendInput(WeaponGameplayTags::Input_Primary, 0);
	FGameplayAbilitySpec* SecondarySpec = ASC->FindAbilitySpecFromClass(UGA_AttackSecondary::StaticClass());
	ASC->CancelAbilityHandle(SecondarySpec->Handle);
	SendInput(WeaponGameplayTags::Input_Secondary, 0);
	Data->ChargeStartDelay = 0.0f;
	const float PrimaryBeforeSwitch = Energy->GetSharedEnergy();
	const float SecondaryBeforeSwitch = Energy->GetSharedEnergy();
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
	TestEqual(TEXT("Switches preserve primary energy"), Energy->GetSharedEnergy(), PrimaryBeforeSwitch);
	TestEqual(TEXT("Switches preserve secondary energy"), Energy->GetSharedEnergy(), SecondaryBeforeSwitch);
	SendInput(WeaponGameplayTags::Input_Primary, 0);
	TestFalse(TEXT("New charge releases normally"), Player->IsChargingWeapon());
	TestEqual(TEXT("Only released primary attack consumes primary energy"), Energy->GetSharedEnergy(), PrimaryBeforeSwitch - Data->EnergyPerShot);
	TestEqual(TEXT("Only the released primary consumes shared energy"), Energy->GetSharedEnergy(), PrimaryBeforeSwitch - Data->EnergyPerShot);
	Advance(0.4f);
	UWeaponDataAsset* SecondaryData = DuplicateObject<UWeaponDataAsset>(Data, Player);
	SecondaryData->bSupportsChargeAttack = false;
	TestTrue(TEXT("Equip uncharged secondary"), Player->EquipWeapon(EWeaponSlot::Secondary, SecondaryData));
	SendInput(WeaponGameplayTags::Input_Primary, 1);
	const float BeforeImmediateSwitch = Energy->GetSharedEnergy();
	SendInput(WeaponGameplayTags::Input_Secondary, 1);
	TestFalse(TEXT("Switch to uncharged attack clears charge UI"), Player->IsChargingWeapon());
	TestEqual(TEXT("Immediate switch consumes one shared cost"), Energy->GetSharedEnergy(), BeforeImmediateSwitch - SecondaryData->EnergyPerShot);
	TestFalse(TEXT("Uncharged attack does not apply slot cooldown"), ASC->HasMatchingGameplayTag(WeaponGameplayTags::Cooldown_Secondary));
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
	TestTrue(TEXT("Animationless weapon can fire again"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	Advance(0.2f);
	Data->bSupportsChargeAttack = true;
	Player->GetCombatAttributes()->SetChargeTimeMultiplier(0.8f);
	Energy->SetSharedEnergy(100);
	// Simulate the attack state held by the preceding montage, using the real input route.
	ASC->AddLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	SendInput(WeaponGameplayTags::Input_Primary, 1);
	TestFalse(TEXT("Charge waits while previous animation is active"), Player->IsChargingWeapon());
	ASC->RemoveLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	Advance(0.02f);
	TestTrue(TEXT("Held press starts accelerated charge after animation ends"), Player->IsChargingWeapon());
	ASC->CancelAbilityHandle(Spec->Handle);
	SendInput(WeaponGameplayTags::Input_Primary, 0);
	ASC->AddLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	SendInput(WeaponGameplayTags::Input_Primary, 1);
	SendInput(WeaponGameplayTags::Input_Primary, 0);
	ASC->RemoveLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	Advance(0.02f);
	TestFalse(TEXT("Release before animation ends cancels queued charge"), Player->IsChargingWeapon());
	ASC->AddLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	SendInput(WeaponGameplayTags::Input_Primary, 1);
	Player->ClearPendingAttack();
	ASC->RemoveLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	Advance(0.02f);
	TestFalse(TEXT("Interrupted attack discards queued input"), Player->IsChargingWeapon());
	USkeletalMesh* PlayerMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Test_Jihong/Blueprints/Player/Robbie_01.Robbie_01"));
	TestNotNull(TEXT("Player mesh for ranged aim exists"), PlayerMesh);
	if (PlayerMesh)
	{
		Player->GetMesh()->SetSkeletalMeshAsset(PlayerMesh);
		Player->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Player->GetMesh()->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr,
			TEXT("/Game/Test_Jihong/Animations/Robbie_01_AnimBP.Robbie_01_AnimBP_C")));
		Advance(0.1f);
		UPlayerAimAnimInstance* AimAnim = Cast<UPlayerAimAnimInstance>(Player->GetMesh()->GetPostProcessInstance());
		TestNotNull(TEXT("Ranged aim post-process is installed"), AimAnim);
		if (AimAnim)
		{
			Advance(0.2f);
			TestTrue(TEXT("Gun-ready idle is active before any shot"), AimAnim->RangedReadyAlpha > 0.9f);
			TestNotNull(TEXT("Dedicated ranged idle sequence is selected"), AimAnim->RangedReadySequence.Get());
			const FQuat NeutralSpine = Player->GetMesh()->GetSocketQuaternion(TEXT("spine_01"));
			SendInput(WeaponGameplayTags::Input_Primary, 1);
			Player->UpdateAimFromWorldRay(FVector(-1000, 500, 1200), -FVector::YAxisVector);
			Advance(0.3f);
			TestTrue(TEXT("Charging faces left toward cursor"), Player->GetActorForwardVector().X < -0.9f);
			TestTrue(TEXT("Ranged aim is enabled during charge"), AimAnim->RangedAimAlpha > 0.9f);
			TestTrue(TEXT("Upward aim produces upper-body rotation"), AimAnim->RangedAimRotation.GetManhattanDistance(FRotator::ZeroRotator) > 20.0f);
			TestTrue(TEXT("Post-process actually modifies the spine pose"), NeutralSpine.AngularDistance(Player->GetMesh()->GetSocketQuaternion(TEXT("spine_01"))) > 0.15f);
			ASC->CancelAbilityHandle(Spec->Handle);
			Advance(0.4f);
			TestTrue(TEXT("Canceled charge restores neutral upper-body blend"), AimAnim->RangedAimAlpha < 0.05f);
			TestTrue(TEXT("After charge cancellation gun stays raised in idle"), AimAnim->RangedReadyAlpha > 0.9f);
			TestTrue(TEXT("Movement-facing behavior is restored"), Player->GetCharacterMovement()->bOrientRotationToMovement);
			Data->AttackMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Test_Jihong/Animations/LightRange/AM_PlayerAttack_LightRange"));
			TestTrue(TEXT("Firing montage can start from ready idle"), Player->GetMesh()->GetAnimInstance()->Montage_Play(Data->AttackMontage) > 0.0f);
			Advance(0.3f);
			TestTrue(TEXT("Firing animation takes priority over ready idle"), AimAnim->RangedReadyAlpha < 0.4f);
			Advance(0.8f);
			TestTrue(TEXT("Gun-ready idle returns after firing"), AimAnim->RangedReadyAlpha > 0.9f);
			Data->AttackMontage = nullptr;
		}
	}
	AProjectileBase* VisualProjectile = World->SpawnActor<AProjectileBase>(FVector(0, 0, 200), FRotator::ZeroRotator);
	Data->ProjectileVisualScale = FVector(0.6f, 0.15f, 0.15f);
	VisualProjectile->ConfigureVisuals(Data);
	TestEqual(TEXT("Projectile visual scale follows weapon data"), VisualProjectile->FindComponentByClass<UStaticMeshComponent>()->GetRelativeScale3D(), Data->ProjectileVisualScale);
	TestEqual(TEXT("Larger projectile visual preserves collision radius"), VisualProjectile->FindComponentByClass<USphereComponent>()->GetUnscaledSphereRadius(), 5.0f);
	VisualProjectile->Destroy();
	UClass* PlasmaBP = LoadClass<AProjectileBase>(nullptr, TEXT("/Game/Test_Jihong/Blueprints/Projectile/BP_Projectile_Plasma.BP_Projectile_Plasma_C"));
	UClass* RailgunBP = LoadClass<AProjectileBase>(nullptr, TEXT("/Game/Test_Jihong/Blueprints/Projectile/BP_Projectile_Railgun.BP_Projectile_Railgun_C"));
	TestNotNull(TEXT("Plasma projectile Blueprint exists"), PlasmaBP);
	TestNotNull(TEXT("Railgun projectile Blueprint exists"), RailgunBP);
	TestEqual(TEXT("Default light ranged weapon resolves editable projectile BP"), Player->GetAttackProjectileClass(Data).Get(), PlasmaBP);
	AProjectileBase* RangeOverrideProjectile = World->SpawnActor<AProjectileBase>(FVector(0, 0, 200), FRotator::ZeroRotator);
	FindFProperty<FBoolProperty>(AProjectileBase::StaticClass(), TEXT("bOverrideWeaponRange"))->SetPropertyValue_InContainer(RangeOverrideProjectile, true);
	FindFProperty<FFloatProperty>(AProjectileBase::StaticClass(), TEXT("TravelRange"))->SetPropertyValue_InContainer(RangeOverrideProjectile, 75.0f);
	TestTrue(TEXT("BP travel range override initializes"), RangeOverrideProjectile->InitializeProjectile(Player, DamageEffect, 10, 1000, 1000));
	RangeOverrideProjectile->LaunchProjectile(FVector::ForwardVector);
	TestTrue(TEXT("BP travel range replaces weapon data range"), FMath::IsNearlyEqual(RangeOverrideProjectile->GetLifeSpan(), 0.075f));
	Advance(0.1f);
	TestFalse(TEXT("Projectile expires at BP travel range"), IsValid(RangeOverrideProjectile));
	// Use a transient mesh so the test never modifies the user's weapon assets.
	UStaticMesh* SocketMesh = DuplicateObject<UStaticMesh>(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")), Player);
	UStaticMeshSocket* Muzzle = NewObject<UStaticMeshSocket>(SocketMesh);
	Muzzle->SocketName = TEXT("Muzzle");
	Muzzle->RelativeLocation = FVector(100, 0, 0);
	SocketMesh->AddSocket(Muzzle);
	Data->WeaponMesh = SocketMesh;
	Data->AttachSocketName = TEXT("spine_03");
	Data->bSupportsChargeAttack = false;
	Data->AttackMontage = nullptr;
	Data->EnergyPerShot = 0;
	Player->EquipWeapon(EWeaponSlot::Primary, Data);
	Player->EquipWeapon(EWeaponSlot::Secondary, Data);
	Player->SetActorRotation(FRotator::ZeroRotator);
	Player->UpdateAimFromWorldRay(FVector(2000, 500, 200), -FVector::YAxisVector);
	for (TActorIterator<AProjectileBase> It(World); It; ++It) It->Destroy();
	TestTrue(TEXT("Shot from muzzle activates"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	const FVector MuzzleLocation = Player->GetWeaponMuzzleLocation(EWeaponSlot::Primary);
	TestFalse(TEXT("Muzzle is distinct from capsule center"), MuzzleLocation.Equals(Player->GetActorLocation(), 1.0f));
	int32 MuzzleShotCount = 0;
	for (TActorIterator<AProjectileBase> It(World); It; ++It)
	{
		++MuzzleShotCount;
		TestTrue(TEXT("Projectile starts exactly at muzzle socket"), It->GetActorLocation().Equals(MuzzleLocation, 0.01f));
		TestTrue(TEXT("Projectile aims from muzzle toward cursor"), It->FindComponentByClass<UProjectileMovementComponent>()->Velocity.GetSafeNormal().Equals(
			(Player->GetAimTargetLocation() - MuzzleLocation).GetSafeNormal(), 0.001f));
		It->Destroy();
	}
	TestEqual(TEXT("Muzzle shot creates one projectile"), MuzzleShotCount, 1);
	int32 EnergyFlashCount = 0;
	for (TActorIterator<AEnergyMuzzleFlash> It(World); It; ++It)
	{
		++EnergyFlashCount;
		TestTrue(TEXT("Energy flash starts at the actual muzzle"), It->GetActorLocation().Equals(MuzzleLocation, 0.01f));
		TestEqual(TEXT("Energy flash is attached to the Muzzle socket"), It->GetRootComponent()->GetAttachSocketName(), FName(TEXT("Muzzle")));
	}
	TestEqual(TEXT("A successful shot creates one energy flash"), EnergyFlashCount, 1);
	TestTrue(TEXT("Secondary muzzle shot activates"), ASC->TryActivateAbilityByClass(UGA_AttackSecondary::StaticClass()));
	MuzzleShotCount = 0;
	for (TActorIterator<AProjectileBase> It(World); It; ++It)
	{
		++MuzzleShotCount;
		TestTrue(TEXT("Secondary uses its own muzzle socket"), It->GetActorLocation().Equals(Player->GetWeaponMuzzleLocation(EWeaponSlot::Secondary), 0.01f));
		It->Destroy();
	}
	TestEqual(TEXT("Secondary muzzle creates one projectile"), MuzzleShotCount, 1);
	Advance(0.2f);
	int32 LingeringFlashes = 0;
	for (TActorIterator<AEnergyMuzzleFlash> It(World); It; ++It) ++LingeringFlashes;
	TestEqual(TEXT("Energy flashes expire after their short pulse"), LingeringFlashes, 0);
	Player->ShowWeaponInSlot(EWeaponSlot::Primary);
	const FVector ClearMuzzle = Player->GetWeaponMuzzleLocation(EWeaponSlot::Primary);
	AActor* MuzzleWall = World->SpawnActor<AActor>();
	UBoxComponent* MuzzleWallBox = NewObject<UBoxComponent>(MuzzleWall);
	MuzzleWall->SetRootComponent(MuzzleWallBox); MuzzleWall->AddInstanceComponent(MuzzleWallBox);
	MuzzleWallBox->SetBoxExtent(FVector(10));
	MuzzleWallBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MuzzleWallBox->SetCollisionResponseToAllChannels(ECR_Block);
	MuzzleWallBox->RegisterComponent();
	MuzzleWall->SetActorLocation((Player->GetActorLocation() + ClearMuzzle) * 0.5f);
	TestTrue(TEXT("Obstructed muzzle shot activates"), ASC->TryActivateAbilityByClass(UGA_AttackPrimary::StaticClass()));
	for (TActorIterator<AProjectileBase> It(World); It; ++It)
	{
		TestTrue(TEXT("Projectile cannot spawn beyond wall at muzzle"), FVector::Dist(It->GetActorLocation(), Player->GetActorLocation()) < FVector::Dist(ClearMuzzle, Player->GetActorLocation()));
		It->Destroy();
	}
	MuzzleWall->Destroy();
	Advance(0.3f);
	UWeaponDataAsset* RailAsset = LoadObject<UWeaponDataAsset>(nullptr, TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Railgun.DA_Weapon_Railgun"));
	TestNotNull(TEXT("Actual railgun asset exists"), RailAsset);
	if (RailAsset)
	{
		UWeaponDataAsset* Rail = DuplicateObject<UWeaponDataAsset>(RailAsset, Player);
		Rail->EnergyPerShot = 0;
		Rail->bSupportsChargeAttack = false;
		Rail->AttackMontage = nullptr;
		TestTrue(TEXT("Actual railgun equips"), Player->EquipWeapon(EWeaponSlot::Secondary, Rail));
		TestTrue(TEXT("Actual railgun fires"), ASC->TryActivateAbilityByClass(UGA_AttackSecondary::StaticClass()));
		int32 RailFlashes = 0;
		for (TActorIterator<AEnergyMuzzleFlash> It(World); It; ++It)
		{
			++RailFlashes;
			TestTrue(TEXT("Actual railgun pulse starts at its muzzle"), It->GetActorLocation().Equals(Player->GetWeaponMuzzleLocation(EWeaponSlot::Secondary), 0.01f));
			It->Tick(0.25f);
			TestFalse(TEXT("Railgun pulse survives its first slow frame"), It->IsActorBeingDestroyed());
		}
		TestEqual(TEXT("Actual railgun creates one pulse"), RailFlashes, 1);
		Advance(0.3f);
		int32 Remaining = 0;
		for (TActorIterator<AEnergyMuzzleFlash> It(World); It; ++It) ++Remaining;
		TestEqual(TEXT("Railgun pulse still expires"), Remaining, 0);
	}
	// Maximum changes clamp the single shared resource.
	Energy->SetSharedMaxEnergy(15);
	TestTrue(TEXT("Maximum reduction clamps energy"), Energy->GetSharedEnergy() <= 15.0f);
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
