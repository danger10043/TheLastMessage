#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "GE_CoreHeal.generated.h"

namespace CoreGameplayTags
{
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_HealAmount);
}

/** 계산된 회복량을 Health에 한 번 더한다. 회복량은 Data.Core.HealAmount로 전달한다. */
UCLASS()
class THELASTMESSAGE_API UGE_CoreHeal : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_CoreHeal();
};
