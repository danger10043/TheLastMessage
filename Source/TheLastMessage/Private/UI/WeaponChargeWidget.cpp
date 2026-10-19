#include "UI/WeaponChargeWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UWeaponChargeWidget::SetChargeProgress(float Ratio)
{
	const float NewRatio = FMath::IsFinite(Ratio) ? FMath::Clamp(Ratio, 0.0f, 1.0f) : 0.0f;
	if (ChargeRatio == NewRatio) return;
	ChargeRatio = NewRatio;
	bFullyCharged = ChargeRatio >= 1.0f;
	if (ChargeBar)
	{
		ChargeBar->SetPercent(ChargeRatio);
		ChargeBar->SetFillColorAndOpacity(bFullyCharged ? ReadyColor : ChargingColor);
	}
	if (ChargeText)
	{
		ChargeText->SetText(bFullyCharged ? NSLOCTEXT("PlayerUI", "ChargeReady", "READY") :
			FText::Format(NSLOCTEXT("PlayerUI", "ChargePercent", "{0}%"), FText::AsNumber(FMath::FloorToInt(ChargeRatio * 100))));
	}
	OnChargeUpdated();
}
