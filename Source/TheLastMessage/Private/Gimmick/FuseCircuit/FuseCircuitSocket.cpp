// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/FuseCircuit/FuseCircuitSocket.h"
#include "EngineUtils.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Gimmick/Carry/FuseItem.h"
#include "Gimmick/Component/CarryComponent.h"

AFuseCircuitSocket::AFuseCircuitSocket()
{
    // 플레이어가 퓨즈를 들고 직접 상호작용하는 대상
    bSupportsInteraction = true;

    CircuitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CircuitMesh"));

    CircuitMesh->SetupAttachment(SceneRoot);
    CircuitMesh->SetMobility(EComponentMobility::Movable);
    CircuitMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    FuseMountPoint = CreateDefaultSubobject<USceneComponent>(TEXT("FuseMountPoint"));

    FuseMountPoint->SetupAttachment(CircuitMesh);
    FuseMountPoint->SetMobility(EComponentMobility::Movable);
}

bool AFuseCircuitSocket::CanInteractInternal(ACharacter* Player) const
{
    // 사용 불가, 활성화 완료, 삽입 처리 중인 회로는 차단
    if (!Super::CanInteractInternal(Player) || bIsActive || bIsInserting || IsValid(InstalledFuse) || !IsValid(FuseMountPoint))
    {
        return false;
    }

    const APlayerBase* PlayerCharacter = Cast<APlayerBase>(Player);

    if (!IsValid(PlayerCharacter) || PlayerCharacter->IsDead() || PlayerCharacter->IsHitReacting())
    {
        return false;
    }

    const UCarryComponent* CarryComponent = PlayerCharacter->FindComponentByClass<UCarryComponent>();

    if (!IsValid(CarryComponent))
    {
        return false;
    }

    // 태그만이 아니라 실제로 들고 있는 아이템의 종류 확인
    const AFuseItem* Fuse = Cast<AFuseItem>(CarryComponent->GetCarriedObject());

    return IsValid(Fuse) && !Fuse->IsInstalled() && Fuse->GetCarrier() == Player;
}

void AFuseCircuitSocket::PerformInteraction(ACharacter* Player)
{
    TryInsertFuse(Player);
}

bool AFuseCircuitSocket::TryInsertFuse(ACharacter * Player)
{
    if (!HasAuthority() || !CanInteractInternal(Player))
    {
        return false;
    }

    UCarryComponent* CarryComponent = Player->FindComponentByClass<UCarryComponent>();

    if (!IsValid(CarryComponent))
    {
        return false;
    }

    AFuseItem* Fuse = Cast<AFuseItem>(CarryComponent->GetCarriedObject());

    if (!IsValid(Fuse))
    {
        return false;
    }

    // 함수가 종료되면 자동으로 원래 값으로 복구
    TGuardValue<bool> InsertingGuard(bIsInserting, true);

    // 장착 위치로 전달하고 운반 상태와 효과 정리
    if (!CarryComponent->TransferCarriedObject(FuseMountPoint))
    {
        return false;
    }

    if (!IsValid(Fuse))
    {
        return false;
    }

    InstalledFuse = Fuse;

    // 삽입이 끝난 회로는 상호작용 강조 해제
    UpdateInteractionHighlight(false);

    // 활성 상태 기록과 연결 장치 알림
    ApplyActiveState(true);

    return true;
}

FProgressActorState AFuseCircuitSocket::CaptureProgress() const
{
    FProgressActorState State = Super::CaptureProgress();
    if (IsValid(InstalledFuse)) State.RelatedActorId = InstalledFuse->GetPersistentId();
    return State;
}

void AFuseCircuitSocket::FinishRestoreProgress(const FProgressActorState& State)
{
    InstalledFuse = nullptr;
    bIsInserting = false;
    if (State.RelatedActorId.IsValid())
        for (TActorIterator<AFuseItem> It(GetWorld()); It; ++It)
            if (It->GetPersistentId() == State.RelatedActorId)
            {
                InstalledFuse = *It;
                InstalledFuse->GetObjectMesh()->SetSimulatePhysics(false);
                InstalledFuse->AttachToComponent(FuseMountPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                InstalledFuse->OnTransferred();
                break;
            }
    Super::FinishRestoreProgress(State);
}
