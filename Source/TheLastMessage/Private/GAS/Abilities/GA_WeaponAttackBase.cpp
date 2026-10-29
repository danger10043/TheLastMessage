#include "Weapon/MeleeSweep.h"
#include "Animation/AnimNotifyState_MeleeTrail.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GAS/Abilities/GA_WeaponAttackBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Boss/BossMonster.h"
#include "Character/CombatDamageLibrary.h"
#include "Interface/AttackReceiverInterface.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "GAS/Effects/GE_WeaponEnergyCost.h"
#include "Projectile/ProjectileBase.h"
#include "GAS/WeaponGameplayTags.h"
#include "Weapon/WeaponBase.h"
#include "UObject/StrongObjectPtr.h"

#if ENABLE_DRAW_DEBUG
static TAutoConsoleVariable<int32> CVarDebugMelee(
	TEXT("tlm.DebugMelee"), 0, TEXT("Draw melee sweep volumes for one second: 0=off, 1=on."));
#endif

UGA_WeaponAttackBase::UGA_WeaponAttackBase()
{
	// 무기·차지 시간·명중 목록을 플레이어마다 독립적으로 보관한다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.AddTag(WeaponGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_HitReacting);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Grappling);
	CostEffectClass = UGE_WeaponEnergyCost::StaticClass();
}

bool UGA_WeaponAttackBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWeaponBase* Weapon = Player ? Player->GetWeaponInSlot(WeaponSlot) : nullptr;
	const UWeaponDataAsset* Data = IsValid(Weapon) ? Weapon->WeaponData.Get() : nullptr;
	if (!Player || Player->IsDead() || !IsValid(Data) || !FMath::IsFinite(Data->AttackPower) || Data->AttackPower <= 0.0f) return false;
	if (!Data->AttackMontage && Weapon->NextAttackAllowedTime > GetWorld()->GetTimeSeconds()) return false;
	if (Data->AttackMontage && !Player->GetMesh()->GetAnimInstance()) return false;
	if (Data->WeaponType == EWeaponType::LightRanged || Data->WeaponType == EWeaponType::HeavyRanged)
	{
		const TSubclassOf<AProjectileBase> ProjectileClass = Player->GetAttackProjectileClass(Data);
		return ProjectileClass && !ProjectileClass->HasAnyClassFlags(CLASS_Abstract)
			&& FMath::IsFinite(Data->ProjectileSpeed) && Data->ProjectileSpeed > 0.0f
			&& FMath::IsFinite(Data->Range) && Data->Range > 0.0f
			&& FMath::IsFinite(Data->EnergyPerShot) && Data->EnergyPerShot >= 0.0f;
	}
	return IsValid(Data->AttackMontage) && Player->GetMesh()->GetAnimInstance()
		&& FMath::IsFinite(Data->MeleeTraceRadius) && Data->MeleeTraceRadius > 0.0f;
}

bool UGA_WeaponAttackBase::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) return false;
	const APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWeaponBase* Weapon = Player ? Player->GetWeaponInSlot(WeaponSlot) : nullptr;
	const UWeaponDataAsset* Data = Weapon ? Weapon->WeaponData.Get() : nullptr;
	if (!Data) return false;
	if (!FMath::IsFinite(Data->EnergyPerShot) || Data->EnergyPerShot < 0.0f) return false;
	if (Data->EnergyPerShot == 0.0f) return true;
	return CostEffectClass
		&& Player->GetAbilitySystemComponent()->GetNumericAttribute(UWeaponSlotAttributeSet::GetSharedEnergyAttribute()) >= Data->EnergyPerShot;
}

void UGA_WeaponAttackBase::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	if (!Data || Data->EnergyPerShot <= 0.0f) return;
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		CostEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(WeaponGameplayTags::Data_EnergyCost, -Data->EnergyPerShot);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
		if (APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr)
		{
			if (Data->EnergyPerShot > 0.0f) Player->NotifyWeaponEnergySpent();
		}
	}
}

const FGameplayTagContainer* UGA_WeaponAttackBase::GetCooldownTags() const
{
	return nullptr;
}

