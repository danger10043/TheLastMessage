#include "Animation/AnimNotifyState_MeleeTrail.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GAS/WeaponGameplayTags.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

namespace
{
 void SendTracePhase(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Animation,float Phase)
 {
  AActor* Owner=Mesh ? Mesh->GetOwner() : nullptr;
  if(!Owner || !Owner->HasAuthority() || !Owner->GetWorld() || !Owner->GetWorld()->IsGameWorld()) return;
  FGameplayEventData Payload; Payload.EventTag=WeaponGameplayTags::Event_MeleeHit;
  Payload.Instigator=Owner; Payload.OptionalObject=Animation;
  // 0 is the legacy single-frame notify; 1/2/3 are window begin/tick/end.
  Payload.EventMagnitude=Phase;
  UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner,Payload.EventTag,Payload);
 }
}
void UAnimNotifyState_MeleeTrail::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
 SendTracePhase(MeshComp,Animation,1.f);
	if (MeshComp && MeshComp->GetWorld() && MeshComp->GetWorld()->IsGameWorld())
		if (APlayerBase* Player = Cast<APlayerBase>(MeshComp->GetOwner())) Player->BeginMeleeTrailWindow(Animation);
}

void UAnimNotifyState_MeleeTrail::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
 SendTracePhase(MeshComp,Animation,3.f);
	if (MeshComp && MeshComp->GetWorld() && MeshComp->GetWorld()->IsGameWorld())
		if (APlayerBase* Player = Cast<APlayerBase>(MeshComp->GetOwner())) Player->EndMeleeTrailWindow(Animation);
}

void UAnimNotifyState_MeleeTrail::NotifyTick(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,float FrameDeltaTime,const FAnimNotifyEventReference& EventReference)
{
 Super::NotifyTick(MeshComp,Animation,FrameDeltaTime,EventReference);
 SendTracePhase(MeshComp,Animation,2.f);
}
