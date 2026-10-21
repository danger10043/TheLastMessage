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


ECoreAcquireResult UCoreComponent::CheckAcquire(const UCoreDataAsset* CoreData) const
{
	if (bIsChangingCoreGrants) return ECoreAcquireResult::Busy;
	// 유효한 데이터와 보유 기록에 사용할 ID가 필요하다.
	if (!IsValid(CoreData) || CoreData->CoreId.IsNone())
	{
		return ECoreAcquireResult::InvalidData;
	}

	// 능력과 효과를 부여할 ASC가 연결되어 있어야 한다.
	if (!AbilitySystemComponent.IsValid())
	{
		return ECoreAcquireResult::NoAbilitySystem;
	}

	// Custom에서만 최대 단계를 검사한다. Infinite는 MaxLevel 값과 관계없이 계속 강화할 수 있다.
	const FCoreGrantRecord* Existing = AppliedCoreHandles.FindByPredicate(
		[CoreData](const FCoreGrantRecord& Record) { return Record.CoreId == CoreData->CoreId; });
	const int32 CurrentLevel = Existing ? Existing->Level : 0;
	if (CurrentLevel == MAX_int32)
	{
		return ECoreAcquireResult::MaxLevel;
	}
	if (CoreData->UpgradeLimitType == ECoreUpgradeLimitType::Custom)
	{
		if (CoreData->MaxLevel < 1)
		{
			return ECoreAcquireResult::InvalidData;
		}
		if (CurrentLevel >= CoreData->MaxLevel)
		{
			return ECoreAcquireResult::MaxLevel;
		}
	}

	// GE는 Infinite Duration·Stacking None, GE/GA 클래스는 유효해야 한다. 에디터 데이터 검증과 같은 기준을 쓴다.
	if (!CoreData->HasValidGrantSetup())
	{
		return ECoreAcquireResult::InvalidData;
	}
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC->IsOwnerActorAuthoritative() || !ASC->AbilityActorInfo.IsValid()
		|| !ASC->AbilityActorInfo->OwnerActor.IsValid() || !ASC->AbilityActorInfo->AvatarActor.IsValid())
	{
		return ECoreAcquireResult::NoAbilitySystem;
	}

	// 유지 효과나 능력이 사라진 코어는 정상적으로 강화할 수 없으므로 후보에서도 제외한다.
	if (Existing)
	{
		for (const FActiveGameplayEffectHandle& Handle : Existing->EffectHandles)
		{
			if (!ASC->GetActiveGameplayEffect(Handle)) return ECoreAcquireResult::GrantFailed;
		}
		for (const FGameplayAbilitySpecHandle& Handle : Existing->AbilityHandles)
		{
			const FGameplayAbilitySpec* Ability = ASC->FindAbilitySpecFromHandle(Handle);
			if (!Ability || Ability->PendingRemove) return ECoreAcquireResult::GrantFailed;
		}
	}
	return CurrentLevel > 0 ? ECoreAcquireResult::Upgraded : ECoreAcquireResult::Acquired;
}

void UCoreComponent::UpgradeCore(FCoreGrantRecord& Record)
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	check(ASC);

	const int32 NextLevel = Record.Level + 1;
	for (const FActiveGameplayEffectHandle& EffectHandle : Record.EffectHandles)
	{
		// 효과를 새로 쌓지 않고 레벨만 갱신한다. GE의 수치 계산이 새 강화 단계로 다시 실행된다.
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


ECoreAcquireResult UCoreComponent::AcquireCore(UCoreDataAsset* CoreData)
{
	const ECoreAcquireResult ExpectedResult = CheckAcquire(CoreData);
	if (!IsCoreAcquireSuccess(ExpectedResult))
	{
		return ExpectedResult;
	}

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();

	const int32 OldLevel = GetCoreLevel(CoreData->CoreId);

	{
		// 획득·강화 결과를 모든 수신자에게 알릴 때까지 다른 획득이나 초기화가 끼어들지 못하게 한다.
		TGuardValue<bool> ChangingGrantsGuard(bIsChangingCoreGrants, true);

		FCoreGrantRecord* ExistingRecord = AppliedCoreHandles.FindByPredicate(
			[CoreData](const FCoreGrantRecord& Record) { return Record.CoreId == CoreData->CoreId; });
		if (ExistingRecord)
		{
			UpgradeCore(*ExistingRecord);
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
					return ECoreAcquireResult::GrantFailed;
				}

				const FActiveGameplayEffectHandle EffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				if (!EffectHandle.WasSuccessfullyApplied() || !EffectHandle.IsValid())
				{
					RemoveCoreGrants(Record);
					return ECoreAcquireResult::GrantFailed;
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
					return ECoreAcquireResult::GrantFailed;
				}

				Record.AbilityHandles.Add(AbilityHandle);
			}

			// 모든 부여가 성공한 경우에만 보유 기록에 추가한다.
			AppliedCoreHandles.Add(MoveTemp(Record));
		}

		OnCoreChanged.Broadcast(CoreData->CoreId, OldLevel, GetCoreLevel(CoreData->CoreId));
	}

	return ExpectedResult;
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

	// 초기화 알림을 받는 코드가 코어를 다시 획득하면, 남은 제거 알림과 실제 보유 상태가 달라질 수 있다.
	// 효과 제거뿐 아니라 모든 알림이 끝날 때까지 초기화 중인 상태를 유지한다.
	TGuardValue<bool> ChangingGrantsGuard(bIsChangingCoreGrants, true);

	// 첫 제거 알림부터 모든 코어가 미보유 상태로 조회되도록 기록을 먼저 비운다.
	const TArray<FCoreGrantRecord> RemovedRecords = MoveTemp(AppliedCoreHandles);
	AppliedCoreHandles.Reset();
	ActiveRewardOwner.Reset();
	OnRewardInvalidated.Broadcast();

	for (const FCoreGrantRecord& Record : RemovedRecords)
	{
		RemoveCoreGrants(Record);
	}

	// 제거가 모두 끝난 뒤 코어마다 단계가 0이 되었음을 알린다.
	for (const FCoreGrantRecord& Record : RemovedRecords)
	{
		OnCoreChanged.Broadcast(Record.CoreId, Record.Level, 0);
	}
}


bool UCoreComponent::TryReserveReward(UObject* Requester)
{
	if (!IsValid(Requester) || bIsChangingCoreGrants || ActiveRewardOwner.IsValid()) return false;
	ActiveRewardOwner = Requester;
	return true;
}

void UCoreComponent::ReleaseReward(const UObject* Requester)
{
	if (ActiveRewardOwner.Get() == Requester) ActiveRewardOwner.Reset();
}

void UCoreComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TGuardValue<bool> ChangingGrantsGuard(bIsChangingCoreGrants, true);
	ActiveRewardOwner.Reset();
	OnRewardInvalidated.Broadcast();
	Super::EndPlay(EndPlayReason);
}