void UGA_WeaponAttackBase::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
 const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
 if (SourceWeapon && SourceWeapon->WeaponData && !SourceWeapon->WeaponData->AttackMontage)
 {
  // Only animationless weapons need a safety interval; preserve it across swaps.
  SourceWeapon->NextAttackAllowedTime = GetWorld()->GetTimeSeconds() + 0.1;
 }
}
void UGA_WeaponAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bFinishingAttack = false;
	bMeleeWindowOpen=false; bHasPreviousMeleeTransform=false;
	bWaitingForRelease = false;
	bChargeStarted = false;
	bAttackExecuted = false;
	ChargeDamageMultiplier = 1.0f;
	CriticalHitDamageMultiplier = 1.0f;
	ChargeElapsedTime = 0.0f;
	ChargeDuration = 0.0f;
	HitActorsThisAttack.Reset();
	RangedAttack = FWeaponRangedAttackSnapshot{};
	MeleeAttack = nullptr;
	RecordedMeleeDamage = 0.0f;
	RemainingBurstShots = 0;
	bAttackMontageFinished = false;
	APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	SourceWeapon = Player ? Player->GetWeaponInSlot(WeaponSlot) : nullptr;
	DamageEffectClass = Player ? Player->GetWeaponDamageEffectClass() : nullptr;
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	if (!DamageEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: assign GE_BP_CommonIncomingDamage to WeaponDamageEffectClass on the player."),
			*GetNameSafe(Player));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!Data)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Player->ShowWeaponInSlot(WeaponSlot);
	Player->StopMovementForAttack();
	if (Data->bSupportsChargeAttack && FMath::IsFinite(Data->MaxChargeTime) && Data->MaxChargeTime > 0.0f)
	{
		// 차지 시작 시 배율을 적용하고, 차지 비율 계산과 UI 게이지에 같은 시간을 사용한다.
		const float ScaledChargeTime = Data->MaxChargeTime * Player->GetCombatAttributes()->GetChargeTimeMultiplier();
		ChargeDuration = FMath::IsFinite(ScaledChargeTime) ? FMath::Max(0.01f, ScaledChargeTime) : Data->MaxChargeTime;
		bWaitingForRelease = true;
		const float Delay = FMath::IsFinite(Data->ChargeStartDelay) ? FMath::Max(0.0f, Data->ChargeStartDelay) : 0.2f;
		if (Delay > 0.0f)
			GetWorld()->GetTimerManager().SetTimer(ChargeStartTimer, this, &UGA_WeaponAttackBase::StartWeaponCharge, Delay, false);
		else StartWeaponCharge();
		// 빠른 클릭으로 활성화 전에 해제된 경우도 한 번만 공격한다.
		const FGameplayAbilitySpec* Spec = GetAbilitySystemComponentFromActorInfo()->FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->InputPressed) InputReleased(Handle, ActorInfo, ActivationInfo);
		return;
	}
	ExecuteAttack();
}

void UGA_WeaponAttackBase::StartWeaponCharge()
{
	APlayerBase* Player = Cast<APlayerBase>(GetAvatarActorFromActorInfo());
	if (!IsWaitingForAttackRelease() || !Player || Player->IsDead()) return;
	bChargeStarted = true;
	ChargeStartTime = GetWorld()->GetTimeSeconds();
	Player->BeginWeaponCharge(WeaponSlot, ChargeDuration);
}

float UGA_WeaponAttackBase::GetChargeRatio() const
{
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	if (!Data || !Data->bSupportsChargeAttack || !bChargeStarted || ChargeDuration <= 0.0f) return 0.0f;
	const float Elapsed = bWaitingForRelease ? float(GetWorld()->GetTimeSeconds() - ChargeStartTime) : ChargeElapsedTime;
	return FMath::Clamp(Elapsed / ChargeDuration, 0.0f, 1.0f);
}

void UGA_WeaponAttackBase::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	if (!IsActive() || bFinishingAttack || !bWaitingForRelease) return;
	GetWorld()->GetTimerManager().ClearTimer(ChargeStartTimer);
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	const float Ratio = GetChargeRatio();
	ChargeElapsedTime = Ratio * ChargeDuration;
	ChargeDamageMultiplier = Data ? FMath::Lerp(1.0f, FMath::Max(1.0f, Data->MaxChargeDamageMultiplier), Ratio) : 1.0f;
	bWaitingForRelease = false;
	if (APlayerBase* Player = Cast<APlayerBase>(GetAvatarActorFromActorInfo())) Player->ClearWeaponCharge(WeaponSlot);
	ExecuteAttack();
}

