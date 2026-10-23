#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/CoreDescriptionFormatter.h"
#include "DataAsset/CoreDataAsset.h"
#include "Engine/CurveTable.h"
#include "GAS/CombatAttributeSet.h"
#include "Templates/UnrealTemplate.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreDescriptionTest, "TheLastMessage.Core.Description",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreDescriptionTest::RunTest(const FString& Parameters)
{
	UGameplayEffect* Effect = GetMutableDefault<UGameplayEffect>();
	TGuardValue<TArray<FGameplayModifierInfo>> ModifierGuard(Effect->Modifiers, {});
	UCurveTable* Table = NewObject<UCurveTable>();
	FRichCurve& HealthCurve = Table->AddRichCurve(TEXT("Health"));
	HealthCurve.AddKey(1, 1.15f);
	HealthCurve.AddKey(2, 1.30f);
	HealthCurve.AddKey(3, 1.45f);
	HealthCurve.AddKey(4, 1.60f);
	HealthCurve.AddKey(5, 1.75f);
	FScalableFloat Scaling;
	Scaling.SetScalingValue(1.0f, TEXT("Health"), Table);
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UCombatAttributeSet::GetMaxHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Scaling);
	Effect->Modifiers.Add(Modifier);
	UCoreDataAsset* Data = NewObject<UCoreDataAsset>();
	Data->GrantedEffects.Add(UGameplayEffect::StaticClass());
	Data->MaxLevel = 5;
	Data->CoreDescription = FText::FromString(TEXT("최대 체력 증가: {HealthBonus}%"));
	FCoreDescriptionValueBinding Binding;
	Binding.Token = TEXT("HealthBonus");
	Binding.Effect = UGameplayEffect::StaticClass();
	Binding.Attribute = Modifier.Attribute;
	Binding.Format = ECoreDescriptionValueFormat::IncreasePercent;
	Data->DescriptionValues.Add(Binding);

	TestEqual(TEXT("New core shows only its first-level benefit"), CoreDescriptionFormatter::MakeDescription(*Data, 0).ToString(), FString(TEXT("최대 체력 증가: 15%")));
	TestEqual(TEXT("Owned core compares current and next levels"), CoreDescriptionFormatter::MakeDescription(*Data, 2).ToString(), FString(TEXT("최대 체력 증가: 30 → 45%")));
	TestEqual(TEXT("Owned viewer shows the current benefit without a preview"), CoreDescriptionFormatter::MakeDescriptionAtLevel(*Data, 2).ToString(), FString(TEXT("최대 체력 증가: 30%")));
	TestEqual(TEXT("Owned viewer can request the next level separately"), CoreDescriptionFormatter::MakeDescriptionAtLevel(*Data, 3).ToString(), FString(TEXT("최대 체력 증가: 45%")));
	TestTrue(TEXT("New core is labelled new"), CoreDescriptionFormatter::MakeLevelText(*Data, 0).ToString().Contains(TEXT("신규")));
	TestTrue(TEXT("Level comparison uses the actual owned level"), CoreDescriptionFormatter::MakeLevelText(*Data, 2).ToString().Contains(TEXT("Lv.2 → Lv.3")));
	TestEqual(TEXT("Maxed core never previews level six"), CoreDescriptionFormatter::MakeDescription(*Data, 5).ToString(), FString(TEXT("최대 체력 증가: 75%")));
	TestTrue(TEXT("Maxed core is labelled MAX"), CoreDescriptionFormatter::MakeLevelText(*Data, 5).ToString().Contains(TEXT("MAX")));

	HealthCurve.UpdateOrAddKey(2, 1.37f);
	TestEqual(TEXT("Changing the GE curve changes the description without another formula"), CoreDescriptionFormatter::MakeDescription(*Data, 2).ToString(), FString(TEXT("최대 체력 증가: 37 → 45%")));
	TestEqual(TEXT("Preview does not rewrite the shared DA template"), Data->CoreDescription.ToString(), FString(TEXT("최대 체력 증가: {HealthBonus}%")));

	// 같은 GE에 피해 감소와 고정 조건을 함께 두어 각각의 Modifier를 찾는지 확인한다.
	FRichCurve& ReductionCurve = Table->AddRichCurve(TEXT("Reduction"));
	ReductionCurve.AddKey(1, .9f);
	ReductionCurve.AddKey(2, .8f);
	ReductionCurve.AddKey(3, .7f);
	Scaling.SetScalingValue(1, TEXT("Reduction"), Table);
	Modifier.Attribute = UCombatAttributeSet::GetDamageTakenMultiplierAttribute();
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Scaling);
	Effect->Modifiers.Add(Modifier);
	Binding.Token = TEXT("Reduction");
	Binding.Attribute = Modifier.Attribute;
	Binding.Format = ECoreDescriptionValueFormat::ReductionPercent;
	Data->DescriptionValues = { Binding };
	Data->CoreDescription = FText::FromString(TEXT("피해 감소: {Reduction}%"));
	TestEqual(TEXT("Reduction converts a multiplier into the reduction percentage"), CoreDescriptionFormatter::MakeDescription(*Data, 2).ToString(), FString(TEXT("피해 감소: 20 → 30%")));

	Modifier.Attribute = UCombatAttributeSet::GetHighHealthThresholdRatioAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(.8f));
	Effect->Modifiers.Add(Modifier);
	Binding.Token = TEXT("Threshold");
	Binding.Attribute = Modifier.Attribute;
	Binding.Format = ECoreDescriptionValueFormat::Percent;
	Data->DescriptionValues.Add(Binding);
	Data->CoreDescription = FText::FromString(TEXT("체력 {Threshold}% 이상 / 피해 감소 {Reduction}%"));
	TestEqual(TEXT("An unchanged threshold has no misleading comparison arrow"), CoreDescriptionFormatter::MakeDescription(*Data, 2).ToString(), FString(TEXT("체력 80% 이상 / 피해 감소 20 → 30%")));

	Data->DescriptionValues[0].Attribute = FGameplayAttribute();
	TestTrue(TEXT("An invalid value is marked unavailable rather than guessed"), CoreDescriptionFormatter::MakeDescription(*Data, 2).ToString().Contains(TEXT("—")));
	Data->DescriptionValues.Reset();
	Data->CoreDescription = FText::FromString(TEXT("그래플링 훅을 해금합니다."));
	TestEqual(TEXT("A plain ability description is preserved"), CoreDescriptionFormatter::MakeDescription(*Data, 0).ToString(), Data->CoreDescription.ToString());
	return true;
}


#endif
