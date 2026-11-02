#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_CoreGuardRoot.generated.h"

/** 가드 중 이동과 에너지 충전 정지. 가드 능력이 활성화된 동안만 적용되며, 제거되면 기존 배율로 돌아간다. */
UCLASS()
class THELASTMESSAGE_API UGE_CoreGuardRoot : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_CoreGuardRoot();
};
