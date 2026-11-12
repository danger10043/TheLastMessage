#include "UI/PlayerStatusWidget.h"
#include "Character/Player/PlayerBase.h"
#include "AbilitySystemComponent.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "UI/PlayerResourceBarWidget.h"
#include "UI/PlayerWeaponSlotWidget.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Character/Player/GrapplingHookComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

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
			UWeaponSlotAttributeSet::GetSharedEnergyAttribute(), UWeaponSlotAttributeSet::GetSharedMaxEnergyAttribute()};
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

void UPlayerStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Observe cursor geometry without entering Slate's hit-test path: aiming and
	// firing must still reach the game through every part of the status panel.
	if (WidgetTree && FSlateApplication::IsInitialized())
	{
		if (UWidget* Plate = WidgetTree->FindWidget(TEXT("UnifiedStatusPlate")))
		{
			const FGeometry& Geometry = Plate->GetCachedGeometry();
			const bool bOver = Geometry.IsUnderLocation(FSlateApplication::Get().GetCursorPos());
			Plate->SetRenderOpacity(FMath::FInterpTo(Plate->GetRenderOpacity(), bOver ? 0.28f : 1.0f, InDeltaTime, 12.0f));
		}
	}
	RefreshLoadout();
	RefreshEnergyDisplay();
	if (WidgetTree)
	{
		const APlayerBase* Pawn=ObservedPlayer.Get();
		const auto* Hook=Pawn ? Pawn->FindComponentByClass<UGrapplingHookComponent>() : nullptr;
		const bool bUnlocked=Hook && Hook->IsGrappleUnlocked();
		if (UWidget* Row=WidgetTree->FindWidget(TEXT("GrappleStatusRow")))
			Row->SetVisibility(bUnlocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (bUnlocked)
		{
			const float Remaining=Hook->GetCooldownRemaining();
			const FLinearColor Color=Remaining>0 ? FLinearColor(1,.65f,.22f) : FLinearColor(.35f,.9f,1);
			if (auto* Bar=Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("GrappleStatusProgress"))))
			{Bar->SetPercent(Hook->GetCooldownProgress());Bar->SetFillColorAndOpacity(Color);}
			if (auto* Text=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("GrappleStatusText"))))
			{Text->SetText(FText::FromString(Remaining>0 ? FString::Printf(TEXT("%.1fs"),Remaining) : TEXT("READY")));Text->SetColorAndOpacity(Color);}
		}
	}
}

void UPlayerStatusWidget::RefreshLoadout()
{
	APlayerBase* Player = ObservedPlayer.Get();
	UPlayerWeaponSlotWidget* Cards[] = { WeaponSlot1, WeaponSlot2, WeaponSlot3, WeaponSlot4 };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (Cards[Index]) Cards[Index]->SetWeaponPresentation(Player ? Player->GetWeaponChoice(Index) : nullptr, Index + 1,
			Player && Player->HasWeaponChoice(Index), Player ? Player->GetWeaponChoiceAssignment(Index) : EWeaponSlot::None,
			Player && Player->GetHeldWeaponChoice() == Index);
	}
}

void UPlayerStatusWidget::RefreshEnergyDisplay()
{
	APlayerBase* Player = ObservedPlayer.Get();
	const UWeaponSlotAttributeSet* Energy = Player ? Player->GetWeaponSlotAttributes() : nullptr;
	DisplayEnergy = Energy ? Energy->GetSharedEnergy() : 0;
	DisplayMaxEnergy = Energy ? Energy->GetSharedMaxEnergy() : 0;
	if (EnergyResource)
	{
		EnergyResource->SetResourceValues(DisplayEnergy, DisplayMaxEnergy);
		EnergyResource->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UPlayerStatusWidget::RefreshStatus()
{
	APlayerBase* Player = ObservedPlayer.Get();
	const UCombatAttributeSet* Combat = Player ? Player->GetCombatAttributes() : nullptr;
	Health = Combat ? Combat->GetHealth() : 0;
	MaxHealth = Combat ? Combat->GetMaxHealth() : 0;
	if (HealthResource) HealthResource->SetResourceValues(Health, MaxHealth);
	RefreshLoadout();
	RefreshEnergyDisplay();
	OnStatusUpdated();
}
