#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreGuard.generated.h"

class UAnimMontage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 가드 키를 누르고 있는 동안 활성화되어 State.Guarding과 이동 정지를 유지하고, 키를 떼면 종료하는 가드 능력이다.
 * 가드 중에는 카메라를 향한 원형 방벽을 표시하며, 방벽 머티리얼은 블루프린트 자식 클래스에서 지정한다.
 */
UCLASS()
class THELASTMESSAGE_API UGA_CoreGuard : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_CoreGuard();

protected:
	/** 방벽을 그릴 평면 메시. 기본값은 엔진의 100x100 평면이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Barrier")
	TObjectPtr<UStaticMesh> BarrierMesh;

	/** 방벽 머티리얼. Integrity(0~1)와 ImpactAngle(라디안) 파라미터로 깨짐 상태를 표현한다. 비우면 방벽을 표시하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Barrier")
	TObjectPtr<UMaterialInterface> BarrierMaterial;

	/** 방벽 원의 반지름(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Barrier", meta = (ClampMin = "1"))
	float BarrierRadius = 110.0f;

	/** 캡슐 중심 기준 방벽 위치. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Barrier")
	FVector BarrierOffset = FVector::ZeroVector;

	/** 한 번 막을 때 방벽이 깎이는 양(0~1). 연속으로 막으면 누적된다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Block", meta = (ClampMin = "0", ClampMax = "1"))
	float BlockBreakAmount = 0.35f;

	/** 연속으로 막아도 남는 최소 방벽 상태(0~1). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Block", meta = (ClampMin = "0", ClampMax = "1"))
	float MinIntegrity = 0.2f;

	/** 막는 순간 방벽이 깎이는 데 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Block", meta = (ClampMin = "0"))
	float BreakTime = 0.05f;

	/** 최소 상태에서 다시 가득 차는 데 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Block", meta = (ClampMin = "0.01"))
	float RebuildTime = 0.4f;

	/** 가드 몽타주. Start → Loop(반복) → End 섹션을 쓴다. 비우면 애니메이션 없이 동작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Animation")
	TSoftObjectPtr<UAnimMontage> GuardMontage;

	/** 가드를 풀 때 넘어갈 섹션. 없으면 몽타주를 블렌드 아웃한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Animation")
	FName GuardEndSection = TEXT("End");

	/** 가드가 취소될 때(사망 등) 몽타주를 끊는 블렌드 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Animation", meta = (ClampMin = "0"))
	float GuardMontageStopBlendTime = 0.15f;

	/** 막았을 때 재생하는 소리. 비우면 소리 없이 동작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Block")
	TObjectPtr<USoundBase> BlockSound;

	/** 일부만 막아 가드가 풀릴 때 재생하는 소리. 비우면 소리 없이 동작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Break")
	TObjectPtr<USoundBase> GuardBreakSound;

	/** 가드 브레이크 때 방벽이 완전히 깨져 사라지는 데 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Break", meta = (ClampMin = "0.01"))
	float GuardBreakTime = 0.15f;

	/** 가드 브레이크 후 다시 가드할 수 없는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Break", meta = (ClampMin = "0"))
	float GuardBreakCooldown = 0.5f;

	/** 퍼펙트 가드 때 방벽이 물드는 색. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Perfect")
	FLinearColor PerfectColor = FLinearColor(0.47f, 0.84f, 1.0f);

	/** 퍼펙트 가드 때 방벽이 퍼펙트 색으로 물드는 정도(0~1). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Perfect", meta = (ClampMin = "0", ClampMax = "1"))
	float PerfectTint = 0.6f;

	/** 퍼펙트 가드 때 방벽 밝기·선·테두리를 강조하는 세기. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Perfect", meta = (ClampMin = "0"))
	float PerfectFlashStrength = 0.9f;

	/** 퍼펙트 가드 때 방벽이 잠깐 커지는 비율. 0.08이면 8% 커졌다가 돌아온다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Perfect", meta = (ClampMin = "0"))
	float PerfectPop = 0.08f;

	/** 퍼펙트 가드 때 방벽 테두리에서 퍼지는 충격파 머티리얼. Progress(0~1)·Expand·Color 파라미터를 쓴다. 비우면 충격파를 표시하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Perfect")
	TObjectPtr<UMaterialInterface> ShockwaveMaterial;

	/** 충격파가 방벽 반지름 대비 바깥으로 퍼지는 거리 비율. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Perfect", meta = (ClampMin = "0.1"))
	float ShockwaveExpand = 0.95f;

	/** 충격파가 퍼져 사라지는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Perfect", meta = (ClampMin = "0.05"))
	float ShockwaveDuration = 0.35f;

	/** 퍼펙트 가드 때 재생하는 소리. 비우면 소리 없이 동작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Perfect")
	TObjectPtr<USoundBase> PerfectBlockSound;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** 가드 몽타주를 Start 섹션부터 재생한다. 능력이 끝나도 자동으로 끊지 않고 EndAbility에서 End로 넘긴다. */
	void PlayGuardMontage();
	/** 정상 해제·가드 브레이크면 End 섹션으로, 취소면 블렌드 아웃으로 몽타주를 정리한다. */
	void FinishGuardMontage(UAbilitySystemComponent* ASC, bool bWasCancelled);

	/** 이번 가드에서 재생한 몽타주. 다른 몽타주가 덮어썼으면 건드리지 않기 위해 기록한다. */
	TWeakObjectPtr<UAnimMontage> PlayingGuardMontage;

	void ShowBarrier(AActor* Avatar);
	void HideBarrier();

	/** 가드 성공 이벤트를 받아 방벽을 맞은 방향부터 깎고 소리를 재생한다. */
	UFUNCTION()
	void HandleGuardBlocked(FGameplayEventData Payload);

	/** 에너지가 부족해 일부만 막으면 방벽을 깨뜨리고 가드를 강제로 해제한다. */
	void BreakGuard();

	/** 퍼펙트 가드 판정 시간이 있으면 그동안 State.GuardPerfect를 붙인다. */
	void StartPerfectWindow(UAbilitySystemComponent* ASC);
	/** 퍼펙트 가드 판정을 끝낸다. 판정 시간이 지나거나 가드가 끝나면 호출한다. */
	void EndPerfectWindow();

	/** 이 가드에서 퍼펙트 가드 태그를 붙였는지. 붙인 만큼만 떼기 위해 기록한다. */
	bool bPerfectWindowActive = false;
	FTimerHandle PerfectWindowTimerHandle;

	/** 퍼펙트 가드 연출(번쩍임·부풀기·충격파)을 시작한다. 방벽은 깎지 않는다. */
	void PlayPerfectFeedback();
	void SpawnShockwave();
	/** 충격파를 모두 제거한다. */
	void ClearShockwaves();
	/** 업데이트 타이머가 멈춰 있으면 시작한다. */
	void EnsureBarrierTimer();

	/** 깎인 방벽을 다시 채우고 머티리얼에 반영한다. 가득 차면 멈추고, 가드 브레이크 중이면 다 깨진 뒤 제거한다. */
	void UpdateBarrier();

	/** 퍼펙트 가드 강조(1에서 0으로 줄어듦). 밝기·색·선 강조에 쓴다. */
	float PerfectFlash = 0.0f;
	/** 부풀기 진행(1에서 0으로 줄어듦). */
	float PerfectPopProgress = 0.0f;
	/** 머티리얼 기본값. 퍼펙트 강조 시 이 값을 기준으로 바꾼다. */
	float BaseLineAlpha = 0.35f;
	float BaseFillScale = 0.3f;
	float BaseRimWidth = 0.05f;
	FLinearColor BaseColor = FLinearColor::White;
	float BaseBarrierScale = 1.0f;

	/** 퍼지는 중인 충격파와 경과 시간. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ShockwaveComponents;
	TArray<float> ShockwaveElapsed;

	/** 가드 브레이크로 방벽이 깨지는 중인지. 가드가 끝나도 깨지는 연출이 끝날 때까지 방벽을 남긴다. */
	bool bGuardBroken = false;
	/** 이 시각까지는 가드를 다시 시작하지 않는다. */
	double GuardBreakCooldownEndTime = 0.0;

	/** 방벽 상태(0~1). 1이면 온전한 방벽이다. */
	float Integrity = 1.0f;
	/** 깨지는 중일 때 도달할 상태. */
	float BreakTarget = 1.0f;
	/** 깨지는 데 남은 시간(초). */
	float BreakRemaining = 0.0f;
	/** 막은 순간의 밝기 강조(1에서 0으로 줄어듦). */
	float Flash = 0.0f;
	/** 머티리얼 기본 밝기. 강조 시 이 값을 기준으로 키운다. */
	float BaseBrightness = 1.0f;
	double LastUpdateTime = 0.0;
	FTimerHandle BarrierTimerHandle;

	/** 가드 중에만 적용하는 이동 정지 GE의 핸들. 종료 시 제거한다. */
	FActiveGameplayEffectHandle RootEffectHandle;

	/** 가드 중 표시하는 방벽. 종료 시 제거한다. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BarrierComponent;

	/** 방벽 머티리얼 인스턴스. 피격 연출에서 파라미터를 바꾼다. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BarrierMaterialInstance;
};
