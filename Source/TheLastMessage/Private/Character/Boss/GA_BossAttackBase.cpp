#include "Character/Boss/GA_BossAttackBase.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"

UGA_BossAttackBase::UGA_BossAttackBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UGA_BossAttackBase::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(
        Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    ABossMonster* Boss = ActorInfo
        ? Cast<ABossMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority())
    {
        return false;
    }

    const EBossState State = Boss->GetBossState();
    APawn* Target = Cast<APawn>(Boss->GetCurrentTarget());
    return (State == EBossState::Idle || State == EBossState::Combat)
        && IsValid(Target) && Target->IsPlayerControlled()
        && Boss->IsDistanceInAttackRange(
            AttackType, Boss->GetDistanceTo(Target));
}

bool UGA_BossAttackBase::CheckCooldown(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    ABossMonster* Boss = ActorInfo
        ? Cast<ABossMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority())
    {
        return false;
    }

    // AttackConfigs is the cooldown source; no cooldown GE is used here.
    return Boss->GetAttackCooldownRemaining(AttackType) == 0.f;
}

void UGA_BossAttackBase::ApplyCooldown(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo) const
{
    ABossMonster* Boss = ActorInfo
        ? Cast<ABossMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!IsValid(Boss) || !Boss->TryStartAttackCooldown(AttackType))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Boss attack: failed to start cooldown. Check AttackConfigs."));
    }
}

ABossMonster* UGA_BossAttackBase::GetBossOwner() const
{
    return Cast<ABossMonster>(GetAvatarActorFromActorInfo());
}

void UGA_BossAttackBase::ApplyAttackDamage(AActor* Target)
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = Cast<APlayerBase>(Target);
    UAbilitySystemComponent* SourceASC = IsValid(Boss)
        ? Boss->GetAbilitySystemComponent() : nullptr;
    UAbilitySystemComponent* TargetASC = IsValid(Player)
        ? Player->GetAbilitySystemComponent() : nullptr;

    if (!IsValid(Boss) || !Boss->HasAuthority()
        || !IsValid(Player) || Player->IsDead()
        || !IsValid(SourceASC) || !IsValid(TargetASC) || !DamageEffectClass)
    {
        return;
    }

    if (!TargetASC->HasAttributeSetForAttribute(
        UCombatAttributeSet::GetIncomingDamageAttribute()))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMelee: target CombatAttributeSet is missing."));
        return;
    }

    const UGameplayEffect* EffectDefaults = DamageEffectClass.GetDefaultObject();
    if (!EffectDefaults
        || EffectDefaults->DurationPolicy != EGameplayEffectDurationType::Instant)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMelee: DamageEffectClass must be Instant."));
        return;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(Boss);
    FGameplayEffectSpecHandle Spec =
        SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Context);
    if (!Spec.IsValid())
    {
        return;
    }

    const FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Damage.Type.Melee")), false);
    if (MeleeTag.IsValid())
    {
        Spec.Data->AddDynamicAssetTag(MeleeTag);
    }

    // The GE defines the amount added to CombatAttributeSet.IncomingDamage.
    TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    UE_LOG(LogTemp, Log, TEXT("BossMelee: damage GE submitted -> %s"),
        *GetNameSafe(Player));
}