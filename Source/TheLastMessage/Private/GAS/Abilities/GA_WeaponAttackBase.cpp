#include "GAS/Abilities/GA_WeaponAttackBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Boss/BossMonster.h"
#include "Character/CombatDamageLibrary.h"
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
		&& Data->AttackRange > 0.0f && MeleeHitRadius > 0.0f;
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
	bWaitingForRelease = false;
	bChargeStarted = false;
	bAttackExecuted = false;
	ChargeDamageMultiplier = 1.0f;
	CriticalHitDamageMultiplier = 1.0f;
	ChargeElapsedTime = 0.0f;
	ChargeDuration = 0.0f;
	HitActorsThisAttack.Reset();
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
		FVector SpawnLocation = Player->GetWeaponMuzzleLocation(WeaponSlot);
		const TSubclassOf<AProjectileBase> ProjectileClass = Player->GetAttackProjectileClass(Data);
		if (!ProjectileClass || ProjectileClass->HasAnyClassFlags(CLASS_Abstract))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
		const float Radius = ProjectileClass->GetDefaultObject<AProjectileBase>()->GetProjectileCollisionRadius();
		// Sweep the whole projectile from the body to the muzzle so a barrel cannot fire through a wall.
		FCollisionQueryParams WallParams(SCENE_QUERY_STAT(MuzzleObstruction), false, Player);
		FHitResult Obstruction;
		if (!SpawnLocation.Equals(Player->GetActorLocation()) && GetWorld()->SweepSingleByChannel(Obstruction,
			Player->GetActorLocation(), SpawnLocation, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(Radius), WallParams))
		{
			if (Obstruction.bStartPenetrating)
			{
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
				return;
			}
			SpawnLocation = Obstruction.Location - (SpawnLocation - Player->GetActorLocation()).GetSafeNormal();
		}
		ShotDirection = (Player->GetAimTargetLocation() - SpawnLocation).GetSafeNormal();
		float FinalDamage = Data->AttackPower * ChargeDamageMultiplier;
		FinalDamage *= FMath::Max(0.0f, Player->GetCombatAttributes()->GetDamageDealtMultiplier());
		FinalDamage *= GetHealthDamageMultiplier();
		FinalDamage *= CriticalHitDamageMultiplier;
		if (ShotDirection.IsNearlyZero() || !FMath::IsFinite(FinalDamage) || FinalDamage <= 0.0f)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
		SpawnTransform = FTransform(ShotDirection.Rotation(), SpawnLocation);
		Projectile = GetWorld()->SpawnActorDeferred<AProjectileBase>(ProjectileClass, SpawnTransform,
			Player, Player, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile || !Projectile->InitializeProjectile(Player, DamageEffectClass, FinalDamage, Data->ProjectileSpeed, Data->Range, GetImpactFeedbackStrength()))
		{
			if (Projectile) Projectile->Destroy();
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
	bAttackExecuted = true;
	if (Projectile)
	{
		SourceWeapon->LastSuccessfulFireTime = GetWorld()->GetTimeSeconds();
		Projectile->ConfigureVisuals(SourceWeapon->WeaponData);
		Projectile->FinishSpawning(SpawnTransform);
		Player->PlayRangedShotFeedback(WeaponSlot, Data, SpawnTransform);
		Projectile->LaunchProjectile(ShotDirection);
		if (!IsActive() || bFinishingAttack) return;
	}

	if (!bRanged)
	{
		UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, WeaponGameplayTags::Event_MeleeHit, nullptr, false, true);
		HitTask->EventReceived.AddDynamic(this, &UGA_WeaponAttackBase::HandleMeleeHit);
		HitTask->ReadyForActivation();
	}

	FName AttackSection = NAME_None;
	if (Data->AttackMontage)
	{
		if (Data->bRandomizeAttackSection && Data->AttackMontage->GetNumSections() > 0)
		{
			const int32 SectionIndex = FMath::RandRange(0, Data->AttackMontage->GetNumSections() - 1);
			AttackSection = Data->AttackMontage->GetSectionName(SectionIndex);
		}
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
	if (!Data->AttackMontage) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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
	const FVector End = Start + Direction * Data->AttackRange;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponMeleeHit), false, Player);
	TArray<FHitResult> Hits;
	Player->GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(MeleeHitRadius), QueryParams);

#if ENABLE_DRAW_DEBUG
	if (CVarDebugMelee.GetValueOnGameThread() != 0)
	{
		// 구체를 Start~End로 훑으면 캡슐 모양의 검사 영역이 된다.
		DrawDebugCapsule(Player->GetWorld(), (Start + End) * 0.5f,
			Data->AttackRange * 0.5f + MeleeHitRadius, MeleeHitRadius,
			FQuat::FindBetweenNormals(FVector::UpVector, Direction), FColor::Cyan, false, 1.0f);
	}
#endif

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) return;
	// 공통 함수는 공격자 배율을 계산하지 않으므로 여기서 한 번만 적용한다.
	float FinalDamage = FMath::Max(0.0f, Data->AttackPower) * ChargeDamageMultiplier;
	if (SourceASC->HasAttributeSetForAttribute(UCombatAttributeSet::GetDamageDealtMultiplierAttribute()))
	{
		FinalDamage *= FMath::Max(0.0f, SourceASC->GetNumericAttribute(UCombatAttributeSet::GetDamageDealtMultiplierAttribute()));
	}
	// 근접 공격의 피해량에 근접 전용 배율을 적용한다.
	if (SourceASC->HasAttributeSetForAttribute(UCombatAttributeSet::GetMeleeDamageMultiplierAttribute()))
	{
		FinalDamage *= FMath::Max(0.0f, SourceASC->GetNumericAttribute(UCombatAttributeSet::GetMeleeDamageMultiplierAttribute()));
	}
	FinalDamage *= GetHealthDamageMultiplier();
	FinalDamage *= CriticalHitDamageMultiplier;
	if (!FMath::IsFinite(FinalDamage) || FinalDamage <= 0.0f) return;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		const TWeakObjectPtr<AActor> TargetKey(Target);
		if (!IsValid(Target) || HitActorsThisAttack.Contains(TargetKey)) continue;
		const AEnemyBase* Enemy = Cast<AEnemyBase>(Target);
		ABossMonster* Boss = Cast<ABossMonster>(Target);
		if ((!Enemy && !Boss) || (Enemy && Enemy->IsDead()) || (Boss && Boss->GetBossState() == EBossState::Dead)) continue;
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
		const bool bApplied = UCombatDamageLibrary::ApplyFinalDamageToTarget(
			Player, Target, DamageEffectClass, FinalDamage, WeaponGameplayTags::Damage_PlayerMelee, GetImpactFeedbackStrength());
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
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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
	GetWorld()->GetTimerManager().ClearTimer(ChargeStartTimer);
	if (bWasCancelled)
	{
		if (APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr) Player->ClearPendingAttack();
	}
	if (APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		Player->ClearWeaponCharge(WeaponSlot);
		if (bAttackExecuted && SourceWeapon && SourceWeapon->WeaponData
			&& (SourceWeapon->WeaponData->WeaponType == EWeaponType::LightMelee || SourceWeapon->WeaponData->WeaponType == EWeaponType::HeavyMelee))
			Player->MulticastStopMeleeAttackFeedback(WeaponSlot);
	}
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
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
