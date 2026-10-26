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