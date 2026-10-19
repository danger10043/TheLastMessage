#include "UI/PlayerStatusWidget.h"
#include "Character/Player/PlayerBase.h"
#include "AbilitySystemComponent.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

namespace
{
float Fraction(float Current, float Maximum)
{
	return Maximum > 0 && FMath::IsFinite(Current) && FMath::IsFinite(Maximum)
		? FMath::Clamp(Current / Maximum, 0.0f, 1.0f) : 0.0f;
}
FText ResourceText(float Current, float Maximum)
{
	return FText::Format(NSLOCTEXT("PlayerUI", "ResourceCount", "{0} / {1}"),
		FText::AsNumber(FMath::CeilToInt(FMath::Max(0.0f, Current))),
		FText::AsNumber(FMath::CeilToInt(FMath::Max(0.0f, Maximum))));
}
bool Ranged(const UWeaponDataAsset* Data)
{
	return Data && (Data->WeaponType == EWeaponType::LightRanged || Data->WeaponType == EWeaponType::HeavyRanged);
}
void WeaponVisual(const UWeaponDataAsset* Data, UTextBlock* Label, UImage* Icon)
{
	if (Label) Label->SetText(Data ? FText::FromName(Data->WeaponId) : NSLOCTEXT("PlayerUI", "EmptyWeapon", "--"));
	if (Icon)
	{
		Icon->SetBrushFromTexture(Data ? Data->WeaponIcon.Get() : nullptr);
		Icon->SetVisibility(Data && Data->WeaponIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
}

void UPlayerStatusWidget::SetObservedPlayer(APlayerBase* Player)
{
	if (ObservedPlayer.Get() == Player && ObservedASC.IsValid()) { RefreshStatus(); return; }
	UnbindPlayer();
	ObservedPlayer = Player;
	UAbilitySystemComponent* ASC = Player ? Player->GetAbilitySystemComponent() : nullptr;
	ObservedASC = ASC;
	if (ASC)
	{
		const FGameplayAttribute Attributes[] = {
			UCombatAttributeSet::GetHealthAttribute(), UCombatAttributeSet::GetMaxHealthAttribute(),
			UWeaponSlotAttributeSet::GetPrimaryEnergyAttribute(), UWeaponSlotAttributeSet::GetPrimaryMaxEnergyAttribute(),
			UWeaponSlotAttributeSet::GetSecondaryEnergyAttribute(), UWeaponSlotAttributeSet::GetSecondaryMaxEnergyAttribute()};
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddUObject(this, &UPlayerStatusWidget::HandleAttributeChanged);
			AttributeBindings.Emplace(Attribute, Handle);
		}
		Player->OnWeaponDataChanged.AddUniqueDynamic(this, &UPlayerStatusWidget::RefreshStatus);
	}
	RefreshStatus();
}

void UPlayerStatusWidget::UnbindPlayer()
{
	if (UAbilitySystemComponent* ASC = ObservedASC.Get())
	{
		for (const auto& Binding : AttributeBindings) ASC->GetGameplayAttributeValueChangeDelegate(Binding.Key).Remove(Binding.Value);
	}
	if (APlayerBase* Player = ObservedPlayer.Get()) Player->OnWeaponDataChanged.RemoveDynamic(this, &UPlayerStatusWidget::RefreshStatus);
	AttributeBindings.Reset();
	ObservedASC.Reset();
	ObservedPlayer.Reset();
}

void UPlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshStatus();
}

void UPlayerStatusWidget::NativeDestruct()
{
	UnbindPlayer();
	Super::NativeDestruct();
}

void UPlayerStatusWidget::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshStatus();
}

void UPlayerStatusWidget::RefreshStatus()
{
	APlayerBase* Player = ObservedPlayer.Get();
	const UCombatAttributeSet* Combat = Player ? Player->GetCombatAttributes() : nullptr;
	const UWeaponSlotAttributeSet* Energy = Player ? Player->GetWeaponSlotAttributes() : nullptr;
	const UWeaponDataAsset* Primary = Player ? Player->GetWeaponDataInSlot(EWeaponSlot::Primary) : nullptr;
	const UWeaponDataAsset* Secondary = Player ? Player->GetWeaponDataInSlot(EWeaponSlot::Secondary) : nullptr;
	Health = Combat ? Combat->GetHealth() : 0;
	MaxHealth = Combat ? Combat->GetMaxHealth() : 0;
	PrimaryEnergy = Energy ? Energy->GetPrimaryEnergy() : 0;
	PrimaryMaxEnergy = Energy ? Energy->GetPrimaryMaxEnergy() : 0;
	SecondaryEnergy = Energy ? Energy->GetSecondaryEnergy() : 0;
	SecondaryMaxEnergy = Energy ? Energy->GetSecondaryMaxEnergy() : 0;
	bPrimaryRanged = Ranged(Primary);
	bSecondaryRanged = Ranged(Secondary);
	if (HealthBar) HealthBar->SetPercent(Fraction(Health, MaxHealth));
	if (HealthText) HealthText->SetText(ResourceText(Health, MaxHealth));
	WeaponVisual(Primary, PrimaryWeaponText, PrimaryWeaponIcon);
	WeaponVisual(Secondary, SecondaryWeaponText, SecondaryWeaponIcon);
	if (PrimaryEnergyRow) PrimaryEnergyRow->SetVisibility(bPrimaryRanged ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (SecondaryEnergyRow) SecondaryEnergyRow->SetVisibility(bSecondaryRanged ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (PrimaryEnergyBar) PrimaryEnergyBar->SetPercent(Fraction(PrimaryEnergy, PrimaryMaxEnergy));
	if (SecondaryEnergyBar) SecondaryEnergyBar->SetPercent(Fraction(SecondaryEnergy, SecondaryMaxEnergy));
	if (PrimaryEnergyText) PrimaryEnergyText->SetText(ResourceText(PrimaryEnergy, PrimaryMaxEnergy));
	if (SecondaryEnergyText) SecondaryEnergyText->SetText(ResourceText(SecondaryEnergy, SecondaryMaxEnergy));
	OnStatusUpdated();
}
