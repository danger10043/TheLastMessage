#include "UI/CoreTooltipWidget.h"

#include "Components/TextBlock.h"

void UCoreTooltipWidget::SetContent(const FText& InName, const FText& InLevel, const FText& InDescription, const FText& InUsage)
{
	// 카드가 계산한 문구를 그대로 받아 표시하므로, 카드와 툴팁의 내용이 서로 달라지지 않는다.
	if (CoreNameText) CoreNameText->SetText(InName);
	if (CoreLevelText) CoreLevelText->SetText(InLevel);
	if (CoreDescriptionText) CoreDescriptionText->SetText(InDescription);
	// 조작 문구가 없는 코어는 줄을 접어 설명 아래에 빈 공간이 남지 않게 한다.
	if (CoreUsageText)
	{
		CoreUsageText->SetText(InUsage);
		CoreUsageText->SetVisibility(InUsage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
