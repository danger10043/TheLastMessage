#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttackCancelWindow.generated.h"

/**
 * 공격 몽타주에서 "여기부터 이동·점프·아래 입력으로 후딜을 끊을 수 있다"는 지점에 배치한다.
 * 이 지점부터 공격이 끝날 때까지 취소가 가능하다. 구간을 직접 정하고 싶으면 Attack Cancel Window 상태 노티파이를 쓴다.
 * 근접은 마지막 타격 프레임 뒤에, 원거리는 연발이 끝난 뒤에만 실제로 열린다.
 */
UCLASS(DisplayName = "Attack Cancel Open")
class THELASTMESSAGE_API UAnimNotify_AttackCancelWindow : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
