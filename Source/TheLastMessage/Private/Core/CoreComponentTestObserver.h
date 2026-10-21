#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CoreComponentTestObserver.generated.h"

/** 코어의 동적 델리게이트를 자동화 테스트의 검사 함수에 연결한다. 게임에서는 사용하지 않는다. */
UCLASS(Transient, NotBlueprintable)
class UCoreComponentTestObserver : public UObject
{
	GENERATED_BODY()

public:
#if WITH_DEV_AUTOMATION_TESTS
	TFunction<void(FName, int32, int32)> Callback;
#endif

	UFUNCTION()
	void HandleCoreChanged(FName CoreId, int32 OldLevel, int32 NewLevel)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (Callback) Callback(CoreId, OldLevel, NewLevel);
#endif
	}
};
