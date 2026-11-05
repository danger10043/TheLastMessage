#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyAimLaserComponent.generated.h"

class AEnemyBase;

/**
 * 원거리 공격 전에 공격 경로를 보여 주는 빨간 레이저 선.
 * 총구에서 조준 방향으로 벽까지(또는 최대 길이까지) 얇은 띠를 그린다. 몬스터가 필요할 때 만들어 쓴다.
 * - 처음에는 플레이어를 따라 조준 방향이 바뀌다가, LockDelay초가 지나면 방향을 고정한다. 고정된 뒤 쏘는 탄은 이 방향으로 나간다.
 * - 이 컴포넌트가 스스로 틱을 돌리므로 몬스터의 틱 설정과 상관없이 동작한다.
 */
UCLASS(ClassGroup = (Enemy))
class THELASTMESSAGE_API UEnemyAimLaserComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UEnemyAimLaserComponent();

	/** 선 굵기(cm)와 최대 길이(cm) */
	void Configure(float InWidth, float InMaxLength);

	/** 레이저를 켠다. LockDelay초 동안은 타겟을 따라가고, 그 뒤에는 방향을 고정한다(0 이하면 바로 고정). */
	void Show(AEnemyBase* InOwnerEnemy, float LockDelay);

	/** 레이저를 끈다. */
	void Hide();

	bool IsShown() const { return bShown; }

	/** 방향이 고정되었는지 */
	bool IsLocked() const { return bShown && bLocked; }

	/** 고정된 조준 방향. 고정되지 않았으면 영벡터. */
	FVector GetLockedDirection() const { return IsLocked() ? LockedDirection : FVector::ZeroVector; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdateLaser();

	TWeakObjectPtr<AEnemyBase> OwnerEnemy;

	float Width = 6.f;
	float MaxLength = 2500.f;
	float LockDelay = 0.f;
	float Elapsed = 0.f;

	bool bShown = false;
	bool bLocked = false;

	/** 마지막으로 계산한 조준 방향(타겟을 잠시 놓쳐도 이 방향을 유지한다) */
	FVector CurrentDirection = FVector::ZeroVector;
	FVector LockedDirection = FVector::ZeroVector;
};
