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
	// 유효한 ID가 없으면 보유 여부를 확인할 수 없으므로 false를 반환한다.
	if (CoreId.IsNone())
	{
		return false;
	}

	// 보유 기록을 순회하며 같은 코어 ID가 있는지 검사한다.
	for (const FCoreGrantRecord& Record : AppliedCoreHandles)
	{
		if (Record.CoreId == CoreId)
		{
			return true;
		}
	}

	return false;
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

	// 같은 코어의 능력과 효과가 다시 부여되는 것을 막는다.
	if (HasCore(CoreData->CoreId))
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

		// 보유 효과를 코어별로 제거할 수 있도록, 적용 전에 Infinite와 Stacking Type None 설정을 확인한다.
		for (const TSubclassOf<UGameplayEffect>& EffectClass : CoreData->GrantedEffects)
		{
			const UGameplayEffect* EffectDefinition = EffectClass.GetDefaultObject();
			if (EffectClass->HasAnyClassFlags(CLASS_Abstract)
				|| !IsValid(EffectDefinition)
				|| EffectDefinition->DurationPolicy != EGameplayEffectDurationType::Infinite
				|| EffectDefinition->GetStackingType() != EGameplayEffectStackingType::None)
			{
				return false;
			}
		}

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : CoreData->GrantedAbilities)
		{
			if (AbilityClass->HasAnyClassFlags(CLASS_Abstract))
			{
				return false;
			}
		}

	
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

		// 
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
		CurrentCandidatePool.Reset();
		CurrentChoices.Reset();

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
	if (bIsChangingCoreGrants || bIsSelectingCore || !CurrentChoices.IsEmpty())
	{
		return {};
	}

	// 원본 후보는 재추첨에 사용하고, 필터링과 추첨은 별도의 목록에서 진행한다.
	CurrentCandidatePool.Reset(CandidatePool.Num());
	for (UCoreDataAsset* CoreData : CandidatePool)
	{
		CurrentCandidatePool.Add(CoreData);
	}

	TArray<UCoreDataAsset*> EligibleCores;
	TSet<FName> CandidateIds;
	for (UCoreDataAsset* CoreData : CandidatePool)
	{
		// 무효 데이터/빈 ID/보유 코어/잘못된 GE·GA 설정을 제외하고, 같은 ID는 한 번만 후보에 넣는다.
		if (!CanAcquireCore(CoreData) || CandidateIds.Contains(CoreData->CoreId))
		{
			continue;
		}

		CandidateIds.Add(CoreData->CoreId);
		EligibleCores.Add(CoreData);
	}

	constexpr int32 MaxChoices = 3;
	const int32 NumChoices = FMath::Min(MaxChoices, EligibleCores.Num());
	TArray<UCoreDataAsset*> Choices;
	Choices.Reserve(NumChoices);

	// 남은 후보 중 하나를 균등하게 고르고 제거하여, 최대 3개를 중복 없이 추첨한다.
	for (int32 ChoiceIndex = 0; ChoiceIndex < NumChoices; ++ChoiceIndex)
	{
		const int32 RandomIndex = FMath::RandRange(0, EligibleCores.Num() - 1);
		UCoreDataAsset* SelectedCore = EligibleCores[RandomIndex];
		Choices.Add(SelectedCore);
		CurrentChoices.Add(SelectedCore);
		EligibleCores.RemoveAtSwap(RandomIndex, EAllowShrinking::No);
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
	for (const TObjectPtr<UCoreDataAsset>& CoreData : CurrentChoices)
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
	CurrentChoices.Reset();
	CurrentCandidatePool.Reset();
	return true;
}

TArray<UCoreDataAsset*> UCoreComponent::RerollCoreChoices()
{
	if (bIsChangingCoreGrants || bIsSelectingCore || CurrentChoices.IsEmpty())
	{
		return {};
	}

	// 원본 후보를 복사한 뒤 기존 선택지를 비우고, 생성 함수에서 보유 상태와 설정을 다시 검사한다.
	TArray<UCoreDataAsset*> CandidatePool;
	CandidatePool.Reserve(CurrentCandidatePool.Num());
	for (const TObjectPtr<UCoreDataAsset>& CoreData : CurrentCandidatePool)
	{
		CandidatePool.Add(CoreData.Get());
	}

	CurrentChoices.Reset();
	return GenerateCoreChoices(CandidatePool);
}