float UGA_WeaponAttackBase::GetHealthDamageMultiplier() const
{
	const APlayerBase* Player = Cast<APlayerBase>(GetAvatarActorFromActorInfo());
	const UCombatAttributeSet* Attributes = Player ? Player->GetCombatAttributes() : nullptr;
	if (!Attributes || Attributes->GetMaxHealth() <= 0.0f) return 1.0f;

	const float HealthRatio = FMath::Clamp(Attributes->GetHealth() / Attributes->GetMaxHealth(), 0.0f, 1.0f);
	float DamageMultiplier = 1.0f;

	// 높은 체력 조건을 만족하면 안정 출력 회로의 피해 배율을 적용한다.
	if (HealthRatio >= Attributes->GetHighHealthThresholdRatio())
	{
		DamageMultiplier *= FMath::Max(0.0f, Attributes->GetHighHealthDamageMultiplier());
	}
	// 낮은 체력 조건을 만족하면 비상 출력 회로의 피해 배율을 적용한다.
	if (HealthRatio <= Attributes->GetLowHealthThresholdRatio())
	{
		DamageMultiplier *= FMath::Max(0.0f, Attributes->GetLowHealthDamageMultiplier());
	}
	return DamageMultiplier;
}

float UGA_WeaponAttackBase::RollCriticalDamageMultiplier() const
{
	const APlayerBase* Player = Cast<APlayerBase>(GetAvatarActorFromActorInfo());
	const UCombatAttributeSet* Attributes = Player ? Player->GetCombatAttributes() : nullptr;
	if (!Attributes) return 1.0f;

	const float CriticalChance = FMath::Clamp(Attributes->GetCriticalChance(), 0.0f, 1.0f);
	const bool bCriticalHit = CriticalChance >= 1.0f
		|| (CriticalChance > 0.0f && FMath::FRand() < CriticalChance);
	return bCriticalHit ? FMath::Max(1.0f, Attributes->GetCriticalDamageMultiplier()) : 1.0f;
}

bool UGA_WeaponAttackBase::CaptureRangedAttack(APlayerBase* Player,
	TSubclassOf<AProjectileBase> ProjectileClass, const FVector& Direction)
{
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	const UCombatAttributeSet* Attributes = Player ? Player->GetCombatAttributes() : nullptr;
	if (!Data || !Attributes || !ProjectileClass || Direction.ContainsNaN() || Direction.IsNearlyZero()) return false;

	const float ShotCount = Attributes->GetRangedBurstShotCount();
	const float TotalMultiplier = Attributes->GetRangedBurstTotalDamageMultiplier();
	const float Interval = Attributes->GetRangedBurstInterval();
	if (!FMath::IsFinite(ShotCount) || ShotCount < 1.0f || ShotCount >= static_cast<float>(MAX_int32)
		|| !FMath::IsFinite(TotalMultiplier) || TotalMultiplier <= 0.0f
		|| !FMath::IsFinite(Interval) || Interval <= 0.0f) return false;

	FWeaponRangedAttackSnapshot Snapshot;
	Snapshot.WeaponData = SourceWeapon->WeaponData;
	Snapshot.ProjectileClass = ProjectileClass;
	Snapshot.Direction = Direction;
	Snapshot.ShotCount = FMath::RoundToInt(ShotCount);
	Snapshot.ShotInterval = Interval;
	Snapshot.ProjectileSpeed = Data->ProjectileSpeed;
	Snapshot.Range = Data->Range;
	Snapshot.FeedbackStrength = GetImpactFeedbackStrength();
	Snapshot.DamagePerShot = Data->AttackPower * ChargeDamageMultiplier;
	Snapshot.DamagePerShot *= FMath::Max(0.0f, Attributes->GetDamageDealtMultiplier());
	Snapshot.DamagePerShot *= GetHealthDamageMultiplier();
	Snapshot.DamagePerShot *= CriticalHitDamageMultiplier;
	// 차지·치명타를 포함한 이번 공격의 총 피해를 첫 발부터 동일하게 나눈다.
	Snapshot.DamagePerShot = Snapshot.DamagePerShot * TotalMultiplier / Snapshot.ShotCount;
	if (!FMath::IsFinite(Snapshot.DamagePerShot) || Snapshot.DamagePerShot <= 0.0f) return false;

	RangedAttack = Snapshot;
	return true;
}

bool UGA_WeaponAttackBase::ResolveRangedSpawnLocation(APlayerBase* Player,
	TSubclassOf<AProjectileBase> ProjectileClass, FVector& Location) const
{
	if (!IsValid(Player) || !ProjectileClass || !GetWorld()) return false;
	Location = Player->GetActorLocation();
	const float Radius = ProjectileClass->GetDefaultObject<AProjectileBase>()->GetProjectileCollisionRadius();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MuzzleObstruction), false, Player);
	FHitResult Obstruction;
	// 총구가 벽을 넘어가도 탄환은 캐릭터와 총구 사이의 장애물 앞에서 생성한다.
	if (!Location.Equals(Player->GetActorLocation()) && GetWorld()->SweepSingleByChannel(Obstruction,
		Player->GetActorLocation(), Location, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(Radius), Params))
	{
		if (Obstruction.bStartPenetrating) return false;
		Location = Obstruction.Location - (Location - Player->GetActorLocation()).GetSafeNormal();
	}
	return true;
}

