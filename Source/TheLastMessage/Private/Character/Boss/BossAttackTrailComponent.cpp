#include "Character/Boss/BossAttackTrailComponent.h"
#include "Character/Boss/BossMonster.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UBossAttackTrailComponent::UBossAttackTrailComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    TrailSettings.Add(EBossTrailType::MeleePhase12);
    TrailSettings.Add(EBossTrailType::MeleePhase3);
    TrailSettings.Add(EBossTrailType::JumpAttack);
    TrailSettings.Add(EBossTrailType::CrossLaserDrop);
}

void UBossAttackTrailComponent::StartForBoss(
    ABossMonster* Boss, EBossTrailType Type)
{
    if (!IsValid(Boss) || !Boss->HasAuthority()) return;
    if (auto* Trails = Boss->FindComponentByClass<UBossAttackTrailComponent>())
    {
        Trails->MulticastSetTrail(true, Type);
    }
}

void UBossAttackTrailComponent::StopForBoss(ABossMonster* Boss)
{
    if (!IsValid(Boss) || !Boss->HasAuthority()) return;
    if (auto* Trails = Boss->FindComponentByClass<UBossAttackTrailComponent>())
    {
        Trails->MulticastSetTrail(false, EBossTrailType::MeleePhase12);
    }
}

void UBossAttackTrailComponent::MulticastSetTrail_Implementation(
    bool bEnabled, EBossTrailType Type)
{
    StopLocal();
    if (!bEnabled || GetNetMode() == NM_DedicatedServer) return;

    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    USkeletalMeshComponent* Mesh = IsValid(Boss) ? Boss->GetMesh() : nullptr;
    const FBossAttackTrailSettings* Settings = TrailSettings.Find(Type);
    if (!IsValid(Mesh) || !Settings || !IsValid(Settings->System.Get())
        || Settings->SocketName.IsNone()
        || !Mesh->DoesSocketExist(Settings->SocketName))
    {
        UE_LOG(LogTemp, Warning, TEXT("보스 Trail의 Niagara 또는 소켓 설정을 확인하세요."));
        return;
    }

    ActiveTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(
        Settings->System.Get(), Mesh, Settings->SocketName,
        Settings->LocationOffset, Settings->RotationOffset,
        EAttachLocation::KeepRelativeOffset,
        true, true, ENCPoolMethod::None, false);
}

void UBossAttackTrailComponent::StopLocal()
{
    if (IsValid(ActiveTrail.Get()))
    {
        // 기존 입자는 월드에 남겨 수명이 끝날 때까지 표시합니다.
        ActiveTrail->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        ActiveTrail->SetAutoDestroy(true);
        ActiveTrail->Deactivate();
    }
    ActiveTrail = nullptr;
}

void UBossAttackTrailComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopLocal();
    Super::EndPlay(EndPlayReason);
}