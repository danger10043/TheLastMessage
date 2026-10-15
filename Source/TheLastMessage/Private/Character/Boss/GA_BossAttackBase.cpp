#include "Character/Boss/GA_BossAttackBase.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossAttributeSet.h"
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
    UAbilitySystemComponent* SourceASC = IsValid(Boss)
        ? Boss->GetAbilitySystemComponent() : nullptr;
    UAbilitySystemComponent* TargetASC = IsValid(Target)
        ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target) : nullptr;

    if (!IsValid(Boss) || !Boss->HasAuthority()
        || !IsValid(SourceASC) || !IsValid(TargetASC) || !DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossMelee: damage skipped. Check source/target ASC and DamageEffectClass."));
        return;
    }

    bool bFoundAttackPower = false;
    const float Damage = SourceASC->GetGameplayAttributeValue(
        UBossAttributeSet::GetAttackPowerAttribute(), bFoundAttackPower);
    const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Data.Damage")), false);
    const FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Damage.Type.Melee")), false);

    if (!bFoundAttackPower || !FMath::IsFinite(Damage) || Damage <= 0.f
        || !DamageTag.IsValid() || !MeleeTag.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossMelee: check AttackPower, Data.Damage and Damage.Type.Melee."));
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

    Spec.Data->SetSetByCallerMagnitude(DamageTag, Damage);
    Spec.Data->AddDynamicAssetTag(MeleeTag);
    SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
    UE_LOG(LogTemp, Log, TEXT("BossMelee: damage GE submitted to %s, amount %.1f"),
        *GetNameSafe(Target), Damage);
}