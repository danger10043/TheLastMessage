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
#include "DataAsset/WeaponDataAsset.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/Effects/GE_WeaponPrimaryCooldown.h"
#include "GAS/WeaponGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
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
	CooldownEffectClass = UGE_WeaponPrimaryCooldown::StaticClass();
	AttackCooldownTags.AddTag(WeaponGameplayTags::Cooldown_Primary);
}

bool UGA_WeaponAttackBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWeaponBase* Weapon = Player ? Player->GetPrimaryWeapon() : nullptr;
	const UWeaponDataAsset* Data = IsValid(Weapon) ? Weapon->WeaponData.Get() : nullptr;
	return Player && !Player->IsDead() && WeaponSlot == EWeaponSlot::Primary && IsValid(Data)
		&& (Data->WeaponType == EWeaponType::LightMelee || Data->WeaponType == EWeaponType::HeavyMelee)
		&& IsValid(Data->AttackMontage) && Player->GetMesh()->GetAnimInstance()
		&& Data->AttackRange > 0.0f && MeleeHitRadius > 0.0f && Data->AttackPower > 0.0f;
}

const FGameplayTagContainer* UGA_WeaponAttackBase::GetCooldownTags() const
{
	return &AttackCooldownTags;
}

void UGA_WeaponAttackBase::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!SourceWeapon || !SourceWeapon->WeaponData || !CooldownEffectClass) return;
	const float Duration = FMath::Max(0.0f, SourceWeapon->WeaponData->AttackInterval);
	if (Duration <= 0.0f) return;
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		CooldownEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(Duration, true);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
}

void UGA_WeaponAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bFinishingAttack = false;
	ChargeElapsedTime = 0.0f;
	HitActorsThisAttack.Reset();
	APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	SourceWeapon = Player ? Player->GetPrimaryWeapon() : nullptr;
	DamageEffectClass = Player ? Player->GetWeaponDamageEffectClass() : nullptr;
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	if (!DamageEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: assign GE_BP_CommonIncomingDamage to WeaponDamageEffectClass on the player."),
			*GetNameSafe(Player));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!Data || !Data->AttackMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Player->StopMovementForAttack();

	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, WeaponGameplayTags::Event_MeleeHit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &UGA_WeaponAttackBase::HandleMeleeHit);
	HitTask->ReadyForActivation();

	FName AttackSection = NAME_None;
	if (Data->bRandomizeAttackSection && Data->AttackMontage->GetNumSections() > 0)
	{
		const int32 SectionIndex = FMath::RandRange(0, Data->AttackMontage->GetNumSections() - 1);
		AttackSection = Data->AttackMontage->GetSectionName(SectionIndex);
	}
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Data->AttackMontage, 1.0f, AttackSection);
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

	if (Data->AttackEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(Player, Data->AttackEffect,
			Player->GetActorLocation(), Player->GetActorRotation());
	}
	if (Data->AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Player, Data->AttackSound, Player->GetActorLocation());
	}
}

void UGA_WeaponAttackBase::HandleMeleeHit(FGameplayEventData Payload)
{
	APlayerBase* Player = Cast<APlayerBase>(GetAvatarActorFromActorInfo());
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	if (!IsActive() || bFinishingAttack || !Player || !Player->HasAuthority() || Player->IsDead()
		|| !Data || !DamageEffectClass || Payload.Instigator != Player || Payload.OptionalObject != Data->AttackMontage) return;

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
	float FinalDamage = FMath::Max(0.0f, Data->AttackPower);
	if (SourceASC->HasAttributeSetForAttribute(UCombatAttributeSet::GetDamageDealtMultiplierAttribute()))
	{
		FinalDamage *= FMath::Max(0.0f, SourceASC->GetNumericAttribute(UCombatAttributeSet::GetDamageDealtMultiplierAttribute()));
	}
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
		const bool bApplied = UCombatDamageLibrary::ApplyFinalDamageToTarget(
			Player, Target, DamageEffectClass, FinalDamage, WeaponGameplayTags::Damage_PlayerMelee);
		// true는 GE 승인 여부이며, 면역으로 실제 체력이 줄지 않아도 이 공격에서는 다시 적용하지 않는다.
		if (!bApplied && IsActive() && !bFinishingAttack) HitActorsThisAttack.Remove(TargetKey);
		// 피해 이벤트가 플레이어 사망·공격 취소를 유발했을 수도 있다.
		if (!IsActive() || bFinishingAttack || Player->IsDead()) break;
	}
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
	HitActorsThisAttack.Reset();
	ChargeElapsedTime = 0.0f;
	SourceWeapon = nullptr;
	DamageEffectClass = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
