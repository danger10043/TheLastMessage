#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PoolableInterface.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintable)
class UPoolableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 오브젝트 풀(UActorPoolSubsystem)에서 재사용되는 액터가 구현하는 인터페이스.
 * 투사체, 보스 미사일, 몬스터 등에 사용한다.
 */
class THELASTMESSAGE_API IPoolableInterface
{
	GENERATED_BODY()

public:
	/** 풀에서 꺼내질 때(재사용 또는 새로 만든 직후) 호출된다. 상태를 처음으로 되돌리고 활성화한다. */
	virtual void OnAcquiredFromPool() {}

	/** 풀로 돌려보낼 때 호출된다. 진행 중인 동작을 멈추고 정리한다. */
	virtual void OnReturnedToPool() {}
};
