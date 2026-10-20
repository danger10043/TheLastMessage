#include "Character/Enemy/UI/EnemyHealthBarWidget.h"
#include "Components/ProgressBar.h"

void UEnemyHealthBarWidget::SetHealthPercent(float Percent, bool bInstant)
{
	TargetPercent = FMath::Clamp(Percent, 0.f, 1.f);

	if (HealthBar)
	{
		HealthBar->SetPercent(TargetPercent);
	}

	if (bInstant || TargetPercent >= DamagePercent)
	{
		// 회복·초기화는 지연 없이 맞춘다.
		DamagePercent = TargetPercent;
		DelayRemaining = 0.f;
		if (DamageBar)
		{
			DamageBar->SetPercent(DamagePercent);
		}
	}
	else
	{
		DelayRemaining = DamageBarDelay;
	}
}

void UEnemyHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DamagePercent <= TargetPercent)
	{
		return;
	}

	if (DelayRemaining > 0.f)
	{
		DelayRemaining -= InDeltaTime;
		return;
	}

	DamagePercent = FMath::Max(TargetPercent, DamagePercent - DamageBarSpeed * InDeltaTime);
	if (DamageBar)
	{
		DamageBar->SetPercent(DamagePercent);
	}
}
