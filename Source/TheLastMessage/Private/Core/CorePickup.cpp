#include "Core/CorePickup.h"

#include "Character/Player/PlayerBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "Templates/UnrealTemplate.h"

ACorePickup::ACorePickup()
{
	PrimaryActorTick.bCanEverTick = false;

	// 메시를 루트로 설정하여 액터의 배치 위치와 회전을 메시에도 적용한다.
	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	SetRootComponent(CoreMesh);

	// 메시는 외형만 표시하도록 충돌과 오버랩 감지를 끈다.
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	PickupCollision->SetupAttachment(CoreMesh);
	PickupCollision->InitSphereRadius(50.0f);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollision->SetGenerateOverlapEvents(true);

	// DefaultEngine.ini의 CorePickup 채널을 사용한다. 플레이어 캡슐만 이 채널에 Overlap으로 응답해야 한다.
	PickupCollision->SetCollisionObjectType(ECC_GameTraceChannel2);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 획득 범위에 들어온 대상을 자동 획득 처리로 연결한다.
	PickupCollision->OnComponentBeginOverlap.AddDynamic(this, &ACorePickup::OnPickupBeginOverlap);
}

void ACorePickup::OnPickupBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 코어 지급은 서버에서 처리한다.
	if (!HasAuthority())
	{
		return;
	}

	// 플레이어의 캡슐이 들어온 경우에만 획득을 요청한다.
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);
	if (!IsValid(Player) || !Player->IsPlayerControlled() || OtherComp != Player->GetCapsuleComponent())
	{
		return;
	}

	// 획득 조건 검사와 성공 후 픽업 파괴는 기존 지급 함수에서 처리한다.
	TryAcquireFixedCore(Player->GetCoreComponent());
}

bool ACorePickup::CanInteract_Implementation(AActor* Interactor) const
{
	// 지급 가능한 보상과 상호작용 대상이 준비되어 있는지 확인한다.
	if (IsActorBeingDestroyed() || bConsumed || bIsGrantingReward
		|| RewardMode != ECoreRewardMode::Fixed || !IsValid(CoreData)
		|| CoreData->CoreId.IsNone() || !IsValid(Interactor))
	{
		return false;
	}

	// 코어를 받을 수 있고, 해당 코어를 아직 보유하지 않은 액터만 허용한다.
	const UCoreComponent* CoreComponent = Interactor->FindComponentByClass<UCoreComponent>();
	return IsValid(CoreComponent) && !CoreComponent->HasCore(CoreData->CoreId);
}

void ACorePickup::Interact_Implementation(AActor* Interactor)
{
	// 요청 시점에도 조건을 확인한 뒤, 실제 지급과 소비는 획득 함수에 맡긴다.
	if (!IsValid(Interactor) || !IInteractableInterface::Execute_CanInteract(this, Interactor))
	{
		return;
	}

	TryAcquireFixedCore(Interactor->FindComponentByClass<UCoreComponent>());
}

bool ACorePickup::TryAcquireFixedCore(UCoreComponent* InCoreComponent)
{
	// 지급 가능한 고정 보상과 코어를 받을 컴포넌트가 준비되어 있어야 한다.
	// 서버 권한이 없거나 액터가 파괴 중이면 보상을 지급할 수 없다.
	if (!HasAuthority() || IsActorBeingDestroyed()
		|| RewardMode != ECoreRewardMode::Fixed || bConsumed || bIsGrantingReward
		|| !IsValid(InCoreComponent) || !IsValid(CoreData))
	{
		return false;
	}

	// 코어 획득 처리가 끝날 때까지 같은 보상의 추가 획득 요청을 막는다.
	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	// 코어 획득을 시도하며, 실패하면 보상 액터를 유지하고 false를 반환한다.
	if (!InCoreComponent->AcquireCore(CoreData))
	{
		return false;
	}

	// 획득 성공 후 소비 상태를 먼저 기록하고, 맵의 보상 액터를 파괴한다.
	bConsumed = true;
	Destroy();
	return true;
}
