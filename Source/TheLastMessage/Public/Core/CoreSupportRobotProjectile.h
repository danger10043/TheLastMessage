#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoreSupportRobotProjectile.generated.h"

class ACoreSupportRobot;
class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;

/**
 * 지원 로봇이 쏘는 탄이다. 플레이어 무기 탄(ProjectileBase)과 로직이 달라 따로 둔다.
 *
 * - 직선으로 날아가며(중력 없음) 처음 닿은 살아 있는 적에게 피해를 주고 사라진다. 사이에 낀 다른 적이 맞아도 된다.
 * - 지형·기믹 메시처럼 막는 것에 닿으면 피해 없이 사라진다. 기믹의 공격 수신(AttackReceiver)은 부르지 않는다.
 * - 플레이어와 트리거 영역은 통과한다.
 * - 피해의 공격자는 로봇이다. 그래서 플레이어 전용 반응(전력 흡수·잔여 전력 회수·전투 판정·히트스톱)이 붙지 않는다.
 * 모양·이펙트는 이 클래스를 상속한 BP에서 붙인다.
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
	/** 무언가에 닿아 사라지기 직전에 불린다. 맞은 적이 없으면 HitActor는 지형이나 nullptr이다. BP에서 맞은 이펙트를 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Core|SupportRobot")
	void ReceiveImpact(const FVector& ImpactLocation, AActor* HitActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|SupportRobot")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|SupportRobot")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	/** 한 번만 처리한다. 적이면 피해를 주고, 이펙트를 알린 뒤 사라진다. */
	void Impact(const FVector& ImpactLocation, AActor* HitActor);

	TWeakObjectPtr<ACoreSupportRobot> Robot;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffect;

	float Damage = 0.0f;
	bool bImpacted = false;
};
