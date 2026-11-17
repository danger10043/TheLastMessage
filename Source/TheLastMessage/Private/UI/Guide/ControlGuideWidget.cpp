#include "UI/Guide/ControlGuideWidget.h"

void UControlGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면이 구성되면 현재 안내 종류를 반영한다.
	OnGuideTypeChanged(GuideType);
}

void UControlGuideWidget::SetGuideType(EControlGuideType InGuideType)
{
	GuideType = InGuideType;

	// 위젯 블루프린트에 화면 갱신을 요청한다.
	OnGuideTypeChanged(GuideType);
}