bool UGA_WeaponAttackBase::CanContinueRangedAttack(const APlayerBase* Player) const
{
	return IsActive() && !bFinishingAttack && IsValid(Player) && !Player->IsDead()
		&& IsValid(SourceWeapon) && IsValid(RangedAttack.WeaponData.Get()) && RangedAttack.ProjectileClass
		&& Player->GetWeaponInSlot(WeaponSlot) == SourceWeapon
		&& SourceWeapon->WeaponData == RangedAttack.WeaponData;
}

AProjectileBase* UGA_WeaponAttackBase::CreateRangedProjectile(APlayerBase* Player, const FTransform& Transform)
{
	if (!CanContinueRangedAttack(Player)) return nullptr;
	AProjectileBase* Projectile = GetWorld()->SpawnActorDeferred<AProjectileBase>(RangedAttack.ProjectileClass,
		Transform, Player, Player, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile) return nullptr;
	if (!CanContinueRangedAttack(Player) || !Projectile->InitializeProjectile(Player, DamageEffectClass,
		RangedAttack.DamagePerShot, RangedAttack.ProjectileSpeed, RangedAttack.Range, RangedAttack.FeedbackStrength))
	{
		Projectile->Destroy();
		return nullptr;
	}
	return Projectile;
}

bool UGA_WeaponAttackBase::LaunchRangedProjectile(APlayerBase* Player, AProjectileBase* Projectile,
	const FTransform& Transform)
{
	if (!CanContinueRangedAttack(Player))
	{
		Projectile->Destroy();
		return false;
	}
	SourceWeapon->LastSuccessfulFireTime = GetWorld()->GetTimeSeconds();
	Projectile->ConfigureVisuals(RangedAttack.WeaponData.Get());
	const float MuzzleDistance = FVector::VectorPlaneProject(
		Player->GetWeaponMuzzleLocation(WeaponSlot) - Transform.GetLocation(), FVector::YAxisVector).Size();
	Projectile->ConfigureInitialVisibility(
		MuzzleDistance * FMath::Max(0.0f, RangedAttack.WeaponData->ProjectileRevealDistanceMultiplier));
	Projectile->FinishSpawning(Transform);
	if (!CanContinueRangedAttack(Player))
	{
		Projectile->Destroy();
		return false;
	}
	Player->PlayRangedShotFeedback(WeaponSlot, RangedAttack.WeaponData.Get(), Transform);
	if (!CanContinueRangedAttack(Player))
	{
		Projectile->Destroy();
		return false;
	}
	Projectile->LaunchProjectile(RangedAttack.Direction);
	return CanContinueRangedAttack(Player);
}

