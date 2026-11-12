// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/DestructibleObstacle/BombDestructibleObstacle.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gimmick/PushPull/ExplosiveBomb.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

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
    
    // 장애물을 숨기기 전에 메시 중심 위치와 회전을 구한다.
    const bool bHasMesh = IsValid(ObstacleMesh.Get());

    const FVector EffectLocation = bHasMesh ? ObstacleMesh->Bounds.Origin : GetActorLocation();

    const FRotator EffectRotation = bHasMesh ? ObstacleMesh->GetComponentRotation() : GetActorRotation();

    // 기존 파괴 상태 처리
    ApplyDestroyedState(true);
    SetGimmickEnabled(false);

    // 장애물 액터와 독립적으로 생성해서 끝까지 재생한다.
    if (GetWorld() && IsValid(DestructionEffect.Get()))
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            DestructionEffect.Get(),
            EffectLocation,
            EffectRotation,
            DestructionEffectScale,
            true // 재생이 끝나면 Niagara 컴포넌트 자동 제거
        );
    }
}

FProgressActorState ABombDestructibleObstacle::CaptureProgress() const
{
    FProgressActorState State = Super::CaptureProgress();
    State.Payload = {static_cast<uint8>(bIsDestroyed)};
    return State;
}

void ABombDestructibleObstacle::RestoreProgress(const FProgressActorState& State)
{
    // Earlier saves only recorded the disable performed by DestroyObstacle.
    const bool bDestroyed = State.Payload.IsEmpty() ? !State.bEnabled : State.Payload[0] != 0;
    ApplyDestroyedState(bDestroyed);
    Super::RestoreProgress(State);
}

void ABombDestructibleObstacle::ApplyDestroyedState(bool bDestroyed)
{
    if (bIsDestroyed == bDestroyed)
    {
        return;
    }

    if (bDestroyed)
    {
        bIntactActorCollision = GetActorEnableCollision();
        bIntactActorHidden = IsHidden();
        if (ObstacleMesh)
        {
            IntactMeshCollision = ObstacleMesh->GetCollisionEnabled();
            bIntactMeshVisible = ObstacleMesh->GetVisibleFlag();
        }
    }

    bIsDestroyed = bDestroyed;
    if (ObstacleMesh)
    {
        ObstacleMesh->SetCollisionEnabled(bDestroyed ? ECollisionEnabled::NoCollision : IntactMeshCollision);
        ObstacleMesh->SetVisibility(!bDestroyed && bIntactMeshVisible);
    }
    SetActorEnableCollision(!bDestroyed && bIntactActorCollision);
    SetActorHiddenInGame(bDestroyed || bIntactActorHidden);
}
