#include "Character/Boss/BossActivationCore.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

ABossActivationCore::ABossActivationCore()
{
    PrimaryActorTick.bCanEverTick = false;

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(
        TEXT("CollisionComponent"));
    SetRootComponent(CollisionComponent);

    // 퍼즐 충돌 처리 구현 전에는 충돌을 활성화하지 않는다.
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CollisionComponent->SetGenerateOverlapEvents(false);

    CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
    CoreMesh->SetupAttachment(CollisionComponent);
    CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABossActivationCore::OnCoreOverlap(
    UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 기반 구성 단계의 정의다. 미사일 검증과 코어 비활성화는 아직 미구현이다.
    // HandleMissileHit() 본문을 구현한 뒤 이곳에서 연결한다.
}