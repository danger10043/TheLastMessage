#include "Core/CoreComponent.h"

#include "AbilitySystemComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "GameplayAbilitySpec.h"
#include "Templates/UnrealTemplate.h"

UCoreComponent::UCoreComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UCoreComponent::Initialize(UAbilitySystemComponent* InASC)
{
	AbilitySystemComponent = InASC;
}

bool UCoreComponent::HasCore(FName CoreId) const
{
	return GetCoreLevel(CoreId) > 0;
}

int32 UCoreComponent::GetCoreLevel(FName CoreId) const
{
	if (CoreId.IsNone())
	{
		return 0;
	}

	for (const FCoreGrantRecord& Record : AppliedCoreHandles)
	{
		if (Record.CoreId == CoreId)
		{
			return Record.Level;
		}
	}

	return 0;
}


bool UCoreComponent::CanAcquireCore(const UCoreDataAsset* CoreData) const
{
	// 유효한 데이터에서만 코어 ID와 부여할 클래스 목록을 읽는다.
	if (!IsValid(CoreData))
	{
		return false;
	}

	// 보유 기록에 사용할 ID와 능력/효과를 부여할 ASC가 필요하다.
	if (CoreData->CoreId.IsNone() || !AbilitySystemComponent.IsValid())
	{
		return false;
	}

	// 보유 중이어도 최대 단계 미만이면 다시 획득할 수 있다. 잘못된 최대 단계 설정도 거부한다.
	if (CoreData->MaxLevel < 1 || GetCoreLevel(CoreData->CoreId) >= CoreData->MaxLevel)
	{
		return false;
	}

	// GA 및 GE 클래스 참조가 유효하고, 추상 클래스가 아니며, GE는 Infinite Duration과 Stacking Type None 설정을 만족해야 한다.
	for (const TSubclassOf<UGameplayEffect>& EffectClass : CoreData->GrantedEffects)
	{
		if (!IsValid(EffectClass.Get()) || EffectClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return false;
		}

		const UGameplayEffect* EffectDefinition = EffectClass.GetDefaultObject();
		if (!IsValid(EffectDefinition)
			|| EffectDefinition->DurationPolicy != EGameplayEffectDurationType::Infinite
			|| EffectDefinition->GetStackingType() != EGameplayEffectStackingType::None)
		{
			return false;
		}
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : CoreData->GrantedAbilities)
	{
		if (!IsValid(AbilityClass.Get()) || AbilityClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return false;
		}
	}

	return true;
}

bool UCoreComponent::UpgradeCore(FCoreGrantRecord& Record)
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	check(ASC);

	// 다른 처리에서 부여 내용이 제거되었다면, 단계만 올라간 채 보상이 소비되지 않도록 거부한다.
	for (const FActiveGameplayEffectHandle& EffectHandle : Record.EffectHandles)
	{
		if (!ASC->GetActiveGameplayEffect(EffectHandle))
		{
			return false;
		}
	}
	for (const FGameplayAbilitySpecHandle& AbilityHandle : Record.AbilityHandles)
	{
		const FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
		if (!AbilitySpec || AbilitySpec->PendingRemove)
		{
			return false;
		}
	}

	const int32 NextLevel = Record.Level + 1;
	for (const FActiveGameplayEffectHandle& EffectHandle : Record.EffectHandles)
	{
		// 효과를 새로 쌓지 않고 레벨만 갱신한다. Modifier의 커브가 이 레벨에 해당하는 수치를 계산한다.
		ASC->SetActiveGameplayEffectLevel(EffectHandle, NextLevel);
	}
	for (const FGameplayAbilitySpecHandle& AbilityHandle : Record.AbilityHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(AbilityHandle))
		{
			AbilitySpec->Level = NextLevel;
			ASC->MarkAbilitySpecDirty(*AbilitySpec);
		}
	}
	Record.Level = NextLevel;
	return true;
}


