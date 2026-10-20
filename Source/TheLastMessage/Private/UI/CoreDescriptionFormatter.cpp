#include "UI/CoreDescriptionFormatter.h"

#include "DataAsset/CoreDataAsset.h"

#define LOCTEXT_NAMESPACE "CoreDescription"

namespace
{
	int32 GetPreviewLevel(const UCoreDataAsset& Data, int32 CurrentLevel)
	{
		const int32 Next = CurrentLevel < MAX_int32 ? FMath::Max(0, CurrentLevel) + 1 : MAX_int32;
		return Data.UpgradeLimitType == ECoreUpgradeLimitType::Custom
			? FMath::Min(Next, FMath::Max(1, Data.MaxLevel)) : Next;
	}

	bool ReadValue(const UCoreDataAsset& Data, const FCoreDescriptionValueBinding& Binding, int32 Level, float& OutValue)
	{
		if (!Binding.Effect || !Binding.Attribute.IsValid() || !Data.GrantedEffects.Contains(Binding.Effect)) return false;
		const UGameplayEffect* Effect = Binding.Effect.GetDefaultObject();
		if (!Effect) return false;
		const FGameplayModifierInfo* Match = nullptr;
		for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
		{
			if (Modifier.Attribute != Binding.Attribute) continue;
			// 동일 Attribute의 Modifier가 여러 개면 어느 값을 표시할지 모호하므로 추측하지 않는다.
			if (Match) return false;
			Match = &Modifier;
		}
		return Match && Match->ModifierMagnitude.GetStaticMagnitudeIfPossible(Level, OutValue) && FMath::IsFinite(OutValue);
	}

	FText FormatValue(float Value, ECoreDescriptionValueFormat Format)
	{
		double DisplayValue = Value;
		switch (Format)
		{
		case ECoreDescriptionValueFormat::Percent: DisplayValue = Value * 100.0; break;
		case ECoreDescriptionValueFormat::IncreasePercent: DisplayValue = (Value - 1.0) * 100.0; break;
		case ECoreDescriptionValueFormat::ReductionPercent: DisplayValue = (1.0 - Value) * 100.0; break;
		default: break;
		}
		FNumberFormattingOptions Options;
		Options.UseGrouping = false;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 1;
		return FText::AsNumber(FMath::Abs(DisplayValue) < 0.0001 ? 0.0 : DisplayValue, &Options);
	}
}

FText CoreDescriptionFormatter::MakeLevelText(const UCoreDataAsset& Data, int32 CurrentLevel)
{
	CurrentLevel = FMath::Max(0, CurrentLevel);
	const int32 Next = GetPreviewLevel(Data, CurrentLevel);
	FText Stage;
	if (CurrentLevel == 0) Stage = FText::Format(LOCTEXT("NewLevel", "신규 · Lv.{0}"), FText::AsNumber(Next));
	else if (Next > CurrentLevel) Stage = FText::Format(LOCTEXT("UpgradeLevel", "Lv.{0} → Lv.{1}"), FText::AsNumber(CurrentLevel), FText::AsNumber(Next));
	else Stage = FText::Format(LOCTEXT("CurrentLevel", "Lv.{0}"), FText::AsNumber(CurrentLevel));
	if (Data.UpgradeLimitType == ECoreUpgradeLimitType::Custom)
	{
		Stage = FText::Format(LOCTEXT("LimitedLevel", "{0} / {1}"), Stage, FText::AsNumber(FMath::Max(1, Data.MaxLevel)));
		if (CurrentLevel >= Data.MaxLevel) Stage = FText::Format(LOCTEXT("MaxLevel", "{0} · MAX"), Stage);
	}
	return Stage;
}

FText CoreDescriptionFormatter::MakeDescription(const UCoreDataAsset& Data, int32 CurrentLevel)
{
	if (Data.DescriptionValues.IsEmpty()) return Data.CoreDescription;
	CurrentLevel = FMath::Max(0, CurrentLevel);
	const int32 PreviewLevel = GetPreviewLevel(Data, CurrentLevel);
	FFormatNamedArguments Arguments;
	for (const FCoreDescriptionValueBinding& Binding : Data.DescriptionValues)
	{
		if (Binding.Token.IsNone()) continue;
		float NextValue = 0.0f;
		FText ValueText = LOCTEXT("UnavailableValue", "—");
		if (ReadValue(Data, Binding, PreviewLevel, NextValue))
		{
			ValueText = FormatValue(NextValue, Binding.Format);
			float CurrentValue = 0.0f;
			if (CurrentLevel > 0 && PreviewLevel > CurrentLevel)
			{
				if (!ReadValue(Data, Binding, CurrentLevel, CurrentValue)) ValueText = LOCTEXT("UnavailableValue", "—");
				else if (!FMath::IsNearlyEqual(CurrentValue, NextValue, 0.000001f))
					ValueText = FText::Format(LOCTEXT("ValueComparison", "{0} → {1}"), FormatValue(CurrentValue, Binding.Format), ValueText);
			}
		}
		Arguments.Add(Binding.Token.ToString(), ValueText);
	}
	return FText::Format(Data.CoreDescription, Arguments);
}

#undef LOCTEXT_NAMESPACE
