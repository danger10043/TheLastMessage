// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Component/InteractionComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Character/Player/PlayerBase.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Gimmick/GAS/GA_InteractGimmick.h"
#include "NativeGameplayTags.h"
#include "Gimmick/Component/CarryComponent.h"
#include "Gimmick/Carry/FuseItem.h"
#include "Gimmick/FuseCircuit/FuseCircuitSocket.h"
#include "Gimmick/Component/PushPullComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EventGimmickInteract, "Event.Gimmick.Interact");

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}

void UInteractionComponent::RequestInteract()
{
    AActor* OwnerActor = GetOwner();
    APlayerBase* Player = Cast<APlayerBase>(OwnerActor);

    if (!IsValid(Player) || Player->IsDead() || Player->IsHitReacting())
    {
        return;
    }

    UPushPullComponent* PushPullComponent = Player->FindComponentByClass<UPushPullComponent>();

    // 밀기·당기기 중에는 다른 상호작용보다 조작 해제를 먼저 처리
    if (IsValid(PushPullComponent) && PushPullComponent->IsPushPulling())
    {
        PushPullComponent->StopPushPull();

        // 해제 후 주변 상호작용 대상과 강조 표시 갱신
        UpdateInteractionTarget();

        return;
    }

    UCarryComponent* CarryComponent = Player->FindComponentByClass<UCarryComponent>();

    // 입력한 순간의 주변 대상과 삽입 가능 여부 확인
    UpdateInteractionTarget();

    AActor* Target = FocusedActor.Get();

    // 물체를 들고 있으면 내려놓기만 시도
    if (IsValid(CarryComponent) && CarryComponent->HasCarriedObject())
    {
        const bool bHasFuseSocketTarget = IsValid(Target) && Target->IsA<AFuseCircuitSocket>();

        // 삽입할 회로가 없다면 기존처럼 내려놓기
        if (!bHasFuseSocketTarget)
        {
            CarryComponent->DropCarriedObject();
            return;
        }
    }

    if (!IsValid(Target) || !CanUseInteractionAbility())
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

    // 기존 상호작용 능력을 통해 회로의 삽입 함수 실행
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

    const UCarryComponent* CarryComponent = OwnerActor->FindComponentByClass<UCarryComponent>();

    const bool bHasCarriedObject = IsValid(CarryComponent) && CarryComponent->HasCarriedObject();

    const FVector SearchOrigin = OwnerActor->GetActorLocation();

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GimmickInteraction), false, OwnerActor);

    // 들고 있는 물체가 회로까지의 시야 검사를 막지 않도록 제외
    if (bHasCarriedObject)
    {
        QueryParams.AddIgnoredActor(CarryComponent->GetCarriedObject());
    }

    TArray<FOverlapResult> Overlaps;

    World->OverlapMultiByObjectType(
        Overlaps, 
        SearchOrigin, 
        FQuat::Identity, 
        ObjectParams, 
        FCollisionShape::MakeSphere(InteractionDistance), 
        QueryParams
    );

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

        // 운반 중에는 퓨즈 회로만 대상으로 허용
        if (bHasCarriedObject && !Candidate->IsA<AFuseCircuitSocket>())
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(SearchOrigin, Candidate->GetActorLocation());

        if (DistanceSquared >= BestDistanceSquared)
        {
            continue;
        }

        // 회로의 경우 퓨즈 삽입 가능 여부까지 확인
        if (!QueryCanInteract(Candidate, OwnerActor))
        {
            continue;
        }

        FHitResult ObstructionHit;

        const bool bBlocked = World->LineTraceSingleByChannel(
                                                ObstructionHit,
                                                SearchOrigin,
                                                Candidate->GetActorLocation(),
                                                InteractionTraceChannel,
                                                QueryParams);

        // 벽을 사이에 둔 대상은 선택하지 않음
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

    // 운반 중에는 일반 상호작용 차단
    const UCarryComponent* CarryComponent = Player->FindComponentByClass<UCarryComponent>();

    // 운반 중에는 정상적으로 들고 있는 퓨즈만 예외 허용
    if (IsValid(CarryComponent) && CarryComponent->HasCarriedObject())
    {
        const AFuseItem* Fuse = Cast<AFuseItem>(CarryComponent->GetCarriedObject());

        if (!IsValid(Fuse) || Fuse->IsInstalled() || Fuse->GetCarrier() != Player)
        {
            return false;
        }
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