void UGA_WeaponAttackBase::FireNextBurstShot()
{
	if (!IsActive() || bFinishingAttack) return;
	APlayerBase* Player = Cast<APlayerBase>(GetAvatarActorFromActorInfo());
	FVector Location;
	if (RemainingBurstShots <= 0 || !CanContinueRangedAttack(Player)
		|| !ResolveRangedSpawnLocation(Player, RangedAttack.ProjectileClass, Location))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	const FTransform Transform(RangedAttack.Direction.Rotation(), Location);
	AProjectileBase* Projectile = CreateRangedProjectile(Player, Transform);
	if (!Projectile || !LaunchRangedProjectile(Player, Projectile, Transform))
	{
		if (IsActive() && !bFinishingAttack) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	// 발사 중 콜백에서도 이번 탄환이 아직 처리 중임을 알 수 있도록 발사 후에 차감한다.
	--RemainingBurstShots;
	if (RemainingBurstShots == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(BurstShotTimer);
		FinishAttackIfReady();
	}
}

void UGA_WeaponAttackBase::FinishAttackIfReady()
{
	if (IsActive() && !bFinishingAttack && bAttackMontageFinished && RemainingBurstShots == 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_WeaponAttackBase::HandleWeaponDataChanged()
{
	if (IsActive() && !bFinishingAttack && !CanContinueRangedAttack(Cast<APlayerBase>(GetAvatarActorFromActorInfo())))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

float UGA_WeaponAttackBase::CalculateMeleeDamage() const
{
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Data || !ASC) return 0.0f;

	float Damage = FMath::Max(0.0f, Data->AttackPower) * ChargeDamageMultiplier;
	if (ASC->HasAttributeSetForAttribute(UCombatAttributeSet::GetDamageDealtMultiplierAttribute()))
		Damage *= FMath::Max(0.0f, ASC->GetNumericAttribute(UCombatAttributeSet::GetDamageDealtMultiplierAttribute()));
	if (ASC->HasAttributeSetForAttribute(UCombatAttributeSet::GetMeleeDamageMultiplierAttribute()))
		Damage *= FMath::Max(0.0f, ASC->GetNumericAttribute(UCombatAttributeSet::GetMeleeDamageMultiplierAttribute()));
	Damage *= GetHealthDamageMultiplier();
	Damage *= CriticalHitDamageMultiplier;
	return FMath::IsFinite(Damage) && Damage > 0.0f ? Damage : 0.0f;
}

void UGA_WeaponAttackBase::CaptureMeleeAttack(APlayerBase* Player, FName AttackSection)
{
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	if (!Player || !Data || (Data->WeaponType != EWeaponType::LightMelee && Data->WeaponType != EWeaponType::HeavyMelee)) return;

	MeleeAttack = NewObject<UMeleeAttackEventData>(this);
	MeleeAttack->WeaponData = SourceWeapon->WeaponData;
	MeleeAttack->AttackMontage = Data->AttackMontage;
	MeleeAttack->AttackSection = AttackSection;
	MeleeAttack->WeaponSlot = WeaponSlot;
	MeleeAttack->SkeletalMesh = Player->GetMesh()->GetSkeletalMeshAsset();
	MeleeAttack->ActorTransform = Player->GetActorTransform();
	MeleeAttack->MeshTransform = Player->GetMesh()->GetComponentTransform();
	MeleeAttack->Direction = FVector::VectorPlaneProject(Player->GetActorForwardVector(), FVector::YAxisVector).GetSafeNormal();
	MeleeAttack->AttackRange = Data->AttackRange;
	MeleeAttack->HitRadius = MeleeHitRadius;
	MeleeAttack->DamageEffectClass = DamageEffectClass;
	MeleeAttack->FeedbackStrength = GetImpactFeedbackStrength();
	RecordedMeleeDamage = CalculateMeleeDamage();
}

void UGA_WeaponAttackBase::ExecuteAttack()
{
	if (!IsActive() || bFinishingAttack || bAttackExecuted) return;
	APlayerBase* Player = Cast<APlayerBase>(GetAvatarActorFromActorInfo());
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	if (!Player || Player->IsDead() || !Data || Player->GetWeaponInSlot(WeaponSlot) != SourceWeapon)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	// 공격마다 치명타를 한 번 판정하고 모든 명중에 같은 결과를 사용한다.
	CriticalHitDamageMultiplier = RollCriticalDamageMultiplier();
	const bool bRanged = Data->WeaponType == EWeaponType::LightRanged || Data->WeaponType == EWeaponType::HeavyRanged;
	AProjectileBase* Projectile = nullptr;
	FTransform SpawnTransform;
	FVector ShotDirection = FVector::ZeroVector;
	if (bRanged)
	{
		FVector SpawnLocation;
		const TSubclassOf<AProjectileBase> ProjectileClass = Player->GetAttackProjectileClass(Data);
		if (!ProjectileClass || ProjectileClass->HasAnyClassFlags(CLASS_Abstract)
			|| !ResolveRangedSpawnLocation(Player, ProjectileClass, SpawnLocation))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
		ShotDirection = FVector::VectorPlaneProject(Player->GetAimTargetLocation() - SpawnLocation, FVector::YAxisVector).GetSafeNormal();
		if (!CaptureRangedAttack(Player, ProjectileClass, ShotDirection))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
		RemainingBurstShots = RangedAttack.ShotCount - 1;
		if (RemainingBurstShots > 0)
		{
			Player->OnWeaponDataChanged.AddUniqueDynamic(this, &UGA_WeaponAttackBase::HandleWeaponDataChanged);
		}
		SpawnTransform = FTransform(ShotDirection.Rotation(), SpawnLocation);
		Projectile = CreateRangedProjectile(Player, SpawnTransform);
		if (!Projectile)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
	}
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		if (Projectile) Projectile->Destroy();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	// 공격 실행 알림에서 취소되었다면 저장된 정보로 발사를 이어가지 않는다.
	if (!IsActive() || bFinishingAttack)
	{
		if (Projectile) Projectile->Destroy();
		return;
	}
	bAttackExecuted = true;
	if (Projectile)
	{
		if (!LaunchRangedProjectile(Player, Projectile, SpawnTransform))
		{
			if (IsActive() && !bFinishingAttack) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
		if (RemainingBurstShots > 0)
		{
			GetWorld()->GetTimerManager().SetTimer(BurstShotTimer, this,
				&UGA_WeaponAttackBase::FireNextBurstShot, RangedAttack.ShotInterval, true);
		}
	}

	if (!bRanged)
	{
		UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, WeaponGameplayTags::Event_MeleeHit, nullptr, false, true);
		HitTask->EventReceived.AddDynamic(this, &UGA_WeaponAttackBase::HandleMeleeHit);
		HitTask->ReadyForActivation();
	}

	bAttackMontageFinished = !Data->AttackMontage;
	FName AttackSection = NAME_None;
	if (Data->AttackMontage)
	{
		if (Data->bRandomizeAttackSection && Data->AttackMontage->GetNumSections() > 0)
		{
			const int32 SectionIndex = FMath::RandRange(0, Data->AttackMontage->GetNumSections() - 1);
			AttackSection = Data->AttackMontage->GetSectionName(SectionIndex);
		}
		if (!bRanged) CaptureMeleeAttack(Player, AttackSection);
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Data->AttackMontage, 1.0f, AttackSection, true, 1.0f, 0.0f, true);
		// 블렌드아웃 도중 중단돼도 OnInterrupted로 공격 상태를 정리한다.
		MontageTask->OnCompleted.AddDynamic(this, &UGA_WeaponAttackBase::HandleMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_WeaponAttackBase::HandleMontageCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &UGA_WeaponAttackBase::HandleMontageCancelled);
		MontageTask->ReadyForActivation();
		if (!IsActive()) return;
		if (!AttackSection.IsNone())
		{
			// 에셋의 원래 연결은 유지하고, 이번 재생에서만 다음 섹션 진행을 끊는다.
			Player->GetMesh()->GetAnimInstance()->Montage_SetNextSection(AttackSection, NAME_None, Data->AttackMontage);
		}
	}

	if (!bRanged)
	{
		Player->MulticastMeleeAttackFeedback(WeaponSlot, SourceWeapon->WeaponData);
	}
	FinishAttackIfReady();
}

void UGA_WeaponAttackBase::HandleMeleeHit(FGameplayEventData Payload)
{
	APlayerBase* Player = Cast<APlayerBase>(GetAvatarActorFromActorInfo());
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	if (!IsActive() || bFinishingAttack || !bAttackExecuted || !Player || !Player->HasAuthority() || Player->IsDead()
		|| !Data || !DamageEffectClass || Payload.Instigator != Player || Payload.OptionalObject != Data->AttackMontage) return;
	if (Data->WeaponType != EWeaponType::LightMelee && Data->WeaponType != EWeaponType::HeavyMelee) return;

	const FVector Direction = FVector::VectorPlaneProject(Player->GetActorForwardVector(), FVector::YAxisVector).GetSafeNormal();
	if (Direction.IsNearlyZero()) return;
	const FVector Start = Player->GetActorLocation();

    const int32 Phase=FMath::RoundToInt(Payload.EventMagnitude);
    if(Phase==0)
    {
        // Assets with a notify window must never also execute their old range hit.
        for(const FAnimNotifyEvent& Event:Data->AttackMontage->Notifies)
            if(Event.NotifyStateClass && Event.NotifyStateClass->IsA<UAnimNotifyState_MeleeTrail>()) return;
    }
    if(Phase==1){bMeleeWindowOpen=true;bHasPreviousMeleeTransform=false;}
    if(Phase>=2 && !bMeleeWindowOpen) return;
    UStaticMeshComponent* WeaponMesh=Player->GetWeaponVisualMesh(WeaponSlot);
    if(!WeaponMesh || !WeaponMesh->GetStaticMesh()) return;
    FVector LocalStart=Data->MeleeTraceStart,LocalEnd=Data->MeleeTraceEnd;
    if(!Data->bCustomMeleeTrace)
    {
        const FBox Bounds=WeaponMesh->GetStaticMesh()->GetBoundingBox();
        const FVector Extent=Bounds.GetExtent();
        int32 Axis=Extent.X>=Extent.Y && Extent.X>=Extent.Z ? 0 : (Extent.Y>=Extent.Z ? 1 : 2);
        LocalStart=LocalEnd=Bounds.GetCenter();
        LocalStart[Axis]=FMath::Lerp(Bounds.Min[Axis],Bounds.Max[Axis],0.15f);
        LocalEnd[Axis]=FMath::Lerp(Bounds.Min[Axis],Bounds.Max[Axis],0.95f);
        if(Data->WeaponType==EWeaponType::HeavyMelee)
        {
            const float Tip=Data->MeleeTrailOffset.GetLocation()[Axis];
            if(Tip<Bounds.GetCenter()[Axis]){LocalStart[Axis]=Bounds.Min[Axis];LocalEnd[Axis]=FMath::Lerp(Bounds.Min[Axis],Bounds.Max[Axis],0.35f);}
            else LocalStart[Axis]=FMath::Lerp(Bounds.Min[Axis],Bounds.Max[Axis],0.65f);
        }
    }
    const FTransform Current=WeaponMesh->GetComponentTransform();
    if(!bHasPreviousMeleeTransform) PreviousMeleeTransform=Current;
    TArray<FHitResult> Hits;
    bool bDebug=false;
#if ENABLE_DRAW_DEBUG
    bDebug=CVarDebugMelee.GetValueOnGameThread()!=0;
#endif
    MeleeSweep::Trace(Player->GetWorld(),Player,LocalStart,LocalEnd,Data->MeleeTraceRadius,PreviousMeleeTransform,Current,Hits,bDebug);
    PreviousMeleeTransform=Current;bHasPreviousMeleeTransform=Phase!=0;
    if(Phase==3){bMeleeWindowOpen=false;bHasPreviousMeleeTransform=false;}
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponMeleeWall),false,Player);

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) return;
	const float FinalDamage = CalculateMeleeDamage();
	if (FinalDamage <= 0.0f) return;
	// 명중 대상이 없어도 타격 프레임의 피해와 위치를 보관한다.
	if (MeleeAttack)
	{
		RecordedMeleeDamage = FinalDamage;
		MeleeAttack->ActorTransform = Player->GetActorTransform();
		MeleeAttack->MeshTransform = Player->GetMesh()->GetComponentTransform();
		MeleeAttack->Direction = Direction;
		MeleeAttack->FeedbackStrength = GetImpactFeedbackStrength();
	}
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		const TWeakObjectPtr<AActor> TargetKey(Target);
		if (!IsValid(Target) || HitActorsThisAttack.Contains(TargetKey)) continue;
		const AEnemyBase* Enemy = Cast<AEnemyBase>(Target);
		ABossMonster* Boss = Cast<ABossMonster>(Target);
		IAttackReceiverInterface* AttackReceiver = Cast<IAttackReceiverInterface>(Target);
		if ((!Enemy && !Boss && !AttackReceiver) || (Enemy && Enemy->IsDead()) || (Boss && Boss->GetBossState() == EBossState::Dead)) continue;
		// 대상 자신을 제외하고 사이의 장애물을 검사하여 벽 너머 타격을 막는다.
		FCollisionQueryParams WallParams(QueryParams);
		WallParams.AddIgnoredActor(Target);
		FHitResult WallHit;
		if (Player->GetWorld()->LineTraceSingleByChannel(WallHit, Start, Target->GetActorLocation(), ECC_Visibility, WallParams)) continue;
		// 피해 이벤트가 재진입할 수 있으므로 호출 전에 이번 공격의 명중을 기록한다.
		HitActorsThisAttack.Add(TargetKey);
		// Initial overlaps have no useful sweep contact. Resolve a surface point before damage callbacks.
		FVector ContactPoint = Hit.ImpactPoint;
		FVector ContactNormal = Hit.ImpactNormal;
		if (Hit.bStartPenetrating)
		{
			ContactPoint = Target->GetActorLocation();
			if (UPrimitiveComponent* Component = Hit.GetComponent())
			{
				FVector ClosestPoint;
				if (Component->GetClosestPointOnCollision(Start, ClosestPoint) > 0.0f) ContactPoint = ClosestPoint;
			}
			ContactNormal = (Start - ContactPoint).GetSafeNormal();
		}
		if (ContactNormal.IsNearlyZero()) ContactNormal = -Direction;
		bool bApplied = false;
		if (!Enemy && !Boss && AttackReceiver)
		{
			AttackReceiver->ReceiveAttackDamage(FinalDamage, Player);
			bApplied = true;
		}
		else
		{
			bApplied = UCombatDamageLibrary::ApplyFinalDamageToTarget(
				Player, Target, DamageEffectClass, FinalDamage, WeaponGameplayTags::Damage_PlayerMelee, GetImpactFeedbackStrength());
		}
		if (bApplied) Player->MulticastWeaponImpact(const_cast<UWeaponDataAsset*>(Data),
			ContactPoint, ContactNormal, nullptr, nullptr);
		// true는 GE 승인 여부이며, 면역으로 실제 체력이 줄지 않아도 이 공격에서는 다시 적용하지 않는다.
		if (!bApplied && IsActive() && !bFinishingAttack) HitActorsThisAttack.Remove(TargetKey);
		// 피해 이벤트가 플레이어 사망·공격 취소를 유발했을 수도 있다.
		if (!IsActive() || bFinishingAttack || Player->IsDead()) break;
	}
}

