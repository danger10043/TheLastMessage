#include "Animation/AnimNotifyState_MeleeTrail.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UAnimNotifyState_MeleeTrail::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (MeshComp && MeshComp->GetWorld() && MeshComp->GetWorld()->IsGameWorld())
		if (APlayerBase* Player = Cast<APlayerBase>(MeshComp->GetOwner())) Player->BeginMeleeTrailWindow(Animation);
}

void UAnimNotifyState_MeleeTrail::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (MeshComp && MeshComp->GetWorld() && MeshComp->GetWorld()->IsGameWorld())
		if (APlayerBase* Player = Cast<APlayerBase>(MeshComp->GetOwner())) Player->EndMeleeTrailWindow(Animation);
}
