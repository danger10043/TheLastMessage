#include "UI/OwnedCoreWidget.h"
#include "UI/FactoryUIStyle.h"

#include "AbilitySystemComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "UI/CoreDescriptionFormatter.h"
#include "UI/OwnedCoreItemWidget.h"

#define LOCTEXT_NAMESPACE "OwnedCore"

void UOwnedCoreWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	CloseButton->OnClicked.AddDynamic(this, &ThisClass::Close);
	AbilityTab->OnClicked.AddDynamic(this, &ThisClass::ShowAbilityCores);
	StatTab->OnClicked.AddDynamic(this, &ThisClass::ShowStatCores);
	DefaultIcon = DetailIcon->GetBrush();
	if (UImage* Screen = Cast<UImage>(GetWidgetFromName(TEXT("ScreenBackground"))))
	{
		Screen->SetBrush(FactoryUI::Chamfer(FactoryUI::EPlate::Screen));
		Screen->SetColorAndOpacity(FLinearColor::White);
	}
	if (UTextBlock* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("ScreenTitle"))))
	{
		Title->SetColorAndOpacity(FactoryUI::Cyan);
	}
	const FSlateBrush DetailPlate = FactoryUI::Chamfer();
	if (UWidget* Background = GetWidgetFromName(TEXT("DetailBackground")))
	{
		if (UBorder* Border = Cast<UBorder>(Background))
		{
			Border->SetBrush(DetailPlate);
			Border->SetBrushColor(FLinearColor::White);
		}
		else if (UImage* Image = Cast<UImage>(Background))
		{
			Image->SetBrush(DetailPlate);
			Image->SetColorAndOpacity(FLinearColor::White);
		}
	}
	CloseButton->SetStyle(FactoryUI::Button());
	FactoryUI::ApplyButtonSounds(CloseButton, FactoryUI::ESound::Cancel);
	CoreName->SetColorAndOpacity(FactoryUI::Text);
	CoreLevel->SetColorAndOpacity(FactoryUI::Cyan);
	CurrentEffect->SetColorAndOpacity(FactoryUI::Text);
	NextEffect->SetColorAndOpacity(FactoryUI::Amber);
}

bool UOwnedCoreWidget::SetSources(UCoreComponent* Core, UAbilitySystemComponent* ASC, FKey InToggleKey)
{
	if (!IsValid(Core) || !IsValid(ASC) || !ItemWidgetClass || ItemWidgetClass->HasAnyClassFlags(CLASS_Abstract)) return false;
	ReleaseSources();
	CoreSource = Core;
	AttributeSource = ASC;
	ToggleKey = InToggleKey;
	SelectedByCategory.Reset();
	Core->OnCoreChanged.AddDynamic(this, &ThisClass::HandleCoreChanged);
	const TArray<FGameplayAttribute> Attributes = {
		UCombatAttributeSet::GetMaxHealthAttribute(), UCombatAttributeSet::GetCriticalChanceAttribute(),
		UWeaponSlotAttributeSet::GetSharedMaxEnergyAttribute(), UCombatAttributeSet::GetMoveSpeedMultiplierAttribute(),
		UCombatAttributeSet::GetDamageDealtMultiplierAttribute(), UCombatAttributeSet::GetDamageTakenMultiplierAttribute()
	};
	for (const FGameplayAttribute& Attribute : Attributes)
	{
		AttributeSubscriptions.Add(Attribute, ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddWeakLambda(this,
			[this](const FOnAttributeChangeData&) { RefreshStats(); }));
	}
	RefreshCores(true);
	RefreshStats();
	return true;
}

