#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreGuard.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
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

	/** 가드 중에만 적용하는 이동 정지 GE의 핸들. 종료 시 제거한다. */
	FActiveGameplayEffectHandle RootEffectHandle;

	/** 가드 중 표시하는 방벽. 종료 시 제거한다. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BarrierComponent;

	/** 방벽 머티리얼 인스턴스. 피격 연출에서 파라미터를 바꾼다. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BarrierMaterialInstance;
};
