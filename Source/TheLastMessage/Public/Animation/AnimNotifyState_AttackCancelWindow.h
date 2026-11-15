#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_AttackCancelWindow.generated.h"

/**
 * 공격 몽타주에서 후딜을 끊을 수 있는 구간을 직접 지정한다. 구간 안에서만 이동·점프·아래 입력이 공격을 끊는다.
 * 근접은 마지막 타격 프레임 뒤에, 원거리는 연발이 끝난 뒤에만 실제로 열린다.
 * 시작 지점부터 끝까지 열어 두면 되는 경우에는 Attack Cancel Open 노티파이가 더 간단하다.
 */
UCLASS(DisplayName = "Attack Cancel Window")
class THELASTMESSAGE_API UAnimNotifyState_AttackCancelWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
