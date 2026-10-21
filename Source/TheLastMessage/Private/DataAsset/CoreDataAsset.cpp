#include "DataAsset/CoreDataAsset.h"

#include "Curves/RealCurve.h"
#include "Engine/CurveTable.h"
#include "GameplayEffect.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "CoreDataAsset"

namespace CoreDataAssetPrivate
{
	/** 실패 이유를 요청한 경우에만 기록하고 false를 반환한다. */
	bool Fail(FText* OutReason, const FText& Reason)
	{
		if (OutReason) *OutReason = Reason;
		return false;
	}

#if WITH_EDITOR
	/**
	 * Modifier가 Scalable Float이면 그 값을 반환한다.
	 * FGameplayEffectModifierMagnitude는 Scalable Float를 꺼내는 공개 함수가 없어 리플렉션으로 커브 정보를 읽는다.
	 */
	const FScalableFloat* GetScalableFloat(const FGameplayEffectModifierMagnitude& Magnitude)
	{
		if (Magnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::ScalableFloat) return nullptr;
		const FStructProperty* Property = FindFProperty<FStructProperty>(
			FGameplayEffectModifierMagnitude::StaticStruct(), TEXT("ScalableFloatMagnitude"));
		if (!Property || Property->Struct != FScalableFloat::StaticStruct()) return nullptr;
		return Property->ContainerPtrToValuePtr<FScalableFloat>(&Magnitude);
	}
#endif
}

bool UCoreDataAsset::HasValidGrantSetup(FText* OutReason) const
{
	using CoreDataAssetPrivate::Fail;

	// 강화는 같은 GE의 레벨만 갱신하므로, 지속 시간이 무한이고 스택이 쌓이지 않는 GE만 허용한다.
	for (const TSubclassOf<UGameplayEffect>& EffectClass : GrantedEffects)
	{
		if (!IsValid(EffectClass.Get()) || EffectClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return Fail(OutReason, LOCTEXT("InvalidEffect", "GrantedEffects에 비어 있거나 추상 클래스인 GE가 있습니다."));
		}

		const UGameplayEffect* Definition = EffectClass.GetDefaultObject();
		if (!IsValid(Definition) || Definition->DurationPolicy != EGameplayEffectDurationType::Infinite)
		{
			return Fail(OutReason, FText::Format(LOCTEXT("EffectNotInfinite", "{0}의 Duration Policy가 Infinite가 아닙니다."),
				FText::FromString(GetNameSafe(EffectClass.Get()))));
		}
		if (Definition->GetStackingType() != EGameplayEffectStackingType::None)
		{
			return Fail(OutReason, FText::Format(LOCTEXT("EffectStacking", "{0}의 Stacking Type이 None이 아닙니다."),
				FText::FromString(GetNameSafe(EffectClass.Get()))));
		}
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : GrantedAbilities)
	{
		if (!IsValid(AbilityClass.Get()) || AbilityClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return Fail(OutReason, LOCTEXT("InvalidAbility", "GrantedAbilities에 비어 있거나 추상 클래스인 GA가 있습니다."));
		}
	}

	return true;
}

bool UCoreDataAsset::FindDescriptionModifier(const FCoreDescriptionValueBinding& Binding,
	const FGameplayModifierInfo*& OutModifier, FText* OutReason) const
{
	using CoreDataAssetPrivate::Fail;
	OutModifier = nullptr;

	if (!Binding.Attribute.IsValid())
	{
		return Fail(OutReason, LOCTEXT("BindingNoAttribute", "Attribute가 지정되지 않았습니다."));
	}
	if (Binding.Effect && !GrantedEffects.Contains(Binding.Effect))
	{
		return Fail(OutReason, FText::Format(LOCTEXT("BindingEffectNotGranted", "지정한 GE {0}가 GrantedEffects에 없습니다."),
			FText::FromString(GetNameSafe(Binding.Effect.Get()))));
	}

	// Effect를 지정했으면 그 GE에서만, 비웠으면 지급하는 모든 GE에서 같은 Attribute의 Modifier를 찾는다.
	TSet<const UClass*> VisitedEffects;
	for (const TSubclassOf<UGameplayEffect>& EffectClass : GrantedEffects)
	{
		if (!EffectClass || (Binding.Effect && EffectClass != Binding.Effect) || VisitedEffects.Contains(EffectClass.Get()))
		{
			continue;
		}
		VisitedEffects.Add(EffectClass.Get());

		const UGameplayEffect* Definition = EffectClass.GetDefaultObject();
		if (!Definition) continue;

		for (const FGameplayModifierInfo& Modifier : Definition->Modifiers)
		{
			if (Modifier.Attribute != Binding.Attribute) continue;
			// 같은 Attribute의 Modifier가 여러 개면 어느 값을 표시할지 모호하므로 추측하지 않는다.
			if (OutModifier)
			{
				OutModifier = nullptr;
				return Fail(OutReason, FText::Format(LOCTEXT("BindingAmbiguous",
					"{0}를 수정하는 Modifier가 여러 개입니다. 바인딩의 Effect를 지정하세요."),
					FText::FromString(Binding.Attribute.GetName())));
			}
			OutModifier = &Modifier;
		}
	}

	if (!OutModifier)
	{
		return Fail(OutReason, FText::Format(LOCTEXT("BindingNotFound", "GrantedEffects에서 {0}를 수정하는 Modifier를 찾지 못했습니다."),
			FText::FromString(Binding.Attribute.GetName())));
	}
	return true;
}

