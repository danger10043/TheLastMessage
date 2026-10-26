#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Core/CoreComponent.h"
#include "Core/CoreRewardSession.h"
#include "DataAsset/CoreDataAsset.h"
#include "Engine/CurveTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GAS/CombatAttributeSet.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_EDITOR
#include "Character/CombatDamageLibrary.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossAttributeSet.h"
#include "Character/Boss/BossPhaseComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreDamageEventTest, "TheLastMessage.Core.DamageEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreDamageEventTest::RunTest(const FString& Parameters)
{
	FCoreTestFixture Fixture(true);
	Fixture.Attributes->SetMaxHealth(100.f);
	Fixture.Attributes->SetHealth(50.f);
	// 공용 피해 처리에서 공격자가 이미 계산한 배율을 다시 적용하지 않는지 확인한다.
	Fixture.Attributes->SetDamageDealtMultiplier(2.f);
	Fixture.Attributes->SetMeleeDamageMultiplier(3.f);
	Fixture.Attributes->SetHighHealthDamageMultiplier(4.f);
	Fixture.Attributes->SetLowHealthDamageMultiplier(5.f);
	Fixture.Attributes->SetCriticalChance(1.f);
	Fixture.Attributes->SetCriticalDamageMultiplier(2.f);
	TArray<FGameplayEventData> Events;
	const FGameplayTag EventTag = CombatGameplayTags::Event_DamageDealt;
	const FGameplayTag Melee = FGameplayTag::RequestGameplayTag(TEXT("Damage.Category.Player.Melee"));
	const FGameplayTag Ranged = FGameplayTag::RequestGameplayTag(TEXT("Damage.Category.Player.Ranged"));
	const FGameplayTag Gimmick = FGameplayTag::RequestGameplayTag(TEXT("Damage.Category.Gimmick"));
	const FDelegateHandle EventHandle = Fixture.ASC->GenericGameplayEventCallbacks.FindOrAdd(EventTag).AddLambda(
		[&Events](const FGameplayEventData* Payload)
		{
			if (Payload) Events.Add(*Payload);
		});
	ON_SCOPE_EXIT
	{
		Fixture.ASC->GenericGameplayEventCallbacks.FindChecked(EventTag).Remove(EventHandle);
	};

	UClass* Effect = LoadClass<UGameplayEffect>(nullptr,
		TEXT("/Game/Test_Inhyeok/GAS/Effect/GE_BP_CommonIncomingDamage.GE_BP_CommonIncomingDamage_C"));
	if (!TestNotNull(TEXT("Actual common incoming damage GE loads"), Effect)) return false;
	AEnemyBase* Enemy = Fixture.World->SpawnActor<AEnemyBase>();
	if (!TestNotNull(TEXT("Enemy target exists"), Enemy)) return false;
	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
	EnemyASC->InitAbilityActorInfo(Enemy, Enemy);
	if (!EnemyASC->GetSet<UEnemyAttributeSet>())
		EnemyASC->AddAttributeSetSubobject(CastChecked<UEnemyAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("AttributeSet"))));
	EnemyASC->SetNumericAttributeBase(UEnemyAttributeSet::GetMaxHealthAttribute(), 1000.f);

	for (const FGameplayTag Category : {Melee, Ranged})
	{
		Events.Reset();
		EnemyASC->SetNumericAttributeBase(UEnemyAttributeSet::GetHealthAttribute(), 1000.f);
		TestTrue(TEXT("Player attack is accepted"), UCombatDamageLibrary::ApplyFinalDamageToTarget(
			Fixture.Owner, Enemy, Effect, 125.f, Category));
		TestEqual(TEXT("One damage event per successful attack"), Events.Num(), 1);
		if (Events.Num() == 1)
		{
			const FGameplayEventData& Payload = Events[0];
			TestTrue(TEXT("Event identifies actual damage"), Payload.EventTag == EventTag);
			TestTrue(TEXT("Event preserves attacker and target"), Payload.Instigator == Fixture.Owner && Payload.Target == Enemy);
			TestEqual(TEXT("Magnitude contains actual HP loss"), Payload.EventMagnitude, 125.f);
			TestTrue(TEXT("Damage category is available to abilities"), Payload.InstigatorTags.HasTagExact(Category));
			TestTrue(TEXT("Damage context preserves source"), Payload.ContextHandle.GetSourceObject() == Fixture.Owner);
		}
		TestEqual(TEXT("Damage application is unchanged"), EnemyASC->GetNumericAttribute(UEnemyAttributeSet::GetHealthAttribute()), 875.f);
	}

	Events.Reset();
	TestTrue(TEXT("Gimmick damage still applies"), UCombatDamageLibrary::ApplyFinalDamageToTarget(
		Fixture.Owner, Enemy, Effect, 25.f, Gimmick));
	TestEqual(TEXT("Gimmick damage is excluded from player attack event"), Events.Num(), 0);
	TestFalse(TEXT("Zero damage is rejected"), UCombatDamageLibrary::ApplyFinalDamageToTarget(
		Fixture.Owner, Enemy, Effect, 0.f, Melee));
	TestFalse(TEXT("Negative damage is rejected"), UCombatDamageLibrary::ApplyFinalDamageToTarget(
		Fixture.Owner, Enemy, Effect, -10.f, Melee));
	TestEqual(TEXT("Rejected damage sends no event"), Events.Num(), 0);

	ABossMonster* Boss = Fixture.World->SpawnActorDeferred<ABossMonster>(ABossMonster::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Boss target exists"), Boss)) return false;
	Boss->AutoPossessAI = EAutoPossessAI::Disabled;
	Boss->FinishSpawning(FTransform::Identity);
	UAbilitySystemComponent* BossASC = Boss->GetAbilitySystemComponent();
	BossASC->InitAbilityActorInfo(Boss, Boss);
	if (!BossASC->GetSet<UBossAttributeSet>())
		BossASC->AddAttributeSetSubobject(CastChecked<UBossAttributeSet>(Boss->GetDefaultSubobjectByName(TEXT("AttributeSet"))));
	BossASC->SetNumericAttributeBase(UBossAttributeSet::GetMaxHealthAttribute(), 1000.f);
	BossASC->SetNumericAttributeBase(UBossAttributeSet::GetHealthAttribute(), 1000.f);
	Boss->SetRangedImmunity(true);
	TestTrue(TEXT("Immune boss still accepts valid GE"), UCombatDamageLibrary::ApplyFinalDamageToTarget(
		Fixture.Owner, Boss, Effect, 125.f, Ranged));
	TestEqual(TEXT("Immunity leaves boss HP unchanged"), BossASC->GetNumericAttribute(UBossAttributeSet::GetHealthAttribute()), 1000.f);
	TestEqual(TEXT("Immune hit sends no damage event"), Events.Num(), 0);
	Boss->SetRangedImmunity(false);
	for (const FGameplayTag Category : {Melee, Ranged})
	{
		Events.Reset();
		BossASC->SetNumericAttributeBase(UBossAttributeSet::GetHealthAttribute(), 1000.f);
		TestTrue(TEXT("Boss receives player attack"), UCombatDamageLibrary::ApplyFinalDamageToTarget(
			Fixture.Owner, Boss, Effect, 125.f, Category));
		TestEqual(TEXT("Boss damage does not reapply attacker multipliers"), BossASC->GetNumericAttribute(UBossAttributeSet::GetHealthAttribute()), 875.f);
		TestEqual(TEXT("Boss hit sends one event"), Events.Num(), 1);
		if (Events.Num() == 1)
			TestTrue(TEXT("Boss event contains actual loss and target"), Events[0].EventMagnitude == 125.f && Events[0].Target == Boss);
	}

	// 페이즈 체력 하한만 검사하므로, 퍼즐 장치가 없는 테스트 월드에서는 전환 요청을 대기 상태로 둔다.
	UBossPhaseComponent* Phase = Boss->FindComponentByClass<UBossPhaseComponent>();
	if (!TestNotNull(TEXT("Boss phase component exists"), Phase)) return false;
	FBoolProperty* PendingTransition = FindFProperty<FBoolProperty>(Phase->GetClass(), TEXT("bIsTransitionPending"));
	if (!TestNotNull(TEXT("Phase transition test state is available"), PendingTransition)) return false;
	PendingTransition->SetPropertyValue_InContainer(Phase, true);
	Events.Reset();
	BossASC->SetNumericAttributeBase(UBossAttributeSet::GetHealthAttribute(), 710.f);
	TestTrue(TEXT("Phase-limited damage is accepted"), UCombatDamageLibrary::ApplyFinalDamageToTarget(
		Fixture.Owner, Boss, Effect, 125.f, Melee));
	TestEqual(TEXT("Boss phase floor is preserved"), BossASC->GetNumericAttribute(UBossAttributeSet::GetHealthAttribute()), 700.f);
	TestEqual(TEXT("Phase-limited hit sends one event"), Events.Num(), 1);
	if (Events.Num() == 1)
		TestEqual(TEXT("Phase-limited event excludes blocked damage"), Events[0].EventMagnitude, 10.f);
	Events.Reset();
	UCombatDamageLibrary::ApplyFinalDamageToTarget(Fixture.Owner, Boss, Effect, 125.f, Melee);
	TestEqual(TEXT("Hit at phase floor sends no event"), Events.Num(), 0);

	EnemyASC->SetNumericAttributeBase(UEnemyAttributeSet::GetHealthAttribute(), 10.f);
	TestTrue(TEXT("Lethal damage is accepted"), UCombatDamageLibrary::ApplyFinalDamageToTarget(
		Fixture.Owner, Enemy, Effect, 125.f, Ranged));
	TestEqual(TEXT("Lethal hit sends one event"), Events.Num(), 1);
	if (Events.Num() == 1)
		TestEqual(TEXT("Overkill reports remaining HP only"), Events[0].EventMagnitude, 10.f);
	Events.Reset();
	UCombatDamageLibrary::ApplyFinalDamageToTarget(Fixture.Owner, Enemy, Effect, 125.f, Ranged);
	TestEqual(TEXT("Dead enemy sends no repeated damage event"), Events.Num(), 0);
	TestEqual(TEXT("Damage event alone does not heal the attacker"), Fixture.Attributes->GetHealth(), 50.f);
	return true;
}

#endif

#endif
