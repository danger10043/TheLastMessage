#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Weapon/WeaponTypes.h"
#include "GA_WeaponAttackBase.generated.h"

class UGameplayEffect;
class UWeaponBase;

/** 주무기·보조무기 공격 능력이 공유하는 데이터와 실행 상태. */
UCLASS(Abstract, Blueprintable)
class THELASTMESSAGE_API UGA_WeaponAttackBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponAttackBase();
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	UFUNCTION(BlueprintPure, Category = "Weapon|Charge")
	float GetChargeRatio() const;
	bool IsWaitingForAttackRelease() const { return bWaitingForRelease && IsActive() && !bFinishingAttack; }
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void HandleMeleeHit(FGameplayEventData Payload);
	UFUNCTION()
	void HandleMontageCompleted();
	UFUNCTION()
	void HandleMontageCancelled();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attack", meta = (ClampMin = "1.0"))
	float MeleeHitRadius = 35.0f;

	// 파생 능력이 주무기 또는 보조무기 슬롯을 지정한다.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attack")
	EWeaponSlot WeaponSlot = EWeaponSlot::None;

	// 공격 시작 시 확정할 무기 객체. 종료·취소 시 정리한다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|Attack")
	TObjectPtr<UWeaponBase> SourceWeapon;

	// 현재 공격의 차지 경과 시간. 기본 공격은 0부터 시작한다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|Attack")
	float ChargeElapsedTime = 0.0f;

	// 이번 근접 공격에서 이미 맞은 액터. 공격당 중복 피해를 방지한다.
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;

	// 이번 공격에 사용할 공통 피해 GE. 플레이어 기본값에서 가져온다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|Effects")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 원거리 공격 시 해당 슬롯의 에너지를 소비하는 GE.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
	TSubclassOf<UGameplayEffect> CostEffectClass;


private:
	void ExecuteAttack();
	void StartWeaponCharge();
	FTimerHandle ChargeStartTimer;
	bool bChargeStarted = false;
	/** 현재 체력 비율에 따라 근접·원거리 공격에 적용할 피해 배율을 계산한다. */
	float GetHealthDamageMultiplier() const;
	/** 치명타 확률로 판정하고 이번 공격에 적용할 피해 배율을 반환한다. */
	float RollCriticalDamageMultiplier() const;
	float GetImpactFeedbackStrength() const;
	double ChargeStartTime = 0.0;
	// 배율을 반영한 이번 공격의 최대 차지 시간. 차지 시작 시 확정한다.
	float ChargeDuration = 0.0f;
	float ChargeDamageMultiplier = 1.0f;
	// 이번 공격에 적용할 치명타 피해 배율. 일반 공격은 1이다.
	float CriticalHitDamageMultiplier = 1.0f;
	bool bWaitingForRelease = false;
	bool bAttackExecuted = false;

private:
	// GAS 태그가 실제 공격 상태를 관리하며, 이 값은 종료 콜백 재진입만 방지한다.
	bool bFinishingAttack = false;
};
