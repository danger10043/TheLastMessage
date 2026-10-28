// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/DestructibleObstacle/BombDestructibleObstacle.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gimmick/PushPull/ExplosiveBomb.h"

ABombDestructibleObstacle::ABombDestructibleObstacle()
{
    PrimaryActorTick.bCanEverTick = false;

    // 플레이어의 직접 상호작용 대상이 아님
    bSupportsInteraction = false;

    ObstacleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObstacleMesh"));

    ObstacleMesh->SetupAttachment(SceneRoot);
    ObstacleMesh->SetMobility(EComponentMobility::Movable);

    // 파괴 전에는 통로를 막고 폭발 범위 검사에도 참여
    ObstacleMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    ObstacleMesh->SetGenerateOverlapEvents(false);
    ObstacleMesh->SetSimulatePhysics(false);
}

void ABombDestructibleObstacle::ReceiveBombExplosion(AExplosiveBomb * Bomb)
{
    if (!bIsEnabled || bIsDestroyed || !IsValid(Bomb) || !Bomb->HasExploded())
    {
        return;
    }

    DestroyObstacle();
}

void ABombDestructibleObstacle::DestroyObstacle()
{
    // 같은 장애물이 여러 번 파괴되지 않도록 방지
    if (bIsDestroyed)
    {
        return;
    }

    bIsDestroyed = true;

    // 상호작용 강조 및 기믹 기능 정리
    SetGimmickEnabled(false);

    if (IsValid(ObstacleMesh))
    {
        ObstacleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision
        );

        ObstacleMesh->SetVisibility(false, true);
    }

    // Blueprint에 추가된 컴포넌트까지 외형과 충돌 비활성화
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
}
