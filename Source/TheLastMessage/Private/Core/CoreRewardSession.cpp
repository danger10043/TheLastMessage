#include "Core/CoreRewardSession.h"

#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "Templates/UnrealTemplate.h"

ECoreRewardStartResult UCoreRewardSession::Start(UCoreComponent* InCore,
	const TArray<UCoreDataAsset*>& Pool, ECoreCategory Category)
{
	if (bActive || bProcessing) return ECoreRewardStartResult::Busy;
	if (!IsValid(InCore)) return ECoreRewardStartResult::Unavailable;
	if (!InCore->TryReserveReward(this)) return ECoreRewardStartResult::Busy;
	Core = InCore;
	State = FCoreChoiceState();
	RejectedCoreIds.Reset();

	TSet<FName> CandidateIds;
	TArray<UCoreDataAsset*> Eligible;
	int32 MatchingCount = 0;
	int32 MaxedCount = 0;
	for (UCoreDataAsset* Data : Pool)
	{
		if (!IsValid(Data) || !Data->bIncludeInRandomPool || Data->CoreCategory != Category) continue;
		if (CandidateIds.Contains(Data->CoreId)) continue;
		CandidateIds.Add(Data->CoreId);
		++MatchingCount;
		const ECoreAcquireResult Result = InCore->CheckAcquire(Data);
		if (Result == ECoreAcquireResult::MaxLevel) ++MaxedCount;
		if (!IsCoreAcquireSuccess(Result)) continue;
		State.CandidatePool.Add(Data);
		Eligible.Add(Data);
	}
	if (Eligible.IsEmpty())
	{
		Finish(false);
		if (MatchingCount == 0) return ECoreRewardStartResult::NoCandidates;
		return MatchingCount == MaxedCount ? ECoreRewardStartResult::AllAtMaxLevel : ECoreRewardStartResult::Unavailable;
	}

	const int32 Count = FMath::Min(CoreRewardRules::MaxChoices, Eligible.Num());
	State.RerollCounts.Init(0, Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 RandomIndex = FMath::RandRange(0, Eligible.Num() - 1);
		UCoreDataAsset* Selected = Eligible[RandomIndex];
		State.Choices.Add(Selected);
		State.ShownCoreIds.Add(Selected->CoreId);
		Eligible.RemoveAtSwap(RandomIndex, EAllowShrinking::No);
	}
	bActive = true;
	InvalidationHandle = InCore->OnRewardInvalidated.AddUObject(this, &UCoreRewardSession::Cancel);
	return ECoreRewardStartResult::Started;
}

TArray<UCoreDataAsset*> UCoreRewardSession::GetChoices() const
{
	TArray<UCoreDataAsset*> Result;
	for (const TObjectPtr<UCoreDataAsset>& Data : State.Choices) Result.Add(Data.Get());
	return Result;
}

TArray<FCoreChoiceEntry> UCoreRewardSession::GetChoiceEntries() const
{
	TArray<FCoreChoiceEntry> Result;
	const UCoreComponent* Owner = Core.Get();
	for (const TObjectPtr<UCoreDataAsset>& Data : State.Choices)
	{
		FCoreChoiceEntry& Entry = Result.AddDefaulted_GetRef();
		Entry.CoreData = Data.Get();
		Entry.CurrentLevel = Owner && IsValid(Data.Get()) ? Owner->GetCoreLevel(Data->CoreId) : 0;
	}
	return Result;
}

ECoreAcquireResult UCoreRewardSession::SelectCore(FName CoreId)
{
	if (bProcessing) return ECoreAcquireResult::Busy;
	if (CoreId.IsNone()) return ECoreAcquireResult::InvalidData;
	if (!bActive) return ECoreAcquireResult::NotOffered;
	if (!Core.IsValid()) { Cancel(); return ECoreAcquireResult::NoAbilitySystem; }
	UCoreDataAsset* Selected = nullptr;
	for (const TObjectPtr<UCoreDataAsset>& Data : State.Choices)
	{
		if (IsValid(Data.Get()) && Data->CoreId == CoreId) { Selected = Data.Get(); break; }
	}
	if (!Selected) return ECoreAcquireResult::NotOffered;

	TGuardValue<bool> ProcessingGuard(bProcessing, true);
	const ECoreAcquireResult Result = Core->AcquireCore(Selected);
	if (IsCoreAcquireSuccess(Result))
	{
		// 획득 성공 시 화면의 페이드와 픽업 소비는 호출부가 처리한다.
		Finish(false);
	}
	else if (Result == ECoreAcquireResult::NoAbilitySystem)
	{
		Cancel();
	}
	else if (Result != ECoreAcquireResult::Busy)
	{
		RejectedCoreIds.Add(CoreId);
		RepairChoices();
	}
	return Result;
}

