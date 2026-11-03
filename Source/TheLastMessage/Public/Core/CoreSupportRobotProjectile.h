#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoreSupportRobotProjectile.generated.h"

class ACoreSupportRobot;
class UGameplayEffect;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 지원 로봇이 쏘는 탄이다. 플레이어 무기 탄(ProjectileBase)과 로직이 달라 따로 둔다.
 *
 * - 직선으로 날아가며(중력 없음) 처음 닿은 살아 있는 적에게 피해를 주고 사라진다. 사이에 낀 다른 적이 맞아도 된다.
 * - 지형·기믹 메시처럼 막는 것에 닿으면 피해 없이 사라진다. 기믹의 공격 수신(AttackReceiver)은 부르지 않는다.
 * - 플레이어와 트리거 영역은 통과한다.
 * - 피해의 공격자는 로봇이다. 그래서 플레이어 전용 반응(전력 흡수·잔여 전력 회수·전투 판정·히트스톱)이 붙지 않는다.
 * 모양(Visual 메시)과 맞은 연출(ImpactEffect·ImpactSound)은 이 클래스를 상속한 BP에서 값으로 지정한다.
 */
UCLASS()
class THELASTMESSAGE_API ACoreSupportRobotProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACoreSupportRobotProjectile();

	/**
	 * 지연 생성 중(FinishSpawning 전) 로봇이 부른다. 피해·크기·속도·방향을 정한다.
	 * MaxDistance(cm)만큼 날아가면 아무것도 맞히지 못해도 사라진다.
	 */
	void InitializeShot(ACoreSupportRobot* InRobot, const FVector& Direction, float Speed, float Radius,
		float InDamage, TSubclassOf<UGameplayEffect> InDamageEffect, float MaxDistance);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|SupportRobot")
	TObjectPtr<USphereComponent> Collision;

	/** 탄 모양. 충돌은 없고 보이기만 한다(판정은 Collision 구 하나). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|SupportRobot")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** 적이나 지형에 닿은 자리에서 재생할 이펙트와 소리. 플레이어 무기의 ImpactEffect·ImpactSound와 같은 역할이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Effect")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Effect")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|SupportRobot")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	/** 한 번만 처리한다. 적이면 피해를 주고, 맞은 연출을 재생한 뒤 사라진다. */
	void Impact(const FVector& ImpactLocation, AActor* HitActor);

	TWeakObjectPtr<ACoreSupportRobot> Robot;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffect;

	float Damage = 0.0f;
	bool bImpacted = false;
};
