// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Component/InteractionComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Character/Player/PlayerBase.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Gimmick/GAS/GameplayAbility_InteractGimmick.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EventGimmickInteract, "Event.Gimmick.Interact");

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}


void UInteractionComponent::RequestInteract()
{
    // 입력 순간의 거리와 조건으로 대상을 다시 확인
    UpdateInteractionTarget();

    AActor* OwnerActor = GetOwner();
    AActor* Target = FocusedActor.Get();

    if (!IsValid(OwnerActor) || !IsValid(Target) || !CanUseInteractionAbility())
    {
        return;
    }

    if (!QueryCanInteract(Target, OwnerActor))
    {
        return;
    }

    UAbilitySystemComponent* ASC = OwnerASC.Get();

    if (!IsValid(ASC))
    {
        return;
    }

    FGameplayEventData EventData;
    EventData.EventTag = TAG_EventGimmickInteract;
    EventData.Instigator = OwnerActor;
    EventData.Target = Target;

    // 플레이어의 ASC에 대상이 담긴 이벤트 전달
    ASC->HandleGameplayEvent(TAG_EventGimmickInteract, &EventData);
}

void UInteractionComponent::UpdateInteractionTarget()
{
    CacheOwnerASC();

    AActor* OwnerActor = GetOwner();
    UWorld* World = GetWorld();

    if (!IsValid(OwnerActor) || !World || !CanUseInteractionAbility())
    {
        FocusedActor.Reset();
        UpdateInteractionHighlight();
        return;
    }

    const FVector SearchOrigin = OwnerActor->GetActorLocation();

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GimmickInteraction), false, OwnerActor);
    TArray<FOverlapResult> Overlaps;

    // 플레이어 주변 구형 범위에서 액터 수집
    World->OverlapMultiByObjectType(
        Overlaps,
        SearchOrigin,
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(InteractionDistance),
        QueryParams);

    AActor* BestTarget = nullptr;
    float BestDistanceSquared = FMath::Square(InteractionDistance);
    TSet<AActor*> CheckedActors;

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();

        if (!IsValid(Candidate) || Candidate == OwnerActor || CheckedActors.Contains(Candidate))
        {
            continue;
        }

        CheckedActors.Add(Candidate);

        const float DistanceSquared = FVector::DistSquared(SearchOrigin, Candidate->GetActorLocation());

        if (DistanceSquared >= BestDistanceSquared)
        {
            continue;
        }

        // Blueprint에서 BPI 구현 여부와 CanInteract 확인
        if (!QueryCanInteract(Candidate, OwnerActor))
        {
            continue;
        }

        // 벽을 사이에 둔 대상은 선택하지 않음
        FHitResult ObstructionHit;

        const bool bBlocked = World->LineTraceSingleByChannel(
            ObstructionHit,
            SearchOrigin,
            Candidate->GetActorLocation(),
            InteractionTraceChannel,
            QueryParams);

        if (bBlocked && ObstructionHit.GetActor() != Candidate)
        {
            continue;
        }

        BestTarget = Candidate;
        BestDistanceSquared = DistanceSquared;
    }

    FocusedActor = BestTarget;

    UpdateInteractionHighlight();
}

void UInteractionComponent::UpdateInteractionHighlight()
{
    AActor* NewHighlightedActor = CanUseInteractionAbility() ? FocusedActor.Get() : nullptr;
    AActor* PreviousActor = HighlightedActor.Get();

    // 같은 대상에게 매 프레임 같은 메시지를 보내지 않음
    if (PreviousActor == NewHighlightedActor)
    {
        return;
    }

    if (IsValid(PreviousActor))
    {
        SendHighlightMessage(PreviousActor, false);
    }

    HighlightedActor = NewHighlightedActor;

    if (IsValid(NewHighlightedActor))
    {
        SendHighlightMessage(NewHighlightedActor, true);
    }
}


void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheOwnerASC();
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 종료할 때 남아 있는 강조 표시 해제
	if (AActor* PreviousActor = HighlightedActor.Get())
	{
		SendHighlightMessage(PreviousActor, false);
	}

	HighlightedActor.Reset();
	FocusedActor.Reset();
	OwnerASC.Reset();

	Super::EndPlay(EndPlayReason);
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateInteractionTarget();
}


void UInteractionComponent::CacheOwnerASC()
{
	if (!OwnerASC.IsValid())
	{
		OwnerASC =
			UAbilitySystemBlueprintLibrary::
			GetAbilitySystemComponent(GetOwner());
	}
}

bool UInteractionComponent::CanUseInteractionAbility() const
{
    const APlayerBase* Player = Cast<APlayerBase>(GetOwner());
    UAbilitySystemComponent* ASC = OwnerASC.Get();

    if (!IsValid(Player) || Player->IsDead() || !IsValid(ASC) || !InteractionAbilityClass || !ASC->AbilityActorInfo.IsValid())
    {
        return false;
    }

    FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromClass(InteractionAbilityClass);

    if (!AbilitySpec)
    {
        return false;
    }

    UGameplayAbility* Ability = AbilitySpec->GetPrimaryInstance();

    if (!Ability)
    {
        Ability = AbilitySpec->Ability.Get();
    }

    return IsValid(Ability) && Ability->CanActivateAbility(AbilitySpec->Handle, ASC->AbilityActorInfo.Get());
}

