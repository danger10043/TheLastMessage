#include "Gimmick/Flag/WorldFlagSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Save/ProgressSaveSubsystem.h"

void UWorldFlagSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UProgressSaveSubsystem>();
	if (UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>())
		GimmickRestoreHandle = Saves->OnGimmickRestoreFinished.AddUObject(this, &UWorldFlagSubsystem::ResyncReceivers);
}

void UWorldFlagSubsystem::Deinitialize()
{
	if (GimmickRestoreHandle.IsValid() && GetGameInstance())
		if (UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>())
			Saves->OnGimmickRestoreFinished.Remove(GimmickRestoreHandle);
	GimmickRestoreHandle.Reset();
	Super::Deinitialize();
}

UWorldFlagSubsystem* UWorldFlagSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UWorldFlagSubsystem>() : nullptr;
}

bool UWorldFlagSubsystem::IsFlagSet(FGameplayTag Flag) const
{
	return Flag.IsValid() && ActiveFlags.Contains(Flag.GetTagName());
}

bool UWorldFlagSubsystem::AreFlagsSet(const TArray<FGameplayTag>& Flags, bool bRequireAll) const
{
	return EvaluateFlags(Flags, bRequireAll, false);
}

bool UWorldFlagSubsystem::AreFlagsPresented(const TArray<FGameplayTag>& Flags, bool bRequireAll) const
{
	return EvaluateFlags(Flags, bRequireAll, true);
}

bool UWorldFlagSubsystem::EvaluateFlags(const TArray<FGameplayTag>& Flags, bool bRequireAll, bool bPresentedOnly) const
{
	if (Flags.IsEmpty()) return false;
	for (const FGameplayTag& Flag : Flags)
	{
		const bool bSet = IsFlagSet(Flag) && !(bPresentedOnly && DeferredFlags.Contains(Flag.GetTagName()));
		if (bRequireAll && !bSet) return false;
		if (!bRequireAll && bSet) return true;
	}
	return bRequireAll;
}

void UWorldFlagSubsystem::EndFlagDeferral(FGameplayTag Flag, bool bNotify)
{
	if (!Flag.IsValid() || DeferredFlags.Remove(Flag.GetTagName()) == 0) return;
	// 받는 쪽이 이미 로드되어 있다면 이 알림으로 열리는 연출이 재생된다.
	if (bNotify && IsFlagSet(Flag)) OnFlagChanged.Broadcast(Flag, true);
}

void UWorldFlagSubsystem::SetFlag(FGameplayTag Flag, bool bValue)
{
	if (!Flag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("WorldFlag: 유효하지 않은 태그라 플래그를 바꾸지 않았습니다."));
		return;
	}
	const FName Key = Flag.GetTagName();
	const bool bWasSet = ActiveFlags.Contains(Key);
	if (bWasSet == bValue) return;
	if (bValue) ActiveFlags.Add(Key);
	else
	{
		ActiveFlags.Remove(Key);
		DeferredFlags.Remove(Key);
	}
	// 연출 시스템이 원하면 받는 쪽에는 연출이 끝날 때까지 꺼진 것으로 보이게 한다.
	if (bValue && DeferHandler.IsBound() && DeferHandler.Execute(Flag, bValue)) DeferredFlags.Add(Key);
	OnFlagChanged.Broadcast(Flag, bValue);
}

void UWorldFlagSubsystem::RestoreFlags(const TSet<FName>& SavedFlags)
{
	ActiveFlags = SavedFlags;
	DeferredFlags.Reset();
}
