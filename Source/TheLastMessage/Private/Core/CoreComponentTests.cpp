#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Core/CoreComponent.h"
#include "Core/CoreRewardSession.h"
#include "CoreComponentTestObserver.h"
#include "DataAsset/CoreDataAsset.h"
#include "Engine/CurveTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GAS/CombatAttributeSet.h"
#include "Misc/DataValidation.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#endif

namespace
{
	/** 코어 테스트 공용 환경: 게임 월드, ASC를 가진 Actor, 초기화된 CoreComponent. 소멸 시 부여 내용을 정리하고 월드를 닫는다. */
	struct FCoreTestFixture
	{
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		UCombatAttributeSet* Attributes = nullptr;
		UCoreComponent* Core = nullptr;
		TStrongObjectPtr<UCoreRewardSession> Reward;

		explicit FCoreTestFixture(bool bWithCombatAttributes) : Reward(NewObject<UCoreRewardSession>())
		{
			UWorld::InitializationValues Values;
			Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Owner = World->SpawnActor<AActor>();
			ASC = NewObject<UAbilitySystemComponent>(Owner);
			Owner->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Owner, Owner);
			if (bWithCombatAttributes)
			{
				Attributes = NewObject<UCombatAttributeSet>(Owner);
				ASC->AddAttributeSetSubobject(Attributes);
			}
			Core = NewObject<UCoreComponent>(Owner);
			Owner->AddInstanceComponent(Core);
			Core->RegisterComponent();
			Core->Initialize(ASC);
		}

		TArray<UCoreDataAsset*> GenerateChoices(const TArray<UCoreDataAsset*>& Pool, ECoreCategory Category)
		{
			return Reward->Start(Core, Pool, Category) == ECoreRewardStartResult::Started ? Reward->GetChoices() : TArray<UCoreDataAsset*>();
		}

		~FCoreTestFixture()
		{
			if (Core) Core->ResetOwnedCores();
			if (World)
			{
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
			}
		}

