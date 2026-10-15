#include "Core/CorePickup.h"

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

	// 충돌 탐색에 사용하되, 플레이어의 이동은 막지 않는다.
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CoreMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
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
