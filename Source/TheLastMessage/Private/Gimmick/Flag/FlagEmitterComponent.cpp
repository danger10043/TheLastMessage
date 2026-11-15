#include "Gimmick/Flag/FlagEmitterComponent.h"
#include "Gimmick/Flag/WorldFlagSubsystem.h"
#include "Gimmick/GimmickBase.h"

UFlagEmitterComponent::UFlagEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFlagEmitterComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Flag.IsValid())
		UE_LOG(LogTemp, Warning, TEXT("FlagEmitter: Flag 태그가 비어 있습니다: %s"), *GetPathName());
	if (AGimmickBase* Gimmick = Cast<AGimmickBase>(GetOwner()))
		Gimmick->OnActiveStateChanged.AddUniqueDynamic(this, &UFlagEmitterComponent::HandleOwnerActiveStateChanged);
}

void UFlagEmitterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AGimmickBase* Gimmick = Cast<AGimmickBase>(GetOwner()))
		Gimmick->OnActiveStateChanged.RemoveDynamic(this, &UFlagEmitterComponent::HandleOwnerActiveStateChanged);
	Super::EndPlay(EndPlayReason);
}

void UFlagEmitterComponent::HandleOwnerActiveStateChanged(AGimmickBase* Source, bool bActive)
{
	// 체크포인트 복원 중의 상태 변화는 이 이벤트로 오지 않는다. 플래그는 세이브에서 따로 복원된다.
	SetFlagState(bActive);
}

void UFlagEmitterComponent::SetFlagState(bool bValue)
{
	if (Mode == EFlagEmitMode::LatchOn && !bValue) return;
	if (UWorldFlagSubsystem* Flags = UWorldFlagSubsystem::Get(this))
		Flags->SetFlag(Flag, bValue);
}
