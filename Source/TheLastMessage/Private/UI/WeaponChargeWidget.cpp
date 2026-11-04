#include "UI/WeaponChargeWidget.h"
#include "UI/FactoryUIStyle.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UWeaponChargeWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (UBorder* Panel = Cast<UBorder>(GetWidgetFromName(TEXT("ChargePanel"))))
	{
		Panel->SetBrush(FactoryUI::Chamfer(FactoryUI::EPlate::Screen));
		Panel->SetBrushColor(FLinearColor::White);
	}
	if (ChargeBar)
	{
		ChargeBar->SetWidgetStyle(FactoryUI::Meter(ChargeBar->GetWidgetStyle()));
		ChargeBar->SetFillColorAndOpacity(bFullyCharged ? FactoryUI::Amber : FactoryUI::Cyan);
	}
	if (ChargeText) ChargeText->SetColorAndOpacity(bFullyCharged ? FactoryUI::Amber : FactoryUI::Text);
}

void UWeaponChargeWidget::SetChargeProgress(float Ratio)
{
	const float NewRatio = FMath::IsFinite(Ratio) ? FMath::Clamp(Ratio, 0.0f, 1.0f) : 0.0f;
	if (ChargeRatio == NewRatio) return;
	ChargeRatio = NewRatio;
	bFullyCharged = ChargeRatio >= 1.0f;
	if (ChargeBar)
	{
		ChargeBar->SetPercent(ChargeRatio);
		ChargeBar->SetFillColorAndOpacity(bFullyCharged ? FactoryUI::Amber : FactoryUI::Cyan);
	}
	if (ChargeText)
	{
		ChargeText->SetColorAndOpacity(bFullyCharged ? FactoryUI::Amber : FactoryUI::Text);
		ChargeText->SetText(bFullyCharged ? NSLOCTEXT("PlayerUI", "ChargeReady", "READY") :
			FText::Format(NSLOCTEXT("PlayerUI", "ChargePercent", "{0}%"), FText::AsNumber(FMath::FloorToInt(ChargeRatio * 100))));
	}
	OnChargeUpdated();
}
