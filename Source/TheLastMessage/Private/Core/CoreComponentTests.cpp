#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "Engine/CurveTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GAS/CombatAttributeSet.h"
#include "Misc/DataValidation.h"
#include "Templates/UnrealTemplate.h"

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

		explicit FCoreTestFixture(bool bWithCombatAttributes)
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
	TestTrue(TEXT("Level-one curve is applied"), FMath::IsNearlyEqual(Attributes->GetDamageDealtMultiplier(), 1.15f));
	TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(FGameplayEffectQuery());
	TestEqual(TEXT("One GE is applied"), Handles.Num(), 1);
	const FActiveGameplayEffectHandle FirstHandle = Handles.IsEmpty() ? FActiveGameplayEffectHandle() : Handles[0];
	const FGameplayAbilitySpecHandle AbilityHandle = ASC->GetActivatableAbilities().IsEmpty()
		? FGameplayAbilitySpecHandle() : ASC->GetActivatableAbilities()[0].Handle;

	// 보유 중인 강화 후보는 유지하되, 같은 ID를 선택지에 두 번 넣지는 않는다.
	TestEqual(TEXT("Owned upgradeable core remains a unique candidate"), Core->GenerateCoreChoices(Pool, ECoreCategory::Stat).Num(), 1);
	FCoreChoiceState Saved = Core->SuspendCoreChoices();
	TestTrue(TEXT("Reacquisition upgrades"), Core->AcquireCore(Data) == ECoreAcquireResult::Upgraded);
	TestEqual(TEXT("Reacquisition is level two"), Core->GetCoreLevel(Data->CoreId), 2);
	TestTrue(TEXT("Curve replaces the old magnitude instead of stacking"), FMath::IsNearlyEqual(Attributes->GetDamageDealtMultiplier(), 1.30f));
	Handles = ASC->GetActiveEffects(FGameplayEffectQuery());
	TestTrue(TEXT("The same effect handle is retained"), Handles.Num() == 1 && Handles[0] == FirstHandle);
	TestEqual(TEXT("Upgrade does not duplicate abilities"), ASC->GetActivatableAbilities().Num(), 1);
	const FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
	TestTrue(TEXT("Existing ability level is updated"), AbilitySpec && AbilitySpec->Level == 2);
	TestEqual(TEXT("Suspended choice remains while upgradeable"), Core->ResumeCoreChoices(Saved).Num(), 1);
	Saved = Core->SuspendCoreChoices();

	TestTrue(TEXT("Final upgrade succeeds"), Core->AcquireCore(Data) == ECoreAcquireResult::Upgraded);
	TestEqual(TEXT("Final level is capped"), Core->GetCoreLevel(Data->CoreId), 3);
	TestTrue(TEXT("Final curve value is applied"), FMath::IsNearlyEqual(Attributes->GetDamageDealtMultiplier(), 1.45f));
	TestTrue(TEXT("Acquisition past the maximum is rejected"), Core->AcquireCore(Data) == ECoreAcquireResult::MaxLevel);
	TestEqual(TEXT("Rejected acquisition preserves level"), Core->GetCoreLevel(Data->CoreId), 3);
	TestTrue(TEXT("Maxed core is removed when resuming"), Core->ResumeCoreChoices(Saved).IsEmpty());
	TestTrue(TEXT("Maxed core is excluded from new choices"), Core->GenerateCoreChoices(Pool, ECoreCategory::Stat).IsEmpty());

	Core->ResetOwnedCores();
	TestEqual(TEXT("Reset clears levels"), Core->GetCoreLevel(Data->CoreId), 0);
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
	TestEqual(TEXT("Infinite remains eligible for future rewards"), Core->GenerateCoreChoices(Pool, ECoreCategory::Stat).Num(), 1);
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

	TArray<UCoreDataAsset*> Choices = Core->GenerateCoreChoices(Pool, ECoreCategory::Stat);
	TestEqual(TEXT("Stat pool produces three cards"), Choices.Num(), 3);
	TSet<FName> OriginalIds;
	for (const UCoreDataAsset* Data : Choices)
	{
		TestTrue(TEXT("Stat choices do not include abilities or maxed cores"), Data->CoreCategory == ECoreCategory::Stat && Data != Maxed);
		OriginalIds.Add(Data->CoreId);
	}
	TestEqual(TEXT("Duplicate source entries do not duplicate cards"), OriginalIds.Num(), 3);
	UCoreDataAsset* Replacement = Core->RerollCoreChoice(0);
	TestTrue(TEXT("Reroll stays in the same pool and does not repeat a shown core"),
		Replacement && Replacement->CoreCategory == ECoreCategory::Stat && !OriginalIds.Contains(Replacement->CoreId));
	if (Choices.IsValidIndex(0)) Choices[0] = Replacement;
	TestNull(TEXT("A card cannot reroll twice"), Core->RerollCoreChoice(0));
	TestFalse(TEXT("Exhausted stat pool does not fall back to abilities"), Core->CanRerollCoreChoice(1));

	FCoreChoiceState Saved = Core->SuspendCoreChoices();
	TestEqual(TEXT("Saved pool contains only four eligible stat cores"), Saved.CandidatePool.Num(), 4);
	TestEqual(TEXT("Shown history includes the replaced card"), Saved.ShownCoreIds.Num(), 4);
	TestTrue(TEXT("Per-card reroll count is saved"), Saved.RerollCounts.Num() == 3 && Saved.RerollCounts[0] == 1);
	const TArray<UCoreDataAsset*> Resumed = Core->ResumeCoreChoices(Saved);
	TestTrue(TEXT("Reopening preserves the exact card order"), Resumed == Choices);
	TestFalse(TEXT("Reopening does not refund a used reroll"), Core->CanRerollCoreChoice(0));
	Core->CancelCoreChoices();

	Choices = Core->GenerateCoreChoices(Pool, ECoreCategory::Ability);
	TestEqual(TEXT("Ability pool produces three cards"), Choices.Num(), 3);
	for (const UCoreDataAsset* Data : Choices)
	{
		TestTrue(TEXT("Ability choices do not include stats or fixed rewards"), Data->CoreCategory == ECoreCategory::Ability && Data != FixedOnly);
	}
	TestFalse(TEXT("Exhausted ability pool does not fall back to stats"), Core->CanRerollCoreChoice(0));
	Core->CancelCoreChoices();
	TestTrue(TEXT("A pool containing only fixed rewards produces no choices"),
		Core->GenerateCoreChoices({ FixedOnly }, ECoreCategory::Ability).IsEmpty());
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
	TestTrue(TEXT("Selecting without choices is rejected"), Core->SelectCore(Pool[0]->CoreId) == ECoreAcquireResult::NotOffered);
	TestTrue(TEXT("Selecting an empty ID is invalid"), Core->SelectCore(NAME_None) == ECoreAcquireResult::InvalidData);

	const TArray<UCoreDataAsset*> Choices = Core->GenerateCoreChoices(Pool, ECoreCategory::Stat);
	TestEqual(TEXT("Three choices are offered"), Choices.Num(), 3);
	FName NotOfferedId;
	for (const UCoreDataAsset* Data : Pool)
	{
		if (!Choices.Contains(Data)) NotOfferedId = Data->CoreId;
	}
	TestTrue(TEXT("A core outside the shown cards is rejected"), Core->SelectCore(NotOfferedId) == ECoreAcquireResult::NotOffered);
	if (Choices.Num() > 0)
	{
		TestTrue(TEXT("Selecting a shown card acquires it"), Core->SelectCore(Choices[0]->CoreId) == ECoreAcquireResult::Acquired);
		TestTrue(TEXT("Successful selection closes the choices"), Core->SelectCore(Choices[0]->CoreId) == ECoreAcquireResult::NotOffered);
	}

	TestTrue(TEXT("Null data is invalid"), Core->AcquireCore(nullptr) == ECoreAcquireResult::InvalidData);
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
	TestTrue(TEXT("Auto-searched binding reads level two"), Data->TryGetDescriptionValue(Binding, 2, Value) && FMath::IsNearlyEqual(Value, 1.20f));

	Data->MaxLevel = 4;
	TestTrue(TEXT("Curve shorter than MaxLevel is invalid"), Validate(Errors, Warnings) == EDataValidationResult::Invalid);

	Data->MaxLevel = 2;
	TestTrue(TEXT("Curve longer than MaxLevel only warns"), Validate(Errors, Warnings) == EDataValidationResult::Valid && Warnings > 0);

	Data->MaxLevel = 3;
	Data->CoreDescription = FText::FromString(TEXT("피해 {Damage}% 증가, 사거리 {Range}"));
	TestTrue(TEXT("Unbound description token is invalid"), Validate(Errors, Warnings) == EDataValidationResult::Invalid);

	Data->CoreDescription = FText::FromString(TEXT("피해 {Damage}% 증가"));
	Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
	TestTrue(TEXT("Non-infinite effect is invalid"), Validate(Errors, Warnings) == EDataValidationResult::Invalid);
	return true;
}
#endif

#endif