void UCoreComponent::RemoveCoreGrants(const FCoreGrantRecord& Record)
{
	// ASC가 유효하고 권한이 있는 경우에만 능력과 효과를 제거한다.
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!IsValid(ASC) || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// 실행 중인 능력을 먼저 취소하여 이동/태스크 등을 정리한 뒤, 해당 능력의 등록을 제거한다.
	for (const FGameplayAbilitySpecHandle& AbilityHandle : Record.AbilityHandles)
	{
		if (AbilityHandle.IsValid())
		{
			ASC->CancelAbilityHandle(AbilityHandle);
			ASC->ClearAbility(AbilityHandle);
		}
	}

	// 이 코어의 기록에 있는 효과만 제거하여 다른 코어와 기본 스탯 효과를 유지한다.
	for (const FActiveGameplayEffectHandle& EffectHandle : Record.EffectHandles)
	{
		if (EffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(EffectHandle);
		}
	}
}


bool UCoreComponent::AcquireCore(UCoreDataAsset* CoreData)
{
	// 코어 획득 중에 다른 획득이나 초기화가 다시 시작되는 것을 막는다.
	if (bIsChangingCoreGrants || !CanAcquireCore(CoreData))
	{
		return false;
	}

	// ASC가 유효하고 권한이 있는 경우에만 능력과 효과를 부여한다.
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!IsValid(ASC) || !ASC->IsOwnerActorAuthoritative()
		|| !ASC->AbilityActorInfo.IsValid()
		|| !ASC->AbilityActorInfo->OwnerActor.IsValid()
		|| !ASC->AbilityActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	{
		TGuardValue<bool> ChangingGrantsGuard(bIsChangingCoreGrants, true);

		FCoreGrantRecord* ExistingRecord = AppliedCoreHandles.FindByPredicate(
			[CoreData](const FCoreGrantRecord& Record) { return Record.CoreId == CoreData->CoreId; });
		if (ExistingRecord)
		{
			if (!UpgradeCore(*ExistingRecord))
			{
				return false;
			}
		}
		else
		{
			FCoreGrantRecord Record;
			Record.CoreId = CoreData->CoreId;

			// 효과와 능력의 출처를 이 코어 데이터로 남긴다.
			FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
			EffectContext.AddSourceObject(CoreData);

			// 부여할 효과와 능력을 순회하며 ASC에 적용한다. 실패하면 이미 부여한 내용을 되돌린다.
			for (const TSubclassOf<UGameplayEffect>& EffectClass : CoreData->GrantedEffects)
			{
				// Infinite Duration과 Stacking Type None 설정을 만족하는 GE만 부여한다.
				const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
				if (!SpecHandle.IsValid())
				{
					RemoveCoreGrants(Record);
					return false;
				}

				const FActiveGameplayEffectHandle EffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				if (!EffectHandle.WasSuccessfullyApplied() || !EffectHandle.IsValid())
				{
					RemoveCoreGrants(Record);
					return false;
				}

				Record.EffectHandles.Add(EffectHandle);
			}

			// 효과 적용이 끝나면 능력을 등록하고, 반환된 핸들을 기록한다.
			for (const TSubclassOf<UGameplayAbility>& AbilityClass : CoreData->GrantedAbilities)
			{
				const FGameplayAbilitySpecHandle AbilityHandle = ASC->GiveAbility(
					FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, CoreData));
				if (!AbilityHandle.IsValid())
				{
					RemoveCoreGrants(Record);
					return false;
				}

				Record.AbilityHandles.Add(AbilityHandle);
			}

			// 모든 부여가 성공한 경우에만 보유 기록에 추가한다.
			AppliedCoreHandles.Add(MoveTemp(Record));
		}
	}

	OnCoreChanged.Broadcast();
	return true;
}

