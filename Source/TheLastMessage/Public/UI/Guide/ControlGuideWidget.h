#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Guide/ControlGuideTypes.h"
#include "ControlGuideWidget.generated.h"

/** 조작법 안내 UI의 부모 클래스 */
UCLASS()
class THELASTMESSAGE_API UControlGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 액터가 전달한 안내 종류를 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Control Guide")
	void SetGuideType(EControlGuideType InGuideType);

protected:
	virtual void NativeConstruct() override;

	/** 현재 위젯에서 표시할 안내 종류 */
	UPROPERTY(BlueprintReadOnly, Category = "Control Guide")
	EControlGuideType GuideType = EControlGuideType::Move;

	/** 안내 종류에 맞는 화면 구성은 위젯 블루프린트에서 구현한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Control Guide")
	void OnGuideTypeChanged(EControlGuideType NewGuideType);
};