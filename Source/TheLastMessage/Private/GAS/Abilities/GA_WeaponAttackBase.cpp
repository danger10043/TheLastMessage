#include "GAS/Abilities/GA_WeaponAttackBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Engine/World.h"
#include "GAS/Effects/GE_WeaponMeleeDamage.h"
#include "GAS/Effects/GE_WeaponPrimaryCooldown.h"
#include "GAS/WeaponGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Weapon/WeaponBase.h"

UGA_WeaponAttackBase::UGA_WeaponAttackBase()
{
	// 무기·차지 시간·명중 목록을 플레이어마다 독립적으로 보관한다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.AddTag(WeaponGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Dead);
	DamageEffectClass = UGE_WeaponMeleeDamage::StaticClass();
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
	const UWeaponDataAsset* Data = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
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
	const FVector Start = Player->GetActorLocation();
	const FVector End = Start + Direction * Data->AttackRange;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponMeleeHit), false, Player);
	TArray<FHitResult> Hits;
	Player->GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(MeleeHitRadius), QueryParams);

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	for (const FHitResult& Hit : Hits)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(Hit.GetActor());
		if (!Enemy || Enemy->IsDead() || HitActorsThisAttack.Contains(TWeakObjectPtr<AActor>(Enemy))) continue;
		// 대상 자신을 제외하고 사이의 장애물을 검사하여 벽 너머 타격을 막는다.
		FCollisionQueryParams WallParams(QueryParams);
		WallParams.AddIgnoredActor(Enemy);
		FHitResult WallHit;
		if (Player->GetWorld()->LineTraceSingleByChannel(WallHit, Start, Enemy->GetActorLocation(), ECC_Visibility, WallParams)) continue;
		UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();
		if (!SourceASC || !TargetASC) continue;
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddSourceObject(SourceWeapon.Get());
		Context.AddHitResult(Hit);
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Context);
		if (!Spec.IsValid()) continue;
		Spec.Data->SetSetByCallerMagnitude(WeaponGameplayTags::Data_Damage, FMath::Max(0.0f, Data->AttackPower));
		Spec.Data->AddDynamicAssetTag(WeaponGameplayTags::Damage_PlayerMelee);
		HitActorsThisAttack.Add(TWeakObjectPtr<AActor>(Enemy));
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
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
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