void UCoreComponent::ResetOwnedCores()
{
	if (bIsChangingCoreGrants)
	{
		return;
	}

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (IsValid(ASC) && !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	const bool bHadOwnedCores = !AppliedCoreHandles.IsEmpty();
	{
		TGuardValue<bool> ChangingGrantsGuard(bIsChangingCoreGrants, true);

		// 제거 알림 중에도 보유 조회가 초기화된 상태를 보도록, 기록을 먼저 별도 목록으로 옮긴다.
		const TArray<FCoreGrantRecord> RecordsToRemove = MoveTemp(AppliedCoreHandles);
		AppliedCoreHandles.Reset();
		CancelCoreChoices();

		for (const FCoreGrantRecord& Record : RecordsToRemove)
		{
			RemoveCoreGrants(Record);
		}
	}

	if (bHadOwnedCores)
	{
		OnCoreChanged.Broadcast();
	}
}


TArray<UCoreDataAsset*> UCoreComponent::GenerateCoreChoices(const TArray<UCoreDataAsset*>& CandidatePool)
{
	// 기존 보상의 선택지가 남아 있으면 다른 보상의 후보로 덮어쓰지 않는다.
	if (bIsChangingCoreGrants || bIsSelectingCore || !CurrentChoiceState.Choices.IsEmpty())
	{
		return {};
	}

	TArray<UCoreDataAsset*> EligibleCores;
	TSet<FName> CandidateIds;
	for (UCoreDataAsset* CoreData : CandidatePool)
	{
		// 무효 데이터/최대 단계/잘못된 GE·GA 설정을 제외하고, 같은 ID는 한 번만 후보에 넣는다.
		if (!CanAcquireCore(CoreData) || CandidateIds.Contains(CoreData->CoreId))
		{
			continue;
		}

		CandidateIds.Add(CoreData->CoreId);
		EligibleCores.Add(CoreData);
	}

	constexpr int32 MaxChoices = 3;
	const int32 NumChoices = FMath::Min(MaxChoices, EligibleCores.Num());
	if (NumChoices == 0) return {};

	// 새 보상의 후보와 카드별 리롤 기록을 준비한다.
	CurrentChoiceState = FCoreChoiceState();
	CurrentChoiceState.CandidatePool.Reserve(CandidatePool.Num());
	for (UCoreDataAsset* CoreData : CandidatePool)
	{
		CurrentChoiceState.CandidatePool.Add(CoreData);
	}
	CurrentChoiceState.RerollCounts.Init(0, NumChoices);

	TArray<UCoreDataAsset*> Choices;
	Choices.Reserve(NumChoices);

	// 남은 후보 중 하나를 균등하게 고르고 제거하여, 최대 3개를 중복 없이 추첨한다.
	for (int32 ChoiceIndex = 0; ChoiceIndex < NumChoices; ++ChoiceIndex)
	{
		const int32 RandomIndex = FMath::RandRange(0, EligibleCores.Num() - 1);
		UCoreDataAsset* SelectedCore = EligibleCores[RandomIndex];
		Choices.Add(SelectedCore);
		CurrentChoiceState.Choices.Add(SelectedCore);
		CurrentChoiceState.ShownCoreIds.Add(SelectedCore->CoreId);
		EligibleCores.RemoveAtSwap(RandomIndex, EAllowShrinking::No);
	}

	return Choices;
}

FCoreChoiceState UCoreComponent::SuspendCoreChoices()
{
	// 창을 닫으면 선택 기록을 해당 보상으로 옮겨 보관한다.
	FCoreChoiceState SavedState = MoveTemp(CurrentChoiceState);
	CurrentChoiceState = FCoreChoiceState();
	return SavedState;
}

TArray<UCoreDataAsset*> UCoreComponent::ResumeCoreChoices(FCoreChoiceState& SavedState)
{
	if (bIsChangingCoreGrants || bIsSelectingCore || !CurrentChoiceState.Choices.IsEmpty()
		|| SavedState.ShownCoreIds.IsEmpty()
		|| SavedState.Choices.Num() != SavedState.RerollCounts.Num()) return {};

	// 다른 보상에서 최대 단계가 된 코어는 제외하되, 나머지 카드의 순서와 리롤 사용 횟수는 함께 유지한다.
	for (int32 Index = SavedState.Choices.Num() - 1; Index >= 0; --Index)
	{
		if (!CanAcquireCore(SavedState.Choices[Index].Get()))
		{
			SavedState.Choices.RemoveAt(Index);
			SavedState.RerollCounts.RemoveAt(Index);
		}
	}
	if (SavedState.Choices.IsEmpty()) return {};

	CurrentChoiceState = MoveTemp(SavedState);
	SavedState = FCoreChoiceState();
	TArray<UCoreDataAsset*> Choices;
	for (const TObjectPtr<UCoreDataAsset>& CoreData : CurrentChoiceState.Choices)
	{
		Choices.Add(CoreData.Get());
	}
	return Choices;
}


bool UCoreComponent::SelectCore(FName CoreId)
{
	if (bIsChangingCoreGrants || bIsSelectingCore || CoreId.IsNone())
	{
		return false;
	}

	// 선택한 코어 ID와 일치하는 현재 선택지의 데이터를 찾는다.
	UCoreDataAsset* SelectedCore = nullptr;
	for (const TObjectPtr<UCoreDataAsset>& CoreData : CurrentChoiceState.Choices)
	{
		if (IsValid(CoreData.Get()) && CoreData->CoreId == CoreId)
		{
			SelectedCore = CoreData.Get();
			break;
		}
	}

	if (!SelectedCore)
	{
		return false;
	}

	// 획득 성공 알림이 발생하는 동안에도 한 보상에서 두 코어를 선택할 수 없게 한다.
	TGuardValue<bool> SelectingCoreGuard(bIsSelectingCore, true);
	if (!AcquireCore(SelectedCore))
	{
		return false;
	}

	// 획득 성공 후에만 선택 상태를 비운다. 실패한 경우에는 기존 선택지를 유지한다.
	CancelCoreChoices();
	return true;
}

void UCoreComponent::CancelCoreChoices()
{
	CurrentChoiceState = FCoreChoiceState();
}

TArray<UCoreDataAsset*> UCoreComponent::GetRerollCandidates(int32 ChoiceIndex) const
{
	if (bIsChangingCoreGrants || bIsSelectingCore
		|| !CurrentChoiceState.Choices.IsValidIndex(ChoiceIndex)
		|| !CurrentChoiceState.RerollCounts.IsValidIndex(ChoiceIndex)
		|| CurrentChoiceState.RerollCounts[ChoiceIndex] >= 1) return {};

	// 현재 카드뿐 아니라 리롤로 사라진 코어도 제외한다. 같은 ID의 후보도 한 번만 모은다.
	TSet<FName> ExcludedIds = CurrentChoiceState.ShownCoreIds;

	TArray<UCoreDataAsset*> Candidates;
	for (const TObjectPtr<UCoreDataAsset>& Data : CurrentChoiceState.CandidatePool)
	{
		if (!CanAcquireCore(Data.Get()) || ExcludedIds.Contains(Data->CoreId)) continue;
		ExcludedIds.Add(Data->CoreId);
		Candidates.Add(Data.Get());
	}
	return Candidates;
}

bool UCoreComponent::CanRerollCoreChoice(int32 ChoiceIndex) const
{
	return !GetRerollCandidates(ChoiceIndex).IsEmpty();
}

UCoreDataAsset* UCoreComponent::RerollCoreChoice(int32 ChoiceIndex)
{
	const TArray<UCoreDataAsset*> Candidates = GetRerollCandidates(ChoiceIndex);
	if (Candidates.IsEmpty()) return nullptr;

	UCoreDataAsset* Replacement = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	// 나머지 선택지와 전체 후보 풀은 그대로 유지한다.
	CurrentChoiceState.Choices[ChoiceIndex] = Replacement;
	CurrentChoiceState.ShownCoreIds.Add(Replacement->CoreId);
	++CurrentChoiceState.RerollCounts[ChoiceIndex];
	return Replacement;
}
