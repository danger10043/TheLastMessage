// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Carry/FuseItem.h"

bool AFuseItem::CanBePickedUp(ACharacter* Player) const
{
    return !bIsInstalled && Super::CanBePickedUp(Player);
}

void AFuseItem::OnTransferred()
{
    bIsInstalled = true;

    // 장착된 퓨즈는 독립적인 상호작용 대상에서 제외
    bSupportsInteraction = false;

    // 회로 내부에 장착된 퓨즈의 충돌 해제
    SetActorEnableCollision(false);

    Super::OnTransferred();
}

// +추가----
void AFuseItem::RestoreProgress(const FProgressActorState& State)
{
    // 이전 플레이에서 남은 장착 상태 제거
    bIsInstalled = false;

    // 바닥에 놓인 퓨즈를 다시 탐색하고 집을 수 있도록 복구
    bSupportsInteraction = true;
    SetActorEnableCollision(true);

    // 장착하면서 중지했던 위치 관리 갱신 재개
    SetActorTickEnabled(true);

    // 운반자 참조·부착·위치 및 공통 기믹 상태 복원
    Super::RestoreProgress(State);
}
// +--------
