#include "GAS/Abilities/GA_CoreSupportRobot.h"

#include "AbilitySystemComponent.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "Core/CoreGameplayTags.h"
#include "Core/CoreSupportRobot.h"
#include "Engine/World.h"
#include "GameplayEffectExtension.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"

UGA_CoreSupportRobot::UGA_CoreSupportRobot()
{
	RobotClass = ACoreSupportRobot::StaticClass();
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
}

void UGA_CoreSupportRobot::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	TryActivatePassive(ActorInfo, Spec);
}

void UGA_CoreSupportRobot::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	TryActivatePassive(ActorInfo, Spec);
}

void UGA_CoreSupportRobot::TryActivatePassive(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(ASC) || Spec.IsActive() || !ActorInfo->AvatarActor.IsValid() || !ASC->IsOwnerActorAuthoritative()) return;
	ASC->TryActivateAbility(Spec.Handle);
}

void UGA_CoreSupportRobot::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!IsValid(ASC) || !IsValid(Player) || !Player->HasAuthority() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	DamageDealtHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(CombatGameplayTags::Event_DamageDealt)
		.AddUObject(this, &UGA_CoreSupportRobot::HandleDamageDealt);
	GuardBlockedHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(CoreGameplayTags::Event_GuardBlocked)
		.AddUObject(this, &UGA_CoreSupportRobot::HandleGuardBlocked);
	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UGA_CoreSupportRobot::HandleHealthChanged);
	IncomingDamageHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetIncomingDamageAttribute())
		.AddUObject(this, &UGA_CoreSupportRobot::HandleIncomingDamageChanged);
	DeadTagHandle = ASC->RegisterGameplayTagEvent(WeaponGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UGA_CoreSupportRobot::HandleDeadTagChanged);

	// 죽은 상태에서 코어를 얻으면 부활할 때 로봇을 만든다.
	if (!Player->IsDead()) SpawnRobot();
}

void UGA_CoreSupportRobot::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(CombatGameplayTags::Event_DamageDealt))
			Delegate->Remove(DamageDealtHandle);
		if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(CoreGameplayTags::Event_GuardBlocked))
			Delegate->Remove(GuardBlockedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetIncomingDamageAttribute()).Remove(IncomingDamageHandle);
		ASC->UnregisterGameplayTagEvent(DeadTagHandle, WeaponGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved);
	}
	DamageDealtHandle.Reset();
	GuardBlockedHandle.Reset();
	HealthChangedHandle.Reset();
	IncomingDamageHandle.Reset();
	DeadTagHandle.Reset();
	DestroyRobot();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_CoreSupportRobot::SpawnRobot()
{
	APlayerBase* Player = Cast<APlayerBase>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();
	if (IsValid(Robot) || !IsValid(Player) || Player->IsDead() || !World || !RobotClass) return;

	const FTransform SpawnTransform(Player->GetActorRotation(), Player->GetActorLocation() + SpawnOffset);
	ACoreSupportRobot* NewRobot = World->SpawnActorDeferred<ACoreSupportRobot>(RobotClass, SpawnTransform,
		Player, Player, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewRobot) return;
	if (!NewRobot->Initialize(Player))
	{
		NewRobot->Destroy();
		return;
	}
	NewRobot->FinishSpawning(SpawnTransform);
	Robot = NewRobot;
}

void UGA_CoreSupportRobot::DestroyRobot()
{
	if (IsValid(Robot)) Robot->Destroy();
	Robot = nullptr;
}

void UGA_CoreSupportRobot::HandleDamageDealt(const FGameplayEventData* Payload)
{
	const AActor* Player = GetAvatarActorFromActorInfo();
	// 로봇 탄의 피해는 공격자가 로봇이라 여기서 걸러진다.
	if (!IsValid(Robot) || !Payload || !Player || Payload->Instigator != Player
		|| !IsValid(Payload->Target) || Payload->Target == Player) return;

	Robot->RefreshCombat(CombatDuration);
	// 이번 공격으로 죽은 적은 노리지 않는다. 적이 아닌 대상(기믹 등)은 로봇이 알아서 무시한다.
	if (Payload->TargetTags.HasTagExact(CombatGameplayTags::Result_TargetKilled))
	{
		Robot->ForgetTarget(Payload->Target.Get());
		return;
	}
	Robot->SetPlayerHitTarget(const_cast<AActor*>(Payload->Target.Get()));
}

void UGA_CoreSupportRobot::HandleGuardBlocked(const FGameplayEventData* Payload)
{
	// 가드로 전부 막아 체력이 줄지 않은 공격도 전투로 본다. 막은 공격의 주인도 노릴 대상 후보가 된다.
	if (!IsValid(Robot)) return;
	Robot->RefreshCombat(CombatDuration);
	if (Payload) Robot->SetAttackerTarget(const_cast<AActor*>(Payload->Instigator.Get()));
}

void UGA_CoreSupportRobot::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// 회복·부활처럼 체력이 늘어난 경우는 전투로 보지 않는다.
	if (IsValid(Robot) && Data.NewValue < Data.OldValue) Robot->RefreshCombat(CombatDuration);
}

void UGA_CoreSupportRobot::HandleIncomingDamageChanged(const FOnAttributeChangeData& Data)
{
	// 공격자는 체력이 아니라 "받은 피해(IncomingDamage)" 알림에서 꺼낸다.
	// 피해 효과는 IncomingDamage만 바꾸고, 체력은 속성 세트가 그 값을 보고 따로 깎기 때문에
	// 체력 알림에는 효과 정보(GEModData)가 없고 IncomingDamage 알림에만 있다.
	// 몬스터·보스 피해는 그 몬스터의 ASC로 만든 효과라 공격자(Instigator)가 몬스터 자신이고, 기믹 피해면 로봇이 무시한다.
	// 처리 뒤 0으로 비우는 알림(값 감소)은 무시한다.
	if (!IsValid(Robot) || !Data.GEModData || Data.NewValue <= Data.OldValue) return;
	Robot->SetAttackerTarget(Data.GEModData->EffectSpec.GetContext().GetInstigator());
}

void UGA_CoreSupportRobot::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0) DestroyRobot();
	else SpawnRobot();
}