		UE_NONCOPYABLE(FCoreTestFixture);
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreUpgradeTest, "TheLastMessage.Core.Upgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreUpgradeTest::RunTest(const FString& Parameters)
{
	// 테스트 설정은 메모리에서만 변경하고 종료 시 기본값을 복구한다.
	// 픽스처보다 먼저 선언해, 픽스처가 GE를 제거할 때까지 변경한 Modifier가 유지되도록 한다.
	UGameplayEffect* Effect = GetMutableDefault<UGameplayEffect>();
	TGuardValue<EGameplayEffectDurationType> DurationGuard(Effect->DurationPolicy, EGameplayEffectDurationType::Infinite);
	TGuardValue<TArray<FGameplayModifierInfo>> ModifiersGuard(Effect->Modifiers, {});
	FCoreTestFixture Fixture(true);
	UAbilitySystemComponent* ASC = Fixture.ASC;
	UCombatAttributeSet* Attributes = Fixture.Attributes;
	UCoreComponent* Core = Fixture.Core;

	UCurveTable* Curves = NewObject<UCurveTable>();
	const FName CurveRow(TEXT("DamageMultiplier"));
	FRichCurve& Curve = Curves->AddRichCurve(CurveRow);
	Curve.AddKey(1.0f, 1.15f);
	Curve.AddKey(2.0f, 1.30f);
	Curve.AddKey(3.0f, 1.45f);
	FScalableFloat Magnitude;
	Magnitude.SetScalingValue(1.0f, CurveRow, Curves);
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UCombatAttributeSet::GetDamageDealtMultiplierAttribute();
	Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Effect->Modifiers.Add(Modifier);

	UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
	Data->CoreId = TEXT("Test_Upgrade");
	Data->MaxLevel = 3;
	Data->GrantedEffects.Add(UGameplayEffect::StaticClass());
	Data->GrantedAbilities.Add(UGameplayAbility::StaticClass());
	const TArray<UCoreDataAsset*> Pool = { Data, Data };
	TestEqual(TEXT("Unowned core is level zero"), Core->GetCoreLevel(Data->CoreId), 0);
	TestFalse(TEXT("Empty ID is not owned"), Core->HasCore(NAME_None));
	TestTrue(TEXT("First acquisition succeeds"), Core->AcquireCore(Data) == ECoreAcquireResult::Acquired);
	TestEqual(TEXT("First acquisition is level one"), Core->GetCoreLevel(Data->CoreId), 1);
	TArray<FOwnedCoreEntry> Owned = Core->GetOwnedCores();
	TestEqual(TEXT("Ownership query lists acquired definitions only"), Owned.Num(), 1);
	TestTrue(TEXT("Ownership query retains the acquired DA"), !Owned.IsEmpty() && Owned[0].CoreData == Data);
	if (!Owned.IsEmpty()) Owned[0].Level = 99;
	TestEqual(TEXT("Editing a UI snapshot cannot change real ownership"), Core->GetCoreLevel(Data->CoreId), 1);
	TestTrue(TEXT("Level-one curve is applied"), FMath::IsNearlyEqual(Attributes->GetDamageDealtMultiplier(), 1.15f));
	TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(FGameplayEffectQuery());
	TestEqual(TEXT("One GE is applied"), Handles.Num(), 1);
	const FActiveGameplayEffectHandle FirstHandle = Handles.IsEmpty() ? FActiveGameplayEffectHandle() : Handles[0];
	const FGameplayAbilitySpecHandle AbilityHandle = ASC->GetActivatableAbilities().IsEmpty()
		? FGameplayAbilitySpecHandle() : ASC->GetActivatableAbilities()[0].Handle;

	// 보유 중인 강화 후보는 유지하되, 같은 ID를 선택지에 두 번 넣지는 않는다.
	TestEqual(TEXT("Owned upgradeable core remains a unique candidate"), Fixture.GenerateChoices(Pool, ECoreCategory::Stat).Num(), 1);
	Fixture.Reward->Cancel();
	TestTrue(TEXT("Reacquisition upgrades"), Core->AcquireCore(Data) == ECoreAcquireResult::Upgraded);
	TestEqual(TEXT("Reacquisition is level two"), Core->GetCoreLevel(Data->CoreId), 2);
	TestEqual(TEXT("Upgrade updates the existing list entry"), Core->GetOwnedCores().Num(), 1);
	TestEqual(TEXT("Ownership query returns the upgraded level"), Core->GetOwnedCores()[0].Level, 2);
	TestTrue(TEXT("Curve replaces the old magnitude instead of stacking"), FMath::IsNearlyEqual(Attributes->GetDamageDealtMultiplier(), 1.30f));
	Handles = ASC->GetActiveEffects(FGameplayEffectQuery());
	TestTrue(TEXT("The same effect handle is retained"), Handles.Num() == 1 && Handles[0] == FirstHandle);
	TestEqual(TEXT("Upgrade does not duplicate abilities"), ASC->GetActivatableAbilities().Num(), 1);
	const FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
	TestTrue(TEXT("Existing ability level is updated"), AbilitySpec && AbilitySpec->Level == 2);
	TestEqual(TEXT("Upgraded core remains eligible for a new reward"), Fixture.GenerateChoices(Pool, ECoreCategory::Stat).Num(), 1);
	Fixture.Reward->Cancel();

	TestTrue(TEXT("Final upgrade succeeds"), Core->AcquireCore(Data) == ECoreAcquireResult::Upgraded);
	TestEqual(TEXT("Final level is capped"), Core->GetCoreLevel(Data->CoreId), 3);
	TestTrue(TEXT("Final curve value is applied"), FMath::IsNearlyEqual(Attributes->GetDamageDealtMultiplier(), 1.45f));
	TestTrue(TEXT("Acquisition past the maximum is rejected"), Core->AcquireCore(Data) == ECoreAcquireResult::MaxLevel);
	TestEqual(TEXT("Rejected acquisition preserves level"), Core->GetCoreLevel(Data->CoreId), 3);
	TestTrue(TEXT("Maxed core is excluded from new choices"), Fixture.GenerateChoices(Pool, ECoreCategory::Stat).IsEmpty());

	Core->ResetOwnedCores();
	TestEqual(TEXT("Reset clears levels"), Core->GetCoreLevel(Data->CoreId), 0);
	TestTrue(TEXT("Reset empties the owned list"), Core->GetOwnedCores().IsEmpty());
	TestFalse(TEXT("Reset clears ownership"), Core->HasCore(Data->CoreId));
	TestTrue(TEXT("Reset removes upgraded effects"), ASC->GetActiveEffects(FGameplayEffectQuery()).IsEmpty());
	TestTrue(TEXT("Reset restores the base attribute"), FMath::IsNearlyEqual(Attributes->GetDamageDealtMultiplier(), 1.0f));
	TestTrue(TEXT("Reset removes upgraded abilities"), ASC->GetActivatableAbilities().IsEmpty());

	// 부여 내용이 외부에서 제거된 경우, 존재하지 않는 효과를 강화한 것으로 기록하지 않는다.
	TestTrue(TEXT("Core can be reacquired after reset"), Core->AcquireCore(Data) == ECoreAcquireResult::Acquired);
	for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
	{
		ASC->RemoveActiveGameplayEffect(Handle);
	}
	TestTrue(TEXT("Missing effect rejects upgrade"), Core->AcquireCore(Data) == ECoreAcquireResult::GrantFailed);
	TestEqual(TEXT("Failed upgrade preserves the old level"), Core->GetCoreLevel(Data->CoreId), 1);
	Core->ResetOwnedCores();

	// Infinite 설정은 숨겨진 MaxLevel 값과 관계없이 동작해야 한다.
	Data->UpgradeLimitType = ECoreUpgradeLimitType::Infinite;
	Data->MaxLevel = 1;
	for (int32 Level = 1; Level <= 10; ++Level)
	{
		TestTrue(TEXT("Infinite ignores the custom maximum"), IsCoreAcquireSuccess(Core->AcquireCore(Data)));
	}
	TestEqual(TEXT("Infinite records the actual upgrade level"), Core->GetCoreLevel(Data->CoreId), 10);
	TestEqual(TEXT("Infinite keeps one effect instead of stacking"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 1);
	TestEqual(TEXT("Infinite remains eligible for future rewards"), Fixture.GenerateChoices(Pool, ECoreCategory::Stat).Num(), 1);
	Core->ResetOwnedCores();

	// 치명타는 커브의 추가 확률을 적용한다. 최대 단계 이후에는 더 획득할 수 없다.
	Attributes->InitCriticalChance(0.2f);
	const FName CriticalRow(TEXT("CriticalBonus"));
	FRichCurve& CriticalCurve = Curves->AddRichCurve(CriticalRow);
	CriticalCurve.AddKey(1.0f, 0.05f);
	CriticalCurve.AddKey(2.0f, 0.10f);
	CriticalCurve.AddKey(3.0f, 0.15f);
	CriticalCurve.AddKey(4.0f, 0.20f);
	CriticalCurve.AddKey(5.0f, 0.25f);
	FScalableFloat CriticalMagnitude;
	CriticalMagnitude.SetScalingValue(1.0f, CriticalRow, Curves);
	FGameplayModifierInfo CriticalModifier;
	CriticalModifier.Attribute = UCombatAttributeSet::GetCriticalChanceAttribute();
	CriticalModifier.ModifierOp = EGameplayModOp::Additive;
	CriticalModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(CriticalMagnitude);
	Effect->Modifiers = { CriticalModifier };
	Data->UpgradeLimitType = ECoreUpgradeLimitType::Custom;
	Data->MaxLevel = 5;
	TestTrue(TEXT("Critical core is acquired"), Core->AcquireCore(Data) == ECoreAcquireResult::Acquired);
	TestEqual(TEXT("Critical first level adds five percentage points"), Attributes->GetCriticalChance(), 0.25f, 0.0001f);
	TestTrue(TEXT("Critical core upgrades"), Core->AcquireCore(Data) == ECoreAcquireResult::Upgraded);
	TestEqual(TEXT("Critical second level adds ten percentage points"), Attributes->GetCriticalChance(), 0.30f, 0.0001f);
	for (int32 Level = 3; Level <= 5; ++Level)
	{
		TestTrue(TEXT("Critical upgrade within the limit succeeds"), Core->AcquireCore(Data) == ECoreAcquireResult::Upgraded);
	}
	TestEqual(TEXT("Critical final level adds twenty-five percentage points"), Attributes->GetCriticalChance(), 0.45f, 0.0001f);
	TestTrue(TEXT("Critical sixth acquisition is rejected"), Core->AcquireCore(Data) == ECoreAcquireResult::MaxLevel);
	TestEqual(TEXT("Rejected critical upgrade preserves level five"), Core->GetCoreLevel(Data->CoreId), 5);
	Handles = ASC->GetActiveEffects(FGameplayEffectQuery());
	TestEqual(TEXT("Critical upgrades retain a single effect"), Handles.Num(), 1);
	ASC->SetNumericAttributeBase(UCombatAttributeSet::GetCriticalChanceAttribute(), 0.4f);
	TestEqual(TEXT("Critical bonus remains additive when the base changes"), Attributes->GetCriticalChance(), 0.65f, 0.0001f);
	Core->ResetOwnedCores();
	TestTrue(TEXT("Removing critical core preserves the external base value"), FMath::IsNearlyEqual(Attributes->GetCriticalChance(), 0.4f));

	UCoreDataAsset* SingleLevel = NewObject<UCoreDataAsset>();
	SingleLevel->CoreId = TEXT("Test_SingleLevel");
	TestTrue(TEXT("Existing assets default to Custom"), SingleLevel->UpgradeLimitType == ECoreUpgradeLimitType::Custom);
	TestEqual(TEXT("Existing assets default to one level"), SingleLevel->MaxLevel, 1);
	TestTrue(TEXT("Single-level core can be acquired"), Core->AcquireCore(SingleLevel) == ECoreAcquireResult::Acquired);
	TestTrue(TEXT("Single-level core cannot be acquired twice"), Core->AcquireCore(SingleLevel) == ECoreAcquireResult::MaxLevel);
	UCoreDataAsset* Invalid = NewObject<UCoreDataAsset>();
	Invalid->CoreId = TEXT("Test_InvalidMaximum");
	Invalid->MaxLevel = 0;
	TestTrue(TEXT("Invalid maximum is rejected at runtime"), Core->AcquireCore(Invalid) == ECoreAcquireResult::InvalidData);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreRewardPoolTest, "TheLastMessage.Core.RewardPools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreRewardPoolTest::RunTest(const FString& Parameters)
{
	FCoreTestFixture Fixture(false);
	UCoreComponent* Core = Fixture.Core;

	TArray<UCoreDataAsset*> Pool;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
		Data->CoreId = FName(*FString::Printf(TEXT("Test_Pool_%d"), Index));
		Data->CoreCategory = Index < 5 ? ECoreCategory::Stat : ECoreCategory::Ability;
		Pool.Add(Data);
	}
	UCoreDataAsset* Maxed = Pool[4];
	TestTrue(TEXT("Prepare a maxed stat core"), Core->AcquireCore(Maxed) == ECoreAcquireResult::Acquired);
	UCoreDataAsset* FixedOnly = NewObject<UCoreDataAsset>();
	FixedOnly->CoreId = TEXT("Test_FixedOnly");
	FixedOnly->CoreCategory = ECoreCategory::Ability;
	FixedOnly->bIncludeInRandomPool = false;
	Pool.Add(FixedOnly);
	UCoreDataAsset* Duplicate = Pool[0];
	Pool.Add(Duplicate);
	Pool.Add(nullptr);

	TArray<UCoreDataAsset*> Choices = Fixture.GenerateChoices(Pool, ECoreCategory::Stat);
	TestEqual(TEXT("Stat pool produces three cards"), Choices.Num(), 3);
	TSet<FName> OriginalIds;
	for (const UCoreDataAsset* Data : Choices)
	{
		TestTrue(TEXT("Stat choices do not include abilities or maxed cores"), Data->CoreCategory == ECoreCategory::Stat && Data != Maxed);
		OriginalIds.Add(Data->CoreId);
	}
	TestEqual(TEXT("Duplicate source entries do not duplicate cards"), OriginalIds.Num(), 3);
	UCoreDataAsset* Replacement = Fixture.Reward->RerollCoreChoice(0);
	TestTrue(TEXT("Reroll stays in the same pool and does not repeat a shown core"),
		Replacement && Replacement->CoreCategory == ECoreCategory::Stat && !OriginalIds.Contains(Replacement->CoreId));
	if (Choices.IsValidIndex(0)) Choices[0] = Replacement;
	TestNull(TEXT("A card cannot reroll twice"), Fixture.Reward->RerollCoreChoice(0));
	TestFalse(TEXT("Exhausted stat pool does not fall back to abilities"), Fixture.Reward->CanRerollCoreChoice(1));

	TestTrue(TEXT("An open reward cannot be replaced by another reward"), Fixture.GenerateChoices(Pool, ECoreCategory::Ability).IsEmpty());
	Fixture.Reward->Cancel();
	TestTrue(TEXT("Closing a reward discards its card choices"), Fixture.Reward->SelectCore(Choices[0]->CoreId) == ECoreAcquireResult::NotOffered);

	Choices = Fixture.GenerateChoices(Pool, ECoreCategory::Ability);
	TestEqual(TEXT("Ability pool produces three cards"), Choices.Num(), 3);
	for (const UCoreDataAsset* Data : Choices)
	{
		TestTrue(TEXT("Ability choices do not include stats or fixed rewards"), Data->CoreCategory == ECoreCategory::Ability && Data != FixedOnly);
	}
	TestFalse(TEXT("Exhausted ability pool does not fall back to stats"), Fixture.Reward->CanRerollCoreChoice(0));
	Fixture.Reward->Cancel();
	TestTrue(TEXT("A pool containing only fixed rewards produces no choices"),
		Fixture.GenerateChoices({ FixedOnly }, ECoreCategory::Ability).IsEmpty());
	TestTrue(TEXT("Random pool exclusions do not prevent fixed acquisition"), Core->AcquireCore(FixedOnly) == ECoreAcquireResult::Acquired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreSelectResultTest, "TheLastMessage.Core.SelectResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreSelectResultTest::RunTest(const FString& Parameters)
{
	FCoreTestFixture Fixture(false);
	UCoreComponent* Core = Fixture.Core;

	TArray<UCoreDataAsset*> Pool;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
		Data->CoreId = FName(*FString::Printf(TEXT("Test_Select_%d"), Index));
		Pool.Add(Data);
	}
	TestTrue(TEXT("Selecting without choices is rejected"), Fixture.Reward->SelectCore(Pool[0]->CoreId) == ECoreAcquireResult::NotOffered);
	TestTrue(TEXT("Selecting an empty ID is invalid"), Fixture.Reward->SelectCore(NAME_None) == ECoreAcquireResult::InvalidData);

	const TArray<UCoreDataAsset*> Choices = Fixture.GenerateChoices(Pool, ECoreCategory::Stat);
	TestEqual(TEXT("Three choices are offered"), Choices.Num(), 3);
	FName NotOfferedId;
	for (const UCoreDataAsset* Data : Pool)
	{
		if (!Choices.Contains(Data)) NotOfferedId = Data->CoreId;
	}
	TestTrue(TEXT("A core outside the shown cards is rejected"), Fixture.Reward->SelectCore(NotOfferedId) == ECoreAcquireResult::NotOffered);
	if (Choices.Num() > 0)
	{
		TestTrue(TEXT("Selecting a shown card acquires it"), Fixture.Reward->SelectCore(Choices[0]->CoreId) == ECoreAcquireResult::Acquired);
		TestTrue(TEXT("Successful selection closes the choices"), Fixture.Reward->SelectCore(Choices[0]->CoreId) == ECoreAcquireResult::NotOffered);
	}

	TestTrue(TEXT("Null data is invalid"), Core->AcquireCore(nullptr) == ECoreAcquireResult::InvalidData);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreRewardRecoveryTest, "TheLastMessage.Core.RewardRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreRewardRecoveryTest::RunTest(const FString& Parameters)
{
	FCoreTestFixture Fixture(false);
	UCoreComponent* Core = Fixture.Core;
	UCoreRewardSession* Reward = Fixture.Reward.Get();
	TestTrue(TEXT("A missing recipient is unavailable"), Reward->Start(nullptr, {}, ECoreCategory::Stat) == ECoreRewardStartResult::Unavailable);
	TestTrue(TEXT("Empty category is distinguished from maxed cores"), Reward->Start(Core, {}, ECoreCategory::Stat) == ECoreRewardStartResult::NoCandidates);
	TestFalse(TEXT("An empty reward never becomes active"), Reward->IsActive());
	TArray<UCoreDataAsset*> Pool;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
		Data->CoreId = FName(*FString::Printf(TEXT("Recovery_%d"), Index));
		Pool.Add(Data);
	}
	TestTrue(TEXT("A normal reward starts after an empty attempt"), Reward->Start(Core, Pool, ECoreCategory::Stat) == ECoreRewardStartResult::Started);
	const TArray<UCoreDataAsset*> Original = Reward->GetChoices();
	TStrongObjectPtr<UCoreRewardSession> Other(NewObject<UCoreRewardSession>());
	TestTrue(TEXT("Another reward cannot reserve the same player"), Other->Start(Core, Pool, ECoreCategory::Stat) == ECoreRewardStartResult::Busy);
	Other->Cancel();
	TestTrue(TEXT("Cancelling an unrelated reward cannot release the active reservation"), Other->Start(Core, Pool, ECoreCategory::Stat) == ECoreRewardStartResult::Busy);
	TestNull(TEXT("Invalid card indices cannot reroll"), Reward->RerollCoreChoice(-1));
	TestTrue(TEXT("A failed reroll does not spend the valid card's use"), Reward->CanRerollCoreChoice(0));
	for (UCoreDataAsset* Data : Original) Core->AcquireCore(Data);
	TestTrue(TEXT("A stale maxed selection reports its actual failure"), Reward->SelectCore(Original[0]->CoreId) == ECoreAcquireResult::MaxLevel);
	const TArray<UCoreDataAsset*> Repaired = Reward->GetChoices();
	TestEqual(TEXT("Card slot indices remain stable during recovery"), Repaired.Num(), Original.Num());
	UCoreDataAsset* Remaining = nullptr;
	int32 ValidCount = 0;
	for (UCoreDataAsset* Data : Repaired)
	{
		if (!Data) continue;
		++ValidCount;
		Remaining = Data;
		TestFalse(TEXT("Recovery does not show a previous card again"), Original.Contains(Data));
	}
	TestEqual(TEXT("Only the unshown eligible core remains"), ValidCount, 1);
	int32 CancellationCount = 0;
	Reward->OnCancelled.AddLambda([&CancellationCount]() { ++CancellationCount; });
	if (Remaining)
	{
		Core->AcquireCore(Remaining);
		TestTrue(TEXT("Exhausting the last card preserves the failure result"), Reward->SelectCore(Remaining->CoreId) == ECoreAcquireResult::MaxLevel);
	}
	TestFalse(TEXT("Exhaustion ends the reward"), Reward->IsActive());
	TestEqual(TEXT("Exhaustion notifies its caller exactly once"), CancellationCount, 1);
	Reward->Cancel();
	TestEqual(TEXT("Repeated cleanup is idempotent"), CancellationCount, 1);
	TestTrue(TEXT("Fully upgraded pool has its own start result"), Other->Start(Core, Pool, ECoreCategory::Stat) == ECoreRewardStartResult::AllAtMaxLevel);
	Core->ResetOwnedCores();
	TestTrue(TEXT("Reset permits a new reward"), Other->Start(Core, Pool, ECoreCategory::Stat) == ECoreRewardStartResult::Started);
	Core->ResetOwnedCores();
	TestFalse(TEXT("Ownership reset also invalidates the active reward"), Other->IsActive());
	Reward->OnCancelled.Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreRewardGrantFailureTest, "TheLastMessage.Core.RewardGrantFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreRewardGrantFailureTest::RunTest(const FString& Parameters)
{
	UGameplayEffect* Effect = GetMutableDefault<UGameplayEffect>();
	TGuardValue<EGameplayEffectDurationType> DurationGuard(Effect->DurationPolicy, EGameplayEffectDurationType::Infinite);
	FCoreTestFixture Fixture(false);
	UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
	Data->CoreId = TEXT("MissingGrant");
	Data->MaxLevel = 3;
	Data->GrantedEffects.Add(UGameplayEffect::StaticClass());
	TestTrue(TEXT("Prepare an upgradeable core"), IsCoreAcquireSuccess(Fixture.Core->AcquireCore(Data)));
	TestEqual(TEXT("A single candidate still opens a reward"), Fixture.GenerateChoices({Data}, ECoreCategory::Stat).Num(), 1);
	for (const FActiveGameplayEffectHandle& Handle : Fixture.ASC->GetActiveEffects(FGameplayEffectQuery())) Fixture.ASC->RemoveActiveGameplayEffect(Handle);
	TestTrue(TEXT("Missing grants report a grant failure"), Fixture.Reward->SelectCore(Data->CoreId) == ECoreAcquireResult::GrantFailed);
	TestFalse(TEXT("A permanently failing last card cannot trap the screen"), Fixture.Reward->IsActive());
	TestEqual(TEXT("The failed reward does not upgrade ownership"), Fixture.Core->GetCoreLevel(Data->CoreId), 1);
	TestTrue(TEXT("A broken grant is excluded before reopening its screen"), Fixture.Reward->Start(Fixture.Core, {Data}, ECoreCategory::Stat) == ECoreRewardStartResult::Unavailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreResetNotificationsTest, "TheLastMessage.Core.ResetNotifications",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreResetNotificationsTest::RunTest(const FString& Parameters)
{
	FCoreTestFixture Fixture(false);
	UCoreComponent* Core = Fixture.Core;
	UCoreDataAsset* First = NewObject<UCoreDataAsset>();
	First->CoreId = TEXT("Test_Reset_First");
	UCoreDataAsset* Second = NewObject<UCoreDataAsset>();
	Second->CoreId = TEXT("Test_Reset_Second");
	TestTrue(TEXT("First core is acquired"), Core->AcquireCore(First) == ECoreAcquireResult::Acquired);
	TestTrue(TEXT("Second core is acquired"), Core->AcquireCore(Second) == ECoreAcquireResult::Acquired);

	// 첫 제거 알림에서 두 번째 코어를 다시 획득하려는 상황을 재현한다.
	TStrongObjectPtr<UCoreComponentTestObserver> Observer(NewObject<UCoreComponentTestObserver>());
	TArray<FName> RemovedIds;
	ECoreAcquireResult ReentrantResult = ECoreAcquireResult::InvalidData;
	Observer->Callback = [&](FName CoreId, int32 OldLevel, int32 NewLevel)
	{
		if (NewLevel != 0) return;
		RemovedIds.Add(CoreId);
		TestEqual(TEXT("Removal reports the previous level"), OldLevel, 1);
		TestEqual(TEXT("Removal notification agrees with actual ownership"), Core->GetCoreLevel(CoreId), 0);
		TestEqual(TEXT("All ownership is cleared before the first notification"), Core->GetCoreLevel(Second->CoreId), 0);
		if (CoreId == First->CoreId)
		{
			ReentrantResult = Core->AcquireCore(Second);
		}
	};
	Core->OnCoreChanged.AddDynamic(Observer.Get(), &UCoreComponentTestObserver::HandleCoreChanged);
	Core->ResetOwnedCores();
	TestTrue(TEXT("Acquisition during reset notification is rejected as busy"), ReentrantResult == ECoreAcquireResult::Busy);
	TestEqual(TEXT("Each removed core emits one notification"), RemovedIds.Num(), 2);
	TestTrue(TEXT("Both core removals are reported"), RemovedIds.Contains(First->CoreId) && RemovedIds.Contains(Second->CoreId));
	TestFalse(TEXT("Reset leaves the first core unowned"), Core->HasCore(First->CoreId));
	TestFalse(TEXT("Reset leaves the second core unowned"), Core->HasCore(Second->CoreId));
	TestTrue(TEXT("Acquisition is allowed again after reset returns"), Core->AcquireCore(Second) == ECoreAcquireResult::Acquired);
	Core->OnCoreChanged.RemoveAll(Observer.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreAcquireNotificationsTest, "TheLastMessage.Core.AcquireNotifications",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreAcquireNotificationsTest::RunTest(const FString& Parameters)
{
	FCoreTestFixture Fixture(false);
	UCoreComponent* Core = Fixture.Core;
	UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
	Data->CoreId = TEXT("Test_Acquire_Notifications");
	Data->MaxLevel = 2;
	UCoreDataAsset* Other = NewObject<UCoreDataAsset>();
	Other->CoreId = TEXT("Test_Acquire_Other");

	TStrongObjectPtr<UCoreComponentTestObserver> MutatingObserver(NewObject<UCoreComponentTestObserver>());
	TStrongObjectPtr<UCoreComponentTestObserver> ReadingObserver(NewObject<UCoreComponentTestObserver>());
	int32 Attempts = 0;
	int32 Notifications = 0;
	// 앞선 수신자가 상태 변경을 요청해도, 뒤의 수신자는 알림과 일치하는 보유 상태를 읽어야 한다.
	MutatingObserver->Callback = [&](FName CoreId, int32 OldLevel, int32 NewLevel)
	{
		if (CoreId != Data->CoreId || NewLevel == 0) return;
		++Attempts;
		TestTrue(TEXT("Another acquisition is busy during notification"), Core->AcquireCore(Other) == ECoreAcquireResult::Busy);
		TestTrue(TEXT("Another upgrade is busy during notification"), Core->AcquireCore(Data) == ECoreAcquireResult::Busy);
		Core->ResetOwnedCores();
		TestEqual(TEXT("Reset during acquisition notification cannot clear ownership"), Core->GetCoreLevel(CoreId), NewLevel);
	};
	ReadingObserver->Callback = [&](FName CoreId, int32 OldLevel, int32 NewLevel)
	{
		++Notifications;
		TestEqual(TEXT("Only the requested core emits a notification"), CoreId, Data->CoreId);
		TestEqual(TEXT("Notification level agrees with actual ownership"), Core->GetCoreLevel(CoreId), NewLevel);
		TestEqual(TEXT("Acquisition or upgrade advances one level"), NewLevel, OldLevel + 1);
	};
	Core->OnCoreChanged.AddDynamic(MutatingObserver.Get(), &UCoreComponentTestObserver::HandleCoreChanged);
	Core->OnCoreChanged.AddDynamic(ReadingObserver.Get(), &UCoreComponentTestObserver::HandleCoreChanged);
	TestTrue(TEXT("First acquisition succeeds"), Core->AcquireCore(Data) == ECoreAcquireResult::Acquired);
	TestEqual(TEXT("First acquisition leaves the core at level one"), Core->GetCoreLevel(Data->CoreId), 1);
	TestTrue(TEXT("Upgrade after the previous notification succeeds"), Core->AcquireCore(Data) == ECoreAcquireResult::Upgraded);
	TestEqual(TEXT("Upgrade leaves the core at level two"), Core->GetCoreLevel(Data->CoreId), 2);
	TestEqual(TEXT("Both acquisition and upgrade were checked"), Attempts, 2);
	TestEqual(TEXT("Each successful request emits one notification"), Notifications, 2);
	TestFalse(TEXT("Rejected acquisition does not grant the other core"), Core->HasCore(Other->CoreId));
	Core->OnCoreChanged.RemoveAll(MutatingObserver.Get());
	Core->OnCoreChanged.RemoveAll(ReadingObserver.Get());
	Core->ResetOwnedCores();
	TestFalse(TEXT("Reset is allowed after acquisition notifications finish"), Core->HasCore(Data->CoreId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreGrantValidationTest, "TheLastMessage.Core.GrantValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreGrantValidationTest::RunTest(const FString& Parameters)
{
	UGameplayEffect* Effect = GetMutableDefault<UGameplayEffect>();
	TGuardValue<EGameplayEffectDurationType> DurationGuard(Effect->DurationPolicy, EGameplayEffectDurationType::Infinite);
	FCoreTestFixture Fixture(false);
	UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
	Data->CoreId = TEXT("Test_DuplicateGrants");
	Data->MaxLevel = 2;
	Data->GrantedEffects = { UGameplayEffect::StaticClass(), UGameplayEffect::StaticClass() };
	FText Reason;
	TestFalse(TEXT("Duplicate effects fail shared grant validation"), Data->HasValidGrantSetup(&Reason));
	TestFalse(TEXT("Duplicate effect validation supplies a reason"), Reason.IsEmpty());
	TestTrue(TEXT("Duplicate effects are rejected before acquisition"), Fixture.Core->AcquireCore(Data) == ECoreAcquireResult::InvalidData);
	TestTrue(TEXT("Rejected effect data leaves no active effects"), Fixture.ASC->GetActiveEffects(FGameplayEffectQuery()).IsEmpty());
	TestFalse(TEXT("Rejected effect data is not recorded as owned"), Fixture.Core->HasCore(Data->CoreId));

	Data->GrantedEffects.SetNum(1);
	Data->GrantedAbilities = { UGameplayAbility::StaticClass(), UGameplayAbility::StaticClass() };
	Reason = FText::GetEmpty();
	TestFalse(TEXT("Duplicate abilities fail shared grant validation"), Data->HasValidGrantSetup(&Reason));
	TestFalse(TEXT("Duplicate ability validation supplies a reason"), Reason.IsEmpty());
	TestTrue(TEXT("Duplicate abilities are rejected before acquisition"), Fixture.Core->AcquireCore(Data) == ECoreAcquireResult::InvalidData);
	TestTrue(TEXT("Rejected ability data leaves no partial effects"), Fixture.ASC->GetActiveEffects(FGameplayEffectQuery()).IsEmpty());
	TestTrue(TEXT("Rejected ability data leaves no granted abilities"), Fixture.ASC->GetActivatableAbilities().IsEmpty());

	Data->GrantedAbilities.SetNum(1);
	TestTrue(TEXT("Distinct GE and GA classes are valid"), Data->HasValidGrantSetup());
	TestTrue(TEXT("Corrected data can be acquired"), Fixture.Core->AcquireCore(Data) == ECoreAcquireResult::Acquired);
	Data->GrantedEffects.Add(UGameplayEffect::StaticClass());
	TestTrue(TEXT("Duplicate data also rejects upgrades"), Fixture.Core->AcquireCore(Data) == ECoreAcquireResult::InvalidData);
	TestEqual(TEXT("Rejected upgrade preserves the owned level"), Fixture.Core->GetCoreLevel(Data->CoreId), 1);
	TestEqual(TEXT("Rejected upgrade preserves one existing effect"), Fixture.ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 1);
	Data->GrantedEffects.SetNum(1);

	UCoreDataAsset* Other = NewObject<UCoreDataAsset>();
	Other->CoreId = TEXT("Test_SharedEffectClass");
	Other->GrantedEffects.Add(UGameplayEffect::StaticClass());
	TestTrue(TEXT("Different cores may use the same effect class"), Fixture.Core->AcquireCore(Other) == ECoreAcquireResult::Acquired);
	TestEqual(TEXT("Different core records keep separate effects"), Fixture.ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 2);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreDataValidationTest, "TheLastMessage.Core.DataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreDataValidationTest::RunTest(const FString& Parameters)
{
	UGameplayEffect* Effect = GetMutableDefault<UGameplayEffect>();
	TGuardValue<EGameplayEffectDurationType> DurationGuard(Effect->DurationPolicy, EGameplayEffectDurationType::Infinite);
	TGuardValue<TArray<FGameplayModifierInfo>> ModifiersGuard(Effect->Modifiers, {});
	UCurveTable* Curves = NewObject<UCurveTable>();
	const FName CurveRow(TEXT("DamageMultiplier"));
	FRichCurve& Curve = Curves->AddRichCurve(CurveRow);
	Curve.AddKey(1.0f, 1.10f);
	Curve.AddKey(2.0f, 1.20f);
	Curve.AddKey(3.0f, 1.30f);
	FScalableFloat Magnitude;
	Magnitude.SetScalingValue(1.0f, CurveRow, Curves);
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UCombatAttributeSet::GetDamageDealtMultiplierAttribute();
	Modifier.ModifierOp = EGameplayModOp::MultiplyAdditive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Effect->Modifiers.Add(Modifier);

	UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
	Data->CoreId = TEXT("Test_Validation");
	Data->CoreName = FText::FromString(TEXT("Validation"));
	Data->GrantedEffects.Add(UGameplayEffect::StaticClass());
	Data->CoreDescription = FText::FromString(TEXT("피해 {Damage}% 증가"));
	FCoreDescriptionValueBinding Binding;
	Binding.Token = TEXT("Damage");
	Binding.Attribute = UCombatAttributeSet::GetDamageDealtMultiplierAttribute();
	Binding.Format = ECoreDescriptionValueFormat::IncreasePercent;
	Data->DescriptionValues.Add(Binding);

	auto Validate = [Data](int32& OutErrors, int32& OutWarnings)
	{
		FDataValidationContext Context;
		const EDataValidationResult Result = Data->IsDataValid(Context);
		OutErrors = static_cast<int32>(Context.GetNumErrors());
		OutWarnings = static_cast<int32>(Context.GetNumWarnings());
		return Result;
	};
	int32 Errors = 0;
	int32 Warnings = 0;

	Data->MaxLevel = 3;
	TestTrue(TEXT("Curve keys matching MaxLevel are valid"), Validate(Errors, Warnings) == EDataValidationResult::Valid);
	TestEqual(TEXT("Matching setup has no errors"), Errors, 0);
	TestEqual(TEXT("Matching setup has no warnings"), Warnings, 0);
	float Value = 0.0f;
	constexpr float CurveTolerance = UE_KINDA_SMALL_NUMBER;
	TestTrue(TEXT("Auto-searched binding reads level two"), Data->TryGetDescriptionValue(Binding, 2, Value) && FMath::IsNearlyEqual(Value, 1.20f));

	Data->MaxLevel = 4;
	TestTrue(TEXT("Constant tail is valid with a warning instead of a missing-key error"),
		Validate(Errors, Warnings) == EDataValidationResult::Valid && Errors == 0 && Warnings > 0);
	TestTrue(TEXT("Constant tail preserves the final key value"),
		Data->TryGetDescriptionValue(Binding, 4, Value) && FMath::IsNearlyEqual(Value, 1.30f, CurveTolerance));

	Curve.PostInfinityExtrap = RCCE_Linear;
	TestTrue(TEXT("Linear tail is valid beyond the last key without warnings"),
		Validate(Errors, Warnings) == EDataValidationResult::Valid && Errors == 0 && Warnings == 0);
	TestTrue(TEXT("Description reads the extrapolated value"),
		Data->TryGetDescriptionValue(Binding, 4, Value) && FMath::IsNearlyEqual(Value, 1.40f, CurveTolerance));
	{
		FCoreTestFixture Fixture(true);
		for (int32 Level = 1; Level <= Data->MaxLevel; ++Level)
		{
			TestTrue(TEXT("A core can upgrade beyond the final curve key"), IsCoreAcquireSuccess(Fixture.Core->AcquireCore(Data)));
		}
		TestTrue(TEXT("The active GE uses the extrapolated level-four magnitude"),
			FMath::IsNearlyEqual(Fixture.Attributes->GetDamageDealtMultiplier(), 1.40f, CurveTolerance));
		AddInfo(FString::Printf(TEXT("Linear Lv.4: description=%.9f, active GE=%.9f, expected=1.400000000"),
			Value, Fixture.Attributes->GetDamageDealtMultiplier()));
		TestTrue(TEXT("Extrapolation does not bypass the maximum upgrade level"),
			Fixture.Core->AcquireCore(Data) == ECoreAcquireResult::MaxLevel);
	}

	Data->MaxLevel = 10;
	TestTrue(TEXT("Raising MaxLevel needs no additional keys for a linear tail"),
		Validate(Errors, Warnings) == EDataValidationResult::Valid && Warnings == 0);
	TestTrue(TEXT("The linear tail reaches the configured maximum level"),
		Data->TryGetDescriptionValue(Binding, 10, Value) && FMath::IsNearlyEqual(Value, 2.0f, CurveTolerance));
	AddInfo(FString::Printf(TEXT("Linear Lv.10: description=%.9f, expected=2.000000000"), Value));

	const FRichCurve CompleteCurve = Curve;
	Curve.Reset();
	Curve.AddKey(1.0f, 1.10f);
	TestTrue(TEXT("A single-key linear tail warns because it has no slope"),
		Validate(Errors, Warnings) == EDataValidationResult::Valid && Warnings > 0);
	TestTrue(TEXT("A single-key linear tail keeps its sole value"),
		Data->TryGetDescriptionValue(Binding, 10, Value) && FMath::IsNearlyEqual(Value, 1.10f, CurveTolerance));
	Curve.Reset();
	TestTrue(TEXT("An empty curve still fails validation"), Validate(Errors, Warnings) == EDataValidationResult::Invalid);
	Curve = CompleteCurve;
	Curve.PostInfinityExtrap = RCCE_Cycle;
	TestTrue(TEXT("A repeating tail remains valid but requests a review"),
		Validate(Errors, Warnings) == EDataValidationResult::Valid && Warnings > 0);
	Curve.PostInfinityExtrap = RCCE_Linear;

	Data->MaxLevel = 2;
	TestTrue(TEXT("Curve longer than MaxLevel only warns"), Validate(Errors, Warnings) == EDataValidationResult::Valid && Warnings > 0);

	Data->MaxLevel = 3;
	Data->CoreDescription = FText::FromString(TEXT("피해 {Damage}% 증가, 사거리 {Range}"));
	TestTrue(TEXT("Unbound description token is invalid"), Validate(Errors, Warnings) == EDataValidationResult::Invalid);

	Data->CoreDescription = FText::FromString(TEXT("피해 {Damage}% 증가"));
	Data->DescriptionValues.Add(Binding);
	TestTrue(TEXT("Duplicate description tokens are invalid"), Validate(Errors, Warnings) == EDataValidationResult::Invalid);
	TestEqual(TEXT("Duplicate token is reported as one error"), Errors, 1);
	Data->DescriptionValues.SetNum(1);
	TestTrue(TEXT("Removing duplicate token restores valid data"), Validate(Errors, Warnings) == EDataValidationResult::Valid);

	Data->GrantedEffects.Add(UGameplayEffect::StaticClass());
	TestTrue(TEXT("Editor validation rejects duplicate effects"), Validate(Errors, Warnings) == EDataValidationResult::Invalid);
	Data->GrantedEffects.SetNum(1);
	Data->GrantedAbilities = { UGameplayAbility::StaticClass(), UGameplayAbility::StaticClass() };
	TestTrue(TEXT("Editor validation rejects duplicate abilities"), Validate(Errors, Warnings) == EDataValidationResult::Invalid);
	Data->GrantedAbilities.Reset();
	TestTrue(TEXT("Removing duplicate grants restores valid data"), Validate(Errors, Warnings) == EDataValidationResult::Valid);

	Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
	TestTrue(TEXT("Non-infinite effect is invalid"), Validate(Errors, Warnings) == EDataValidationResult::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreValidationBudgetTest, "TheLastMessage.Core.ValidationBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreValidationBudgetTest::RunTest(const FString& Parameters)
{
	UGameplayEffect* Effect = GetMutableDefault<UGameplayEffect>();
	TGuardValue<EGameplayEffectDurationType> DurationGuard(Effect->DurationPolicy, EGameplayEffectDurationType::Infinite);
	TGuardValue<TArray<FGameplayModifierInfo>> ModifiersGuard(Effect->Modifiers, {});
	FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UCombatAttributeSet::GetDamageDealtMultiplierAttribute();
	Modifier.ModifierOp = EGameplayModOp::MultiplyAdditive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.25f));
	UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
	Data->CoreId = TEXT("Test_ValidationBudget");
	Data->CoreName = FText::FromString(TEXT("Validation budget"));
	Data->CoreDescription = FText::FromString(TEXT("피해 {Damage}% 증가"));
	Data->GrantedEffects.Add(UGameplayEffect::StaticClass());
	FCoreDescriptionValueBinding Binding;
	Binding.Token = TEXT("Damage");
	Binding.Attribute = Modifier.Attribute;
	Binding.Format = ECoreDescriptionValueFormat::IncreasePercent;
	Data->DescriptionValues.Add(Binding);
	auto Check = [this, Data](const TCHAR* Case, int32 ExpectedErrors, int32 ExpectedWarnings)
	{
		FDataValidationContext Context;
		const EDataValidationResult Result = Data->IsDataValid(Context);
		TestTrue(Case, Result == (ExpectedErrors ? EDataValidationResult::Invalid : EDataValidationResult::Valid));
		TestEqual(FString::Printf(TEXT("%s error count"), Case), static_cast<int32>(Context.GetNumErrors()), ExpectedErrors);
		TestEqual(FString::Printf(TEXT("%s warning count"), Case), static_cast<int32>(Context.GetNumWarnings()), ExpectedWarnings);
	};
	const double StartedAt = FPlatformTime::Seconds();
	Data->MaxLevel = MAX_int32;
	Check(TEXT("A fixed magnitude needs no partial-validation warning even at MAX_int32"), 0, 0);

	UCurveTable* Table = NewObject<UCurveTable>();
	FRichCurve& Curve = Table->AddRichCurve(TEXT("Scaling"));
	Curve.AddKey(1.f, 1.f);
	Curve.AddKey(2.f, 2.f);
	Curve.PostInfinityExtrap = RCCE_Linear;
	FScalableFloat Scaling;
	Scaling.SetScalingValue(1.f, TEXT("Scaling"), Table);
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Scaling);
	Data->MaxLevel = 1024;
	Check(TEXT("Levels within the validation budget are checked without a partial warning"), 0, 0);
	Data->MaxLevel = 1025;
	Check(TEXT("Exceeding the validation budget warns once"), 0, 1);
	Data->MaxLevel = MAX_int32;
	Check(TEXT("A huge maximum finishes without an overflowing loop"), 0, 1);
	TestEqual(TEXT("Validation preserves the configured upgrade limit"), Data->MaxLevel, MAX_int32);
	float Value = 0.f;
	TestTrue(TEXT("The actual high-level curve value remains readable"),
		Data->TryGetDescriptionValue(Binding, Data->MaxLevel, Value) && Value > 1.e9f);

	// 초반 검사 범위는 유한하지만 최대 단계에서 넘치는 수치도 오류로 잡아야 한다.
	Scaling.SetScalingValue(MAX_flt / 2048.f, TEXT("Scaling"), Table);
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Scaling);
	Check(TEXT("A non-finite magnitude at the final level is still rejected"), 1, 1);
	Scaling.SetScalingValue(1.f, TEXT("Scaling"), Table);
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Scaling);
	Curve.UpdateOrAddKey(1024.f, .5f);
	Data->MaxLevel = 1024;
	Check(TEXT("The final level of the full-check range still validates its format"), 0, 1);
	Curve.Reset();
	Curve.DefaultValue = 1.f;
	Check(TEXT("An empty curve remains an error"), 1, 0);
	Curve.AddKey(1.f, 1.f);
	Curve.AddKey(2.f, 2.f);
	Curve.PostInfinityExtrap = RCCE_Constant;
	Data->UpgradeLimitType = ECoreUpgradeLimitType::Infinite;
	Check(TEXT("Infinite ignores MaxLevel and retains its existing checks"), 0, 0);
	AddInfo(FString::Printf(TEXT("Validation budget boundary cases completed in %.3f ms; MaxLevel=%d was preserved."),
		(FPlatformTime::Seconds() - StartedAt) * 1000., Data->MaxLevel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreIdValidationTest, "TheLastMessage.Core.UniqueIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreIdValidationTest::RunTest(const FString& Parameters)
{
	// 서로 다른 패키지의 에셋을 메모리에서만 등록하고 테스트 종료 시 등록을 해제한다.
	struct FRegisteredCore
	{
		TStrongObjectPtr<UCoreDataAsset> Data;
		explicit FRegisteredCore(FName CoreId)
		{
			const FString AssetName = TEXT("CoreId_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			const FString PackageName = TEXT("/Game/__Automation__/") + AssetName;
			UPackage* Package = CreatePackage(*PackageName);
			Package->MarkAsFullyLoaded();
			Data.Reset(NewObject<UCoreDataAsset>(Package, FName(*AssetName), RF_Public | RF_Standalone));
			Data->CoreId = CoreId;
			Data->CoreName = FText::FromString(TEXT("ID validation"));
			FAssetRegistryModule::AssetCreated(Data.Get());
		}
		~FRegisteredCore()
		{
			FAssetRegistryModule::AssetDeleted(Data.Get());
			Data->ClearFlags(RF_Public | RF_Standalone);
			Data->SetFlags(RF_Transient);
		}
	};
	const FName SharedId(*(TEXT("Test_Unique_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	const FName SeparateId(*(SharedId.ToString() + TEXT("_Separate")));
	FRegisteredCore First(SharedId);
	FRegisteredCore Second(SeparateId);
	auto Check = [this](const TCHAR* Case, const UCoreDataAsset* Data, int32 ExpectedErrors)
	{
		FDataValidationContext Context;
		const EDataValidationResult Result = Data->IsDataValid(Context);
		TestTrue(Case, Result == (ExpectedErrors ? EDataValidationResult::Invalid : EDataValidationResult::Valid));
		TestEqual(FString::Printf(TEXT("%s error count"), Case), static_cast<int32>(Context.GetNumErrors()), ExpectedErrors);
		return Context.GetNumErrors() ? Context.GetIssues()[0].Message.ToString() : FString();
	};
	Check(TEXT("An asset does not conflict with itself or a different ID"), First.Data.Get(), 0);
	Second.Data->CoreId = SharedId;
	const FString Message = Check(TEXT("An unsaved ID change on another asset is detected"), First.Data.Get(), 1);
	TestTrue(TEXT("The error identifies the duplicated ID and the other asset"),
		Message.Contains(SharedId.ToString()) && Message.Contains(Second.Data->GetPathName()));
	Check(TEXT("Both assets report a duplicate"), Second.Data.Get(), 1);
	Second.Data->CoreId = FName(*SharedId.ToString().ToLower());
	Check(TEXT("IDs follow FName's case-insensitive identity"), First.Data.Get(), 1);
	Second.Data->CoreId = SeparateId;
	Check(TEXT("Correcting the ID restores valid data without saving"), First.Data.Get(), 0);
	{
		FRegisteredCore Third(SeparateId);
		Check(TEXT("Duplicates of an unrelated ID do not invalidate this asset"), First.Data.Get(), 0);
		Check(TEXT("The unrelated duplicate is still detected on its owner"), Second.Data.Get(), 1);
	}
	Check(TEXT("Removing an asset clears its duplicate registration"), Second.Data.Get(), 0);
	Second.Data->CoreId = NAME_None;
	Check(TEXT("An empty ID remains an error"), Second.Data.Get(), 1);
	UCoreDataAsset* Transient = NewObject<UCoreDataAsset>();
	Transient->CoreId = SharedId;
	Transient->CoreName = First.Data->CoreName;
	Check(TEXT("A temporary runtime definition is not a separate project asset"), Transient, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreDescriptionLimitsTest, "TheLastMessage.Core.DescriptionLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreDescriptionLimitsTest::RunTest(const FString& Parameters)
{
	UGameplayEffect* Effect = GetMutableDefault<UGameplayEffect>();
	TGuardValue<EGameplayEffectDurationType> DurationGuard(Effect->DurationPolicy, EGameplayEffectDurationType::Infinite);
	TGuardValue<TArray<FGameplayModifierInfo>> ModifiersGuard(Effect->Modifiers, {});
	Effect->Modifiers.AddDefaulted();
	UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
	Data->CoreId = TEXT("Test_DescriptionLimits");
	Data->CoreName = FText::FromString(TEXT("Description limits"));
	Data->CoreDescription = FText::FromString(TEXT("효과 {Value}"));
	Data->GrantedEffects.Add(UGameplayEffect::StaticClass());
	auto Configure = [Effect, Data](const FGameplayAttribute& Attribute, float Value,
		EGameplayModOp::Type Operation, ECoreDescriptionValueFormat Format)
	{
		Effect->Modifiers[0].Attribute = Attribute;
		Effect->Modifiers[0].ModifierOp = Operation;
		Effect->Modifiers[0].ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		FCoreDescriptionValueBinding Binding;
		Binding.Token = TEXT("Value");
		Binding.Attribute = Attribute;
		Binding.Format = Format;
		Data->DescriptionValues = { Binding };
	};
	auto CheckWarnings = [this, Data](const TCHAR* Case, int32 ExpectedWarnings)
	{
		FDataValidationContext Context;
		TestTrue(FString::Printf(TEXT("%s remains valid"), Case), Data->IsDataValid(Context) == EDataValidationResult::Valid);
		TestEqual(FString::Printf(TEXT("%s warning count"), Case), static_cast<int32>(Context.GetNumWarnings()), ExpectedWarnings);
	};

	Configure(UCombatAttributeSet::GetDamageTakenMultiplierAttribute(), .3f,
		EGameplayModOp::MultiplyAdditive, ECoreDescriptionValueFormat::ReductionPercent);
	UCurveTable* Table = NewObject<UCurveTable>();
	FRichCurve& Curve = Table->AddRichCurve(TEXT("Reduction"));
	Curve.AddKey(1.f, .5f);
	Curve.AddKey(2.f, .4f);
	Curve.PostInfinityExtrap = RCCE_Linear;
	FScalableFloat Scaling;
	Scaling.SetScalingValue(1.f, TEXT("Reduction"), Table);
	Effect->Modifiers[0].ModifierMagnitude = FGameplayEffectModifierMagnitude(Scaling);
	Data->MaxLevel = 3;
	CheckWarnings(TEXT("Damage multiplier at its runtime floor"), 0);
	Data->MaxLevel = 6;
	CheckWarnings(TEXT("Several extrapolated levels below the floor produce one warning"), 1);
	Data->MaxLevel = 4;
	{
		FCoreTestFixture Fixture(true);
		for (int32 Level = 1; Level <= Data->MaxLevel; ++Level)
			TestTrue(TEXT("A warning does not prevent acquisition"), IsCoreAcquireSuccess(Fixture.Core->AcquireCore(Data)));
		float DescriptionValue = 0.f;
		TestTrue(TEXT("The description still reads the configured magnitude"),
			Data->TryGetDescriptionValue(Data->DescriptionValues[0], 4, DescriptionValue)
			&& FMath::IsNearlyEqual(DescriptionValue, .2f, UE_KINDA_SMALL_NUMBER));
		TestTrue(TEXT("The actual effect retains its runtime floor"),
			FMath::IsNearlyEqual(Fixture.Attributes->GetDamageTakenMultiplier(), .3f, UE_KINDA_SMALL_NUMBER));
	}
	Configure(UCombatAttributeSet::GetChargeTimeMultiplierAttribute(), .1f,
		EGameplayModOp::MultiplyAdditive, ECoreDescriptionValueFormat::ReductionPercent);
	CheckWarnings(TEXT("Charge time below its runtime floor"), 1);
	Configure(UCombatAttributeSet::GetChargeTimeMultiplierAttribute(), .2f,
		EGameplayModOp::MultiplyAdditive, ECoreDescriptionValueFormat::ReductionPercent);
	CheckWarnings(TEXT("Charge time at its runtime floor"), 0);
	Configure(UCombatAttributeSet::GetCriticalChanceAttribute(), 1.2f,
		EGameplayModOp::Override, ECoreDescriptionValueFormat::Percent);
	CheckWarnings(TEXT("Critical chance override above one"), 1);
	Configure(UCombatAttributeSet::GetCriticalChanceAttribute(), 1.2f,
		EGameplayModOp::Additive, ECoreDescriptionValueFormat::Percent);
	CheckWarnings(TEXT("Critical chance contribution above one"), 1);
	Configure(UCombatAttributeSet::GetCriticalChanceAttribute(), .05f,
		EGameplayModOp::Additive, ECoreDescriptionValueFormat::Percent);
	CheckWarnings(TEXT("A normal critical chance contribution"), 0);
	Configure(UCombatAttributeSet::GetCriticalChanceAttribute(), -.1f,
		EGameplayModOp::Additive, ECoreDescriptionValueFormat::Percent);
	CheckWarnings(TEXT("A negative contribution requires the player's existing stats"), 0);
	Configure(UCombatAttributeSet::GetCriticalChanceAttribute(), 2.f,
		EGameplayModOp::MultiplyAdditive, ECoreDescriptionValueFormat::IncreasePercent);
	CheckWarnings(TEXT("A multiplier is not itself an absolute critical chance"), 0);
	Configure(UCombatAttributeSet::GetHealthAttribute(), 50.f,
		EGameplayModOp::Override, ECoreDescriptionValueFormat::Number);
	CheckWarnings(TEXT("Current health requires the player's maximum health"), 0);
	return true;
}
#endif

#endif
