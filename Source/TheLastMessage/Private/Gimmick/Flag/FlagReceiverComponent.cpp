#include "Gimmick/Flag/FlagReceiverComponent.h"
#include "Gimmick/Flag/WorldFlagSubsystem.h"
#include "Gimmick/GimmickBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

UFlagReceiverComponent::UFlagReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UFlagReceiverComponent::IsConditionMet() const
{
	const UWorldFlagSubsystem* FlagSystem = UWorldFlagSubsystem::Get(this);
	if (!FlagSystem) return false;
	// 연출 보류 중인 플래그는 아직 꺼진 것으로 본다(원격 연출이 끝난 뒤 문이 열리게 하려고).
	const bool bMet = FlagSystem->AreFlagsPresented(Flags, MatchMode == EFlagMatchMode::All);
	return bInvert ? !bMet : bMet;
}

void UFlagReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Flags.IsEmpty())
		UE_LOG(LogTemp, Warning, TEXT("FlagReceiver: Flags가 비어 있습니다: %s"), *GetPathName());

	if (UWorldFlagSubsystem* FlagSystem = UWorldFlagSubsystem::Get(this))
	{
		FlagSystem->OnFlagChanged.AddUniqueDynamic(this, &UFlagReceiverComponent::HandleFlagChanged);
		ResyncHandle = FlagSystem->OnFlagsResynced.AddUObject(this, &UFlagReceiverComponent::HandleResync);
	}

	// 소유 기믹의 체크포인트 복원은 BeginPlay 직후 다음 틱에 실행된다.
	// 그 뒤에 현재 플래그를 적용하도록 두 틱 늦춘다(복원이 오래된 저장 상태로 덮어쓰지 못하게).
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UFlagReceiverComponent> WeakThis(this);
		World->GetTimerManager().SetTimerForNextTick([WeakThis]()
		{
			UFlagReceiverComponent* Self = WeakThis.Get();
			if (!IsValid(Self) || !Self->GetWorld()) return;
			Self->GetWorld()->GetTimerManager().SetTimerForNextTick(Self, &UFlagReceiverComponent::ApplyInitialState);
		});
	}
}

void UFlagReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorldFlagSubsystem* FlagSystem = UWorldFlagSubsystem::Get(this))
	{
		FlagSystem->OnFlagChanged.RemoveDynamic(this, &UFlagReceiverComponent::HandleFlagChanged);
		if (ResyncHandle.IsValid()) FlagSystem->OnFlagsResynced.Remove(ResyncHandle);
	}
	ResyncHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void UFlagReceiverComponent::ApplyInitialState()
{
	Apply(true);
}

void UFlagReceiverComponent::HandleResync()
{
	Apply(true);
}

void UFlagReceiverComponent::HandleFlagChanged(FGameplayTag Flag, bool bValue)
{
	if (!Flags.Contains(Flag)) return;
	// 초기 적용 전에 바뀐 값은 초기 적용이 처리한다.
	if (!bHasApplied) return;
	if (IsConditionMet() != bLastApplied) Apply(false);
}

void UFlagReceiverComponent::Apply(bool bInstant)
{
	const bool bActive = IsConditionMet();
	bHasApplied = true;
	bLastApplied = bActive;

	if (bDriveOwnerGimmick)
	{
		if (AGimmickBase* Gimmick = Cast<AGimmickBase>(GetOwner()))
		{
			// 복원/로드는 연출 없이 바로, 라이브 변화는 연출과 함께 기믹의 상태를 바꾼다.
			if (bInstant) Gimmick->RestoreLinkedActiveState(bActive);
			else Gimmick->ApplyActiveState(bActive);
		}
	}
	OnStateChanged.Broadcast(bActive, bInstant);
}