bool UCoreDataAsset::TryGetDescriptionValue(const FCoreDescriptionValueBinding& Binding, int32 Level,
	float& OutValue, FText* OutReason) const
{
	const FGameplayModifierInfo* Modifier = nullptr;
	if (!FindDescriptionModifier(Binding, Modifier, OutReason))
	{
		return false;
	}
	if (!Modifier->ModifierMagnitude.GetStaticMagnitudeIfPossible(Level, OutValue) || !FMath::IsFinite(OutValue))
	{
		return CoreDataAssetPrivate::Fail(OutReason, FText::Format(LOCTEXT("BindingNotStatic",
			"{0}의 수치가 Scalable Float가 아니어서 설명에 표시할 수 없습니다."),
			FText::FromString(Binding.Attribute.GetName())));
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UCoreDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	bool bHasError = Super::IsDataValid(Context) == EDataValidationResult::Invalid;
	auto AddError = [&Context, &bHasError](const FText& Message)
	{
		Context.AddError(Message);
		bHasError = true;
	};
	auto AddWarning = [&Context](const FText& Message)
	{
		Context.AddWarning(Message);
	};

	// 기본 정의
	if (CoreId.IsNone())
	{
		AddError(LOCTEXT("NoCoreId", "CoreId가 비어 있습니다."));
	}
	if (CoreName.IsEmpty())
	{
		AddWarning(LOCTEXT("NoCoreName", "CoreName이 비어 있어 선택 카드에 이름이 표시되지 않습니다."));
	}

	const bool bCustomLimit = UpgradeLimitType == ECoreUpgradeLimitType::Custom;
	if (bCustomLimit && MaxLevel < 1)
	{
		AddError(LOCTEXT("InvalidMaxLevel", "Custom의 MaxLevel은 1 이상이어야 합니다."));
	}

	FText GrantReason;
	if (!HasValidGrantSetup(&GrantReason))
	{
		AddError(GrantReason);
	}

	// 커브 키와 강화 단계: Custom은 MaxLevel까지 키가 있어야 하고, Infinite는 마지막 키 이후 값이 변하지 않아야 안전하다.
	for (const TSubclassOf<UGameplayEffect>& EffectClass : GrantedEffects)
	{
		const UGameplayEffect* Definition = EffectClass ? EffectClass.GetDefaultObject() : nullptr;
		if (!Definition) continue;

		for (const FGameplayModifierInfo& Modifier : Definition->Modifiers)
		{
			const FScalableFloat* Scaling = CoreDataAssetPrivate::GetScalableFloat(Modifier.ModifierMagnitude);
			if (!Scaling || !Scaling->Curve.CurveTable)
			{
				// 커브 없이 고정된 수치(예: 과부하의 받는 피해 1.15)는 단계와 무관하다.
				continue;
			}

			const FText RowName = FText::FromName(Scaling->Curve.RowName);
			const FRealCurve* Curve = Scaling->Curve.GetCurve(TEXT("CoreDataAsset validation"), false);
			if (!Curve || Curve->GetNumKeys() == 0)
			{
				AddError(FText::Format(LOCTEXT("CurveRowMissing", "{0}의 커브 행 '{1}'을 찾을 수 없거나 키가 없습니다."),
					FText::FromString(GetNameSafe(EffectClass.Get())), RowName));
				continue;
			}

			float MinTime = 0.0f;
			float MaxTime = 0.0f;
			Curve->GetTimeRange(MinTime, MaxTime);
			if (MinTime > 1.0f + KINDA_SMALL_NUMBER)
			{
				AddWarning(FText::Format(LOCTEXT("CurveNoFirstLevel", "커브 행 '{0}'에 Lv.1 키가 없습니다."), RowName));
			}

			if (bCustomLimit && MaxLevel >= 1)
			{
				if (MaxTime + KINDA_SMALL_NUMBER < MaxLevel)
				{
					AddError(FText::Format(LOCTEXT("CurveShorterThanMaxLevel",
						"커브 행 '{0}'의 마지막 키는 Lv.{1}인데 MaxLevel은 {2}입니다. Lv.{1} 이후 단계의 키를 입력하거나 MaxLevel을 줄이세요."),
						RowName, FText::AsNumber(FMath::RoundToInt(MaxTime)), FText::AsNumber(MaxLevel)));
				}
				else if (MaxTime > MaxLevel + KINDA_SMALL_NUMBER)
				{
					AddWarning(FText::Format(LOCTEXT("CurveLongerThanMaxLevel",
						"커브 행 '{0}'에 MaxLevel({1}) 이후의 키가 있습니다. 해당 키는 사용되지 않습니다."),
						RowName, FText::AsNumber(MaxLevel)));
				}
			}
			else if (!bCustomLimit && Curve->PostInfinityExtrap != RCCE_Constant)
			{
				AddWarning(FText::Format(LOCTEXT("InfiniteCurveExtrapolation",
					"Infinite 코어인데 커브 행 '{0}'의 Post-Infinity 외삽이 Constant가 아닙니다. 마지막 키 이후 수치가 계속 변해 의도하지 않은 값이 될 수 있습니다."),
					RowName));
			}
		}
	}

	// 설명 서식의 {토큰}과 바인딩이 서로 맞는지 확인한다.
	TArray<FString> DescriptionTokens;
	FText::GetFormatPatternParameters(FTextFormat(CoreDescription), DescriptionTokens);

	TSet<FString> BoundTokens;
	const int32 LastCheckedLevel = bCustomLimit ? FMath::Max(1, MaxLevel) : 2;
	for (const FCoreDescriptionValueBinding& Binding : DescriptionValues)
	{
		if (Binding.Token.IsNone())
		{
			AddError(LOCTEXT("BindingNoToken", "Token이 비어 있는 설명 바인딩이 있습니다."));
			continue;
		}

		const FString Token = Binding.Token.ToString();
		const FText TokenText = FText::FromString(Token);
		BoundTokens.Add(Token);
		if (!DescriptionTokens.Contains(Token))
		{
			AddWarning(FText::Format(LOCTEXT("TokenUnused", "설명에 `{{0}`}이 없어 바인딩 {0}의 값이 표시되지 않습니다."), TokenText));
		}

		const FGameplayModifierInfo* Modifier = nullptr;
		FText Reason;
		if (!FindDescriptionModifier(Binding, Modifier, &Reason))
		{
			AddError(FText::Format(LOCTEXT("BindingInvalid", "설명 바인딩 {0}: {1}"), TokenText, Reason));
			continue;
		}

		// 표시할 모든 단계에서 값을 읽을 수 있는지, 표시 형식이 실제 연산과 맞는지 확인한다.
		bool bAnyBelowOne = false;
		bool bAnyAboveOne = false;
		for (int32 Level = 1; Level <= LastCheckedLevel; ++Level)
		{
			float Value = 0.0f;
			if (!TryGetDescriptionValue(Binding, Level, Value, &Reason))
			{
				AddError(FText::Format(LOCTEXT("BindingValueUnreadable", "설명 바인딩 {0}: {1}"), TokenText, Reason));
				break;
			}
			bAnyBelowOne |= Value < 1.0f - KINDA_SMALL_NUMBER;
			bAnyAboveOne |= Value > 1.0f + KINDA_SMALL_NUMBER;
		}

		const bool bMultiplyModifier = Modifier->ModifierOp == EGameplayModOp::MultiplyAdditive;
		const bool bPercentChangeFormat = Binding.Format == ECoreDescriptionValueFormat::IncreasePercent
			|| Binding.Format == ECoreDescriptionValueFormat::ReductionPercent;
		if (bPercentChangeFormat && !bMultiplyModifier)
		{
			AddWarning(FText::Format(LOCTEXT("FormatNeedsMultiply",
				"설명 바인딩 {0}: Increase/Reduction Percent는 Multiply (Additive) 배율용입니다. 추가값·덮어쓰기에는 Number나 Percent를 쓰세요."),
				TokenText));
		}
		else if (!bPercentChangeFormat && bMultiplyModifier)
		{
			AddWarning(FText::Format(LOCTEXT("MultiplyNeedsPercentChange",
				"설명 바인딩 {0}: 배율 Modifier는 Increase Percent(1.15 → 15)나 Reduction Percent(0.9 → 10)로 표시해야 합니다."),
				TokenText));
		}
		else if (Binding.Format == ECoreDescriptionValueFormat::IncreasePercent && bAnyBelowOne)
		{
			AddWarning(FText::Format(LOCTEXT("IncreaseBelowOne",
				"설명 바인딩 {0}: 1보다 작은 배율이 있어 음수로 표시됩니다. 감소 효과는 Reduction Percent를 쓰세요."), TokenText));
		}
		else if (Binding.Format == ECoreDescriptionValueFormat::ReductionPercent && bAnyAboveOne)
		{
			AddWarning(FText::Format(LOCTEXT("ReductionAboveOne",
				"설명 바인딩 {0}: 1보다 큰 배율이 있어 음수로 표시됩니다. 증가 효과는 Increase Percent를 쓰세요."), TokenText));
		}
	}

	// 바인딩이 없는 토큰은 화면에 {토큰} 글자가 그대로 노출된다.
	for (const FString& Token : DescriptionTokens)
	{
		if (!BoundTokens.Contains(Token))
		{
			AddError(FText::Format(LOCTEXT("TokenUnbound", "설명의 `{{0}`}에 연결된 설명 바인딩이 없습니다."), FText::FromString(Token)));
		}
	}

	return bHasError ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
