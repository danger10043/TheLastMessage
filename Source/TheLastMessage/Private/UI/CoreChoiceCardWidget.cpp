#include "UI/CoreChoiceCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "DataAsset/CoreDataAsset.h"
#include "Engine/Texture2D.h"

// 언리얼이 제공하는 UUserWidget::NativeOnInitialized()를 재정의하여, WBP에서 배치한 위젯과 연결하고 버튼 이벤트를 처리한다.
void UCoreChoiceCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// 방향키 입력은 부모 선택 화면에서 처리하므로, 카드 위젯 자체는 키보드 포커스를 받지 않는다.
	SetIsFocusable(false);

	// 개별 코어 아이콘으로 바꾸기 전에 WBP에 지정된 기본 이미지를 기억한다.
	if (CoreIconImage)
	{
		DefaultCoreIcon = Cast<UTexture2D>(CoreIconImage->GetBrush().GetResourceObject());
	}

	// WBP에 배치된 버튼의 입력을 C++ 처리 함수에 연결한다.
	if (CardButton)
	{
		CardButton->OnClicked.AddDynamic(this, &UCoreChoiceCardWidget::HandleClicked);
		CardButton->OnHovered.AddDynamic(this, &UCoreChoiceCardWidget::HandleHovered);
		// 표시할 코어가 준비되기 전에는 카드를 선택할 수 없게 한다.
		CardButton->SetIsEnabled(IsValid(CoreData));
	}
}

void UCoreChoiceCardWidget::SetCoreData(UCoreDataAsset* InCoreData, int32 InDisplayIndex)
{
	CoreData = InCoreData;
	DisplayIndex = InDisplayIndex;

	// CoreData가 유효한지 확인한다.
	const bool bHasCore = IsValid(CoreData);

	// 레이아웃은 WBP가 담당하고, C++은 데이터 에셋의 내용을 위젯에 표시한다.

	// bHasCore가 true이면 코어 이름을 표시하고, false이면 빈 문자열을 표시한다.
	if (CoreNameText)
	{
		CoreNameText->SetText(bHasCore ? CoreData->CoreName : FText::GetEmpty());
	}

	if (CoreDescriptionText)
	{
		CoreDescriptionText->SetText(bHasCore ? CoreData->CoreDescription : FText::GetEmpty());
	}
	if (CoreIconImage)
	{
		UTexture2D* IconTexture = nullptr;
		if (bHasCore)
		{
			// 같은 카드에 다른 코어를 표시할 때도 이전 코어의 이미지가 남지 않도록 한다.
			IconTexture = IsValid(CoreData->CoreIcon) ? CoreData->CoreIcon.Get() : DefaultCoreIcon.Get();
		}
		CoreIconImage->SetBrushFromTexture(IconTexture, true);
		// 표시할 이미지가 없으면 숨기되, 아이콘 영역의 공간은 유지한다.
		CoreIconImage->SetVisibility(IsValid(IconTexture)
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (CardButton)
	{
		CardButton->SetIsEnabled(bHasCore);
	}
}

void UCoreChoiceCardWidget::SetHighlighted(bool bInHighlighted)
{
	if (bIsHighlighted == bInHighlighted) return;
	bIsHighlighted = bInHighlighted;
	// C++은 상태 변경만 알리고, 강조를 그리는 방법은 WBP에 맡긴다.
	OnHighlightChanged(bIsHighlighted);
}

void UCoreChoiceCardWidget::HandleClicked()
{
	if (IsValid(CoreData))
	{
		// Broadcast는 연결된 선택 화면에 결과를 알리는 호출이다. 코어 지급은 그 알림을 받은 쪽에서 처리한다.
		OnCoreChosen.Broadcast(CoreData);
	}
}

// 마우스가 카드 위에 올라오면 선택 화면에 카드 위치를 알린다.
void UCoreChoiceCardWidget::HandleHovered()
{
	OnCoreHovered.Broadcast(DisplayIndex);
}
