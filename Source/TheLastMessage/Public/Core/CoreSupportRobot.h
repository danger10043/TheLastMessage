#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoreSupportRobot.generated.h"

class APlayerBase;

/**
 * 지원 로봇 코어가 만드는 로봇이다. 능력이 전달한 전투 상태와 노릴 적을 보관한다.
 * 이동과 사격은 이 정보를 기준으로 동작한다.
 */
UCLASS()
class THELASTMESSAGE_API ACoreSupportRobot : public AActor
{
	GENERATED_BODY()

public:
	ACoreSupportRobot();

	/** 지연 생성 중 호출한다. 따라다닐 플레이어를 정한다. */
	bool Initialize(APlayerBase* InPlayer);

	/** 전투 상태를 지금부터 Duration초 동안 유지한다. 이미 더 길게 남아 있으면 줄이지 않는다. */
	void RefreshCombat(float Duration);

	/** 플레이어가 마지막으로 공격한 적을 노릴 대상으로 정한다. */
	void SetTarget(AActor* InTarget);

	/** 전투 상태와 노릴 적을 모두 비운다. */
	void ClearCombat();

	bool IsInCombat() const;
	AActor* GetTarget() const;
	APlayerBase* GetPlayer() const;

private:
	/** 능력이 끝나지 않은 채 플레이어가 사라져도 로봇이 남지 않게 한다. */
	UFUNCTION()
	void HandlePlayerDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerBase> Player;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Target;

	/** 전투 상태가 끝나는 월드 시간(초). */
	double CombatEndTime = 0.0;
};
