#include "Animation/AnimNotify_MeleeHit.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/WeaponGameplayTags.h"

void UAnimNotify_MeleeHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || !Owner->GetWorld() || !Owner->GetWorld()->IsGameWorld()) return;

	FGameplayEventData Payload;
	Payload.EventTag = WeaponGameplayTags::Event_MeleeHit;
	Payload.Instigator = Owner;
	Payload.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, Payload.EventTag, Payload);
}
