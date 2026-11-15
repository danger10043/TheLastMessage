#include "Animation/AnimNotifyState_AttackCancelWindow.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

namespace
{
	void RequestCancelWindow(USkeletalMeshComponent* MeshComp, bool bRequested)
	{
		// 에디터 미리보기나 클라이언트에서는 아무것도 하지 않는다. 판단은 서버가 한다.
		APlayerBase* Player = MeshComp ? Cast<APlayerBase>(MeshComp->GetOwner()) : nullptr;
		if (!Player || !Player->HasAuthority() || !Player->GetWorld() || !Player->GetWorld()->IsGameWorld()) return;
		Player->SetAttackCancelWindowRequested(bRequested);
	}
}

void UAnimNotifyState_AttackCancelWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	RequestCancelWindow(MeshComp, true);
}

void UAnimNotifyState_AttackCancelWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	RequestCancelWindow(MeshComp, false);
}
