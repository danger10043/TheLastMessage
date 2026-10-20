// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/GAS/GA_InteractGimmick.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Character/Player/PlayerBase.h"

UGA_InteractGimmick::UGA_InteractGimmick()
{
    // 플레이어별 능력 인스턴스가 대상 정보를 보관
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_InteractGimmick::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    TargetActor.Reset();

    // 이 능력은 대상이 담긴 GAS 이벤트로 실행
    if (!ActorInfo || !TriggerEventData)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    APlayerBase* Player = Cast<APlayerBase>(ActorInfo->AvatarActor.Get());

    if (!IsValid(Player) || Player->IsDead())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 이벤트 데이터는 const Actor 참조를 제공하지만,
    // 상호작용 메시지는 변경 가능한 Actor 참조를 사용
    AActor* RequestedTarget = const_cast<AActor*>(TriggerEventData->Target.Get());

    if (!IsValid(RequestedTarget) || RequestedTarget == Player)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    TargetActor = RequestedTarget;

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        TargetActor.Reset();

        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // BPI 조건 확인과 Interact 호출은 자식 Blueprint에서 구현
    SendInteractionMessage(RequestedTarget, Player);

    TargetActor.Reset();

    // 한 번의 상호작용 요청을 처리했으므로 능력 종료
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