TArray<UCoreDataAsset*> UCoreRewardSession::GetRerollCandidates(int32 ChoiceIndex) const
{
	if (!IsActive() || bProcessing || !State.Choices.IsValidIndex(ChoiceIndex)
		|| !State.RerollCounts.IsValidIndex(ChoiceIndex)
		|| State.RerollCounts[ChoiceIndex] >= CoreRewardRules::RerollsPerCard) return {};
	return GetUnshownCandidates();
}

TArray<UCoreDataAsset*> UCoreRewardSession::GetUnshownCandidates() const
{
	if (!Core.IsValid()) return {};
	TSet<FName> Excluded = State.ShownCoreIds;
	TArray<UCoreDataAsset*> Result;
	for (const TObjectPtr<UCoreDataAsset>& Data : State.CandidatePool)
	{
		if (!IsValid(Data.Get()) || Excluded.Contains(Data->CoreId) || RejectedCoreIds.Contains(Data->CoreId)
			|| !IsCoreAcquireSuccess(Core->CheckAcquire(Data.Get()))) continue;
		Excluded.Add(Data->CoreId);
		Result.Add(Data.Get());
	}
	return Result;
}

bool UCoreRewardSession::CanRerollCoreChoice(int32 ChoiceIndex) const
{
	return !GetRerollCandidates(ChoiceIndex).IsEmpty();
}

UCoreDataAsset* UCoreRewardSession::RerollCoreChoice(int32 ChoiceIndex)
{
	const TArray<UCoreDataAsset*> Candidates = GetRerollCandidates(ChoiceIndex);
	if (Candidates.IsEmpty()) return nullptr;
	UCoreDataAsset* Replacement = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	State.Choices[ChoiceIndex] = Replacement;
	State.ShownCoreIds.Add(Replacement->CoreId);
	++State.RerollCounts[ChoiceIndex];
	return Replacement;
}

void UCoreRewardSession::RepairChoices()
{
	if (!Core.IsValid()) { Cancel(); return; }
	bool bHasChoice = false;
	for (TObjectPtr<UCoreDataAsset>& Data : State.Choices)
	{
		if (IsValid(Data.Get()) && !RejectedCoreIds.Contains(Data->CoreId))
		{
			const ECoreAcquireResult Result = Core->CheckAcquire(Data.Get());
			if (IsCoreAcquireSuccess(Result) || Result == ECoreAcquireResult::Busy) { bHasChoice = true; continue; }
			if (Result == ECoreAcquireResult::NoAbilitySystem) { Cancel(); return; }
			RejectedCoreIds.Add(Data->CoreId);
		}
		Data = nullptr;

		// 유효하지 않은 카드를 복구할 때도 이번 보상에 이미 나온 코어는 다시 제시하지 않는다.
		const TArray<UCoreDataAsset*> Replacements = GetUnshownCandidates();
		if (!Replacements.IsEmpty())
		{
			Data = Replacements[FMath::RandRange(0, Replacements.Num() - 1)];
			State.ShownCoreIds.Add(Data->CoreId);
			bHasChoice = true;
		}
	}
	if (!bHasChoice) Cancel();
}

void UCoreRewardSession::Cancel()
{
	Finish(true);
}

void UCoreRewardSession::Finish(bool bNotifyCancellation)
{
	TGuardValue<bool> ProcessingGuard(bProcessing, true);
	const bool bWasActive = bActive;
	bActive = false;
	if (UCoreComponent* Owner = Core.Get())
	{
		Owner->OnRewardInvalidated.Remove(InvalidationHandle);
		Owner->ReleaseReward(this);
	}
	InvalidationHandle.Reset();
	Core.Reset();
	State = FCoreChoiceState();
	RejectedCoreIds.Reset();
	if (bWasActive && bNotifyCancellation) OnCancelled.Broadcast();
}
