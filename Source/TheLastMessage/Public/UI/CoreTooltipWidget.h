#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CoreTooltipWidget.generated.h"

class UTextBlock;

/**
 * 코어 카드에 마우스를 올리면 커서 옆에 뜨는 툴팁이다.
 * 카드는 크기가 정해져 있어 긴 설명이 잘릴 수 있으므로, 같은 내용을 잘리지 않는 크기로 다시 보여 준다.
 * 모양은 WBP가 정하고, 이 클래스는 카드가 넘겨준 문구를 표시만 한다.
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API UCoreTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 카드에 표시한 것과 같은 이름·단계·설명·조작 문구를 받아 툴팁에 표시한다. */
	void SetContent(const FText& InName, const FText& InLevel, const FText& InDescription, const FText& InUsage);

protected:
	/** BindWidget은 WBP에서 같은 이름으로 배치한 위젯을 이 멤버와 연결한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CoreNameText;

	/** 전체 효과 설명. WBP에서 자동 줄바꿈과 최대 너비를 설정해 잘리지 않게 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CoreDescriptionText;

	/** 신규/강화 단계 문구. 툴팁에 단계를 보여 주지 않을 WBP는 배치하지 않아도 된다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoreLevelText;

	/** 조작 문구. 문구가 없는 코어에서는 접어서 숨긴다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoreUsageText;
};