float UGA_WeaponAttackBase::GetImpactFeedbackStrength() const
{
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	if (!Data) return 1.0f;
	const bool bHeavy = Data->WeaponType == EWeaponType::HeavyMelee || Data->WeaponType == EWeaponType::HeavyRanged;
	return (bHeavy ? 1.5f : 1.0f) * FMath::Lerp(1.0f, 1.35f, GetChargeRatio());
}

void UGA_WeaponAttackBase::HandleMontageCompleted()
{
	if (!IsActive() || bFinishingAttack) return;
	bAttackMontageFinished = true;
	FinishAttackIfReady();
}

void UGA_WeaponAttackBase::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_WeaponAttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bFinishingAttack || !IsEndAbilityValid(Handle, ActorInfo)) return;
	bFinishingAttack = true;
	APlayerBase* CompletedPlayer = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* CompletedASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	// 종료 알림에서 다음 공격이 시작돼도 이번 공격의 정보는 독립적으로 유지한다.
	TStrongObjectPtr<UMeleeAttackEventData> CompletedAttack(
		!bWasCancelled && bAttackExecuted && bAttackMontageFinished && RecordedMeleeDamage > 0.0f ? MeleeAttack.Get() : nullptr);
	const float CompletedDamage = RecordedMeleeDamage;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeStartTimer);
		World->GetTimerManager().ClearTimer(BurstShotTimer);
	}
	if (bWasCancelled)
	{
		if (APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr) Player->ClearPendingAttack();
	}
	if (APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		Player->OnWeaponDataChanged.RemoveDynamic(this, &UGA_WeaponAttackBase::HandleWeaponDataChanged);
		Player->ClearWeaponCharge(WeaponSlot);
		if (bAttackExecuted && SourceWeapon && SourceWeapon->WeaponData
			&& (SourceWeapon->WeaponData->WeaponType == EWeaponType::LightMelee || SourceWeapon->WeaponData->WeaponType == EWeaponType::HeavyMelee))
			Player->MulticastStopMeleeAttackFeedback(WeaponSlot);
	}
	bMeleeWindowOpen=false; bHasPreviousMeleeTransform=false;
	bWaitingForRelease = false;
	bChargeStarted = false;
	bAttackExecuted = false;
	ChargeStartTime = 0.0;
	ChargeDuration = 0.0f;
	ChargeDamageMultiplier = 1.0f;
	CriticalHitDamageMultiplier = 1.0f;
	HitActorsThisAttack.Reset();
	ChargeElapsedTime = 0.0f;
	SourceWeapon = nullptr;
	DamageEffectClass = nullptr;
	RangedAttack = FWeaponRangedAttackSnapshot{};
	MeleeAttack = nullptr;
	RecordedMeleeDamage = 0.0f;
	RemainingBurstShots = 0;
	bAttackMontageFinished = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	// 본체의 공격 상태를 해제한 뒤 후속 능력에 알리므로 본체 후딜을 늘리지 않는다.
	if (CompletedAttack.IsValid() && IsValid(CompletedPlayer) && CompletedPlayer->HasAuthority()
		&& !CompletedPlayer->IsDead() && IsValid(CompletedASC) && CompletedASC->GetAvatarActor() == CompletedPlayer)
	{
		FGameplayEventData Payload;
		Payload.EventTag = CombatGameplayTags::Event_MeleeAttackFinished;
		Payload.Instigator = CompletedPlayer;
		Payload.EventMagnitude = CompletedDamage;
		Payload.InstigatorTags.AddTag(WeaponGameplayTags::Damage_PlayerMelee);
		Payload.OptionalObject = CompletedAttack.Get();
		CompletedASC->HandleGameplayEvent(Payload.EventTag, &Payload);
	}
}