void UOwnedCoreWidget::RefreshCores(bool bChooseInitialCategory)
{
	Entries = CoreSource.IsValid() ? CoreSource->GetOwnedCores() : TArray<FOwnedCoreEntry>();
	AbilityCoresBox->ClearChildren();
	StatCoresBox->ClearChildren();
	Items.Reset();
	for (const FOwnedCoreEntry& Entry : Entries)
	{
		UOwnedCoreItemWidget* Item = CreateWidget<UOwnedCoreItemWidget>(GetOwningPlayer(), ItemWidgetClass);
		if (!Item) continue;
		Item->SetEntry(Entry);
		Item->OnCoreClicked.AddUObject(this, &ThisClass::SelectCore);
		(Entry.CoreData->CoreCategory == ECoreCategory::Ability ? AbilityCoresBox : StatCoresBox)->AddChildToWrapBox(Item);
		Items.Add(Item);
	}
	// 최초 진입은 보유 코어가 있는 분류를 열고, 갱신 중에는 사용자가 보고 있던 탭을 유지한다.
	if (bChooseInitialCategory)
	{
		const bool bHasAbility = Entries.ContainsByPredicate([](const FOwnedCoreEntry& Entry) { return Entry.CoreData->CoreCategory == ECoreCategory::Ability; });
		ActiveCategory = bHasAbility || Entries.IsEmpty() ? ECoreCategory::Ability : ECoreCategory::Stat;
	}
	SetActiveCategory(ActiveCategory);
}

void UOwnedCoreWidget::ShowAbilityCores()
{
	SetActiveCategory(ECoreCategory::Ability);
}

void UOwnedCoreWidget::ShowStatCores()
{
	SetActiveCategory(ECoreCategory::Stat);
}

