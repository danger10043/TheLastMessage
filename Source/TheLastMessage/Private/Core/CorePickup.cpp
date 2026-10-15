#include "Core/CorePickup.h"

#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "Templates/UnrealTemplate.h"

ACorePickup::ACorePickup()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool ACorePickup::TryAcquireFixedCore(UCoreComponent* InCoreComponent)
{
	// 지급 가능한 고정 보상과 코어를 받을 컴포넌트가 준비되어 있어야 한다.
	if (!HasAuthority() || IsActorBeingDestroyed()
		|| RewardMode != ECoreRewardMode::Fixed || bConsumed || bIsGrantingReward
		|| !IsValid(InCoreComponent) || !IsValid(CoreData))
	{
		return false;
	}

	// AcquireCore의 알림 중 다른 대상으로 같은 보상을 다시 지급하는 것을 막는다.
	TGuardValue<bool> GrantingRewardGuard(bIsGrantingReward, true);
	if (!InCoreComponent->AcquireCore(CoreData))
	{
		return false;
	}

	// 획득 성공 후에만 소비한다. 보상 액터를 숨기고 충돌을 꺼서 획득을 마무리한다.
	bConsumed = true;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	return true;
}
