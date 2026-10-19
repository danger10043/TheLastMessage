#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_MeleeHit.generated.h"

/** 공격 몽타주의 타격 프레임에 배치한다. 피해 판정은 GAS 능력이 처리한다. */
UCLASS(DisplayName = "Melee Hit")
class THELASTMESSAGE_API UAnimNotify_MeleeHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