void UOwnedCoreWidget::SetActiveCategory(ECoreCategory Category)
{
	ActiveCategory = Category;
	const bool bAbility = Category == ECoreCategory::Ability;
	CategoryPages->SetActiveWidgetIndex(bAbility ? 0 : 1);
	AbilityTab->SetStyle(FactoryUI::Button(bAbility));
	StatTab->SetStyle(FactoryUI::Button(!bAbility));
	// SetStyle above drops sounds, so add them back. Core item buttons stay silent.
	FactoryUI::ApplyButtonSounds(AbilityTab, FactoryUI::ESound::Click);
	FactoryUI::ApplyButtonSounds(StatTab, FactoryUI::ESound::Click);
	AbilityTabIndicator->SetVisibility(bAbility ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	StatTabIndicator->SetVisibility(bAbility ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	const FName PreviousId = SelectedByCategory.FindRef(Category);
	const FOwnedCoreEntry* Selected = Entries.FindByPredicate([Category, PreviousId](const FOwnedCoreEntry& Entry)
	{
		return Entry.CoreData->CoreCategory == Category && Entry.CoreData->CoreId == PreviousId;
	});
	if (!Selected)
		Selected = Entries.FindByPredicate([Category](const FOwnedCoreEntry& Entry) { return Entry.CoreData->CoreCategory == Category; });
	SelectCore(Selected ? Selected->CoreData->CoreId : NAME_None);
}

void UOwnedCoreWidget::SelectCore(FName CoreId)
{
	const FOwnedCoreEntry* Entry = Entries.FindByPredicate([this, CoreId](const FOwnedCoreEntry& Item)
	{
		return Item.CoreData->CoreCategory == ActiveCategory && Item.CoreData->CoreId == CoreId;
	});
	const FName SelectedId = Entry ? CoreId : NAME_None;
	SelectedByCategory.Add(ActiveCategory, SelectedId);
	DetailIcon->SetVisibility(Entry ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	CoreDetails->SetVisibility(Entry ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	for (UOwnedCoreItemWidget* Item : Items) Item->SetSelected(Item->GetCoreId() == SelectedId);
	if (!Entry)
	{
		// 빈 분류에서는 도면 틀만 유지하고 이전 선택의 내용은 비운다.
		CoreName->SetText(FText::GetEmpty());
		CoreLevel->SetText(FText::GetEmpty());
		CurrentEffect->SetText(FText::GetEmpty());
		NextEffect->SetText(FText::GetEmpty());
		return;
	}
	const UCoreDataAsset& Data = *Entry->CoreData;
	CoreName->SetText(Data.CoreName);
	// 단계 문구와 최대 단계 판단은 카드와 같은 포맷터 기준을 쓴다.
	const bool bMaxed = CoreDescriptionFormatter::IsMaxLevel(Data, Entry->Level);
	CoreLevel->SetText(CoreDescriptionFormatter::MakeCurrentLevelText(Data, Entry->Level));
	// 현재 효과와 다음 강화의 효과를 각각의 GE 단계에서 읽는다.
	CurrentEffect->SetText(CoreDescriptionFormatter::MakeDescriptionAtLevel(Data, Entry->Level));
	NextEffect->SetText(bMaxed ? LOCTEXT("MaxStage", "최대 단계입니다.")
		: FText::Format(LOCTEXT("NextStage", "다음 강화 · Lv.{0}\n{1}"), FText::AsNumber(Entry->Level + 1),
			CoreDescriptionFormatter::MakeDescriptionAtLevel(Data, Entry->Level + 1)));
	if (Data.CoreIcon) DetailIcon->SetBrushFromTexture(Data.CoreIcon, false);
	else DetailIcon->SetBrush(DefaultIcon);
	// 다른 코어의 설명은 처음부터 읽을 수 있도록 스크롤 위치를 초기화한다.
	if (EffectScroll) EffectScroll->ScrollToStart();
	if (NextScroll) NextScroll->ScrollToStart();
}

void UOwnedCoreWidget::HandleCoreChanged(FName CoreId, int32 OldLevel, int32 NewLevel)
{
	RefreshCores();
}

void UOwnedCoreWidget::RefreshStats()
{
	UAbilitySystemComponent* ASC = AttributeSource.Get();
	if (!ASC) return;
	FNumberFormattingOptions Options;
	Options.UseGrouping = false;
	Options.MaximumFractionalDigits = 1;
	const auto Show = [&](UTextBlock* Text, const FGameplayAttribute& Attribute, bool bPercent, bool bMultiplier)
	{
		if (!ASC->HasAttributeSetForAttribute(Attribute)) { Text->SetText(LOCTEXT("Unavailable", "—")); return; }
		const float RawValue = ASC->GetNumericAttribute(Attribute);
		const float Display = bMultiplier ? (RawValue - 1.f) * 100.f : (bPercent ? RawValue * 100.f : RawValue);
		const FString Number = FText::AsNumber(FMath::IsNearlyZero(Display) ? 0.f : Display, &Options).ToString();
		Text->SetText(FText::FromString((bMultiplier && Display >= 0.f ? TEXT("+") : TEXT("")) + Number + (bPercent ? TEXT("%") : TEXT(""))));
	};
	// 코어 외의 효과도 포함하도록 ASC의 현재 최종 스탯을 표시한다.
	Show(MaxHealthValue, UCombatAttributeSet::GetMaxHealthAttribute(), false, false);
	Show(CriticalChanceValue, UCombatAttributeSet::GetCriticalChanceAttribute(), true, false);
	Show(MaxEnergyValue, UWeaponSlotAttributeSet::GetSharedMaxEnergyAttribute(), false, false);
	Show(MoveSpeedValue, UCombatAttributeSet::GetMoveSpeedMultiplierAttribute(), true, true);
	Show(DamageDealtValue, UCombatAttributeSet::GetDamageDealtMultiplierAttribute(), true, true);
	Show(DamageTakenValue, UCombatAttributeSet::GetDamageTakenMultiplierAttribute(), true, true);
}

void UOwnedCoreWidget::ReleaseSources()
{
	if (CoreSource.IsValid()) CoreSource->OnCoreChanged.RemoveDynamic(this, &ThisClass::HandleCoreChanged);
	if (UAbilitySystemComponent* ASC = AttributeSource.Get())
		for (const auto& Subscription : AttributeSubscriptions) ASC->GetGameplayAttributeValueChangeDelegate(Subscription.Key).Remove(Subscription.Value);
	AttributeSubscriptions.Reset();
	CoreSource.Reset();
	AttributeSource.Reset();
}

void UOwnedCoreWidget::Close()
{
	RemoveFromParent();
}

FReply UOwnedCoreWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == ToggleKey)
	{
		if (!Event.IsRepeat())
		{
			FactoryUI::PlayUISound(FactoryUI::ESound::Cancel);
			Close();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::Tab
		|| Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Left || Key == EKeys::Right) return FReply::Handled();
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UOwnedCoreWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	return FReply::Handled();
}

void UOwnedCoreWidget::NativeDestruct()
{
	ReleaseSources();
	OnClosed.Broadcast();
	Super::NativeDestruct();
}

#undef LOCTEXT_NAMESPACE
