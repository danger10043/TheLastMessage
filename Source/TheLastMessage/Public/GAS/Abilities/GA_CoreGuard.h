#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreGuard.generated.h"

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

	/** 막았을 때 재생하는 소리. 비우면 소리 없이 동작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Block")
	TObjectPtr<USoundBase> BlockSound;

	/** 에너지가 부족해 일부만 막았을 때의 소리 높낮이 배율. 가드 브레이크 소리가 없을 때 막는 소리에 적용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Block", meta = (ClampMin = "0.1"))
	float PartialBlockPitch = 0.8f;

	/** 일부만 막아 가드가 풀릴 때 재생하는 소리. 비우면 막는 소리를 낮은 높이로 재생한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Break")
	TObjectPtr<USoundBase> GuardBreakSound;

	/** 가드 브레이크 때 방벽이 완전히 깨져 사라지는 데 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Break", meta = (ClampMin = "0.01"))
	float GuardBreakTime = 0.15f;

	/** 가드 브레이크 후 다시 가드할 수 없는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Break", meta = (ClampMin = "0"))
	float GuardBreakCooldown = 0.5f;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void ShowBarrier(AActor* Avatar);
	void HideBarrier();

	/** 가드 성공 이벤트를 받아 방벽을 맞은 방향부터 깎고 소리를 재생한다. */
	UFUNCTION()
	void HandleGuardBlocked(FGameplayEventData Payload);

	/** 에너지가 부족해 일부만 막으면 방벽을 깨뜨리고 가드를 강제로 해제한다. */
	void BreakGuard();

	/** 깎인 방벽을 다시 채우고 머티리얼에 반영한다. 가득 차면 멈추고, 가드 브레이크 중이면 다 깨진 뒤 제거한다. */
	void UpdateBarrier();

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
