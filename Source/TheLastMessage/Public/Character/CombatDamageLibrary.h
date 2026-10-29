#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Weapon/WeaponTypes.h"
#include "CombatDamageLibrary.generated.h"

class UGameplayEffect;
class UAnimMontage;
class USkeletalMesh;
class UWeaponDataAsset;
struct FGameplayEffectSpec;

namespace CombatGameplayTags
{
    // 공격자 ASC의 피해 결과 알림. EventMagnitude는 실제 체력 감소량,
    // InstigatorTags는 근접/원거리 분류와 피해 출처이며 Instigator와 Target은 공격자와 피격 대상이다.
    THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_DamageDealt);

    // 근접 공격의 정상 종료 알림. EventMagnitude는 대상에게 적용하기 전의 계산된 피해,
    // OptionalObject는 UMeleeAttackEventData이며 명중 여부와 관계없이 공격당 한 번 전달한다.
    THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_MeleeAttackFinished);

    // 근접 분류를 유지하면서 보조 타격이 만든 피해임을 구분한다.
    THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Source_AssistStrike);

    // 이번 피해로 체력이 0이 된 결과. 이벤트의 TargetTags에만 담는다.
    THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Result_TargetKilled);
}

/** 근접 공격 종료 후에도 원래 무기·모션·공격 위치를 사용할 수 있도록 보관하는 정보다. */
UCLASS(BlueprintType)
class THELASTMESSAGE_API UMeleeAttackEventData : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    TObjectPtr<UWeaponDataAsset> WeaponData = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    TObjectPtr<UAnimMontage> AttackMontage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    FName AttackSection = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    EWeaponSlot WeaponSlot = EWeaponSlot::None;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    FTransform ActorTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    FTransform MeshTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    FVector Direction = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    float AttackRange = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    float HitRadius = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Melee")
    float FeedbackStrength = 1.0f;
};

UCLASS()
class THELASTMESSAGE_API UCombatDamageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // GE 승인 여부를 반환한다. DamageSource는 피해 분류와 별개인 선택적 출처 표시다.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat|Damage")
    static bool ApplyFinalDamageToTarget(
        AActor* SourceActor, AActor* Target,
        TSubclassOf<UGameplayEffect> DamageEffectClass,
        float FinalDamage, FGameplayTag DamageCategory, float FeedbackStrength = 1.0f,
        FGameplayTag DamageSource = FGameplayTag());

    // Rejects missing or ambiguous damage categories.
    static bool TryGetDamageCategory(const FGameplayEffectSpec& Spec, FGameplayTag& OutCategory);
};
