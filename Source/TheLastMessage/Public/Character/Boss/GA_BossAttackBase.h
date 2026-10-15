#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Character/Boss/BossTypes.h"
#include "GA_BossAttackBase.generated.h"

class ABossMonster;
class UGameplayEffect;

UCLASS(Abstract, Blueprintable)
class THELASTMESSAGE_API UGA_BossAttackBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_BossAttackBase();

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual bool CheckCooldown(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ApplyCooldown(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
    ABossMonster* GetBossOwner() const;
    void ApplyAttackDamage(AActor* Target);

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    EBossAttackType AttackType = EBossAttackType::None;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    TSubclassOf<UGameplayEffect> DamageEffectClass;
};