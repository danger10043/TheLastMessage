#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CoreChoiceCardWidget.generated.h"

class UButton;
class UCoreDataAsset;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCoreCardChosen, UCoreDataAsset*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCoreCardHovered, int32);

/**
 * 코어 하나의 이름·설명·아이콘을 표시하는 카드다.
 * WBP가 카드의 모양을 정하고, 이 클래스는 데이터 표시와 선택·마우스 알림을 담당한다.
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API UCoreChoiceCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 코어와 카드 위치(0부터 시작)를 설정한다. 위치는 마우스로 가리킨 카드를 구분할 때 사용한다. */
	UFUNCTION(BlueprintCallable, Category = "Core|UI")
	void SetCoreData(UCoreDataAsset* InCoreData, int32 InDisplayIndex);

	/** 입력 대상으로 가리키는 카드인지 설정하고, 변경된 상태를 WBP에 알린다. */
	UFUNCTION(BlueprintCallable, Category = "Core|UI")
	void SetHighlighted(bool bInHighlighted);

	/** 클릭한 코어를 부모 선택 화면에 전달한다. */
	FOnCoreCardChosen OnCoreChosen;
	/** 마우스로 가리킨 카드의 위치를 부모 선택 화면에 전달한다. */
	FOnCoreCardHovered OnCoreHovered;

protected:
	/** WBP의 기본 아이콘을 보관하고 카드 버튼의 클릭·마우스 이벤트를 연결한다. */
	virtual void NativeOnInitialized() override;

	/** 강조 상태가 바뀌면 호출된다. 테두리 색상이나 애니메이션은 WBP에서 작성한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Core|UI")
	void OnHighlightChanged(bool bHighlighted);

	/** 현재 입력 대상으로 가리키는 카드인지 나타낸다. WBP에서 표현을 결정할 때 읽는다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Core|UI")
	bool bIsHighlighted = false;

	/** WBP의 같은 이름을 가진 위젯과 연결하여 카드 전체의 클릭을 받는다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CardButton;

	/** 코어별 아이콘을 표시하며, 개별 아이콘이 없으면 WBP의 기본 아이콘을 사용한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> CoreIconImage;

	/** BindWidget은 WBP에서 같은 이름으로 배치한 위젯을 이 멤버와 연결한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CoreNameText;

	/** 코어의 효과 설명을 표시한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CoreDescriptionText;

private:
	/** 카드 버튼의 클릭을 코어 선택 알림으로 전달한다. */
	UFUNCTION()
	void HandleClicked();

	/** 카드 버튼에 마우스가 올라오면 카드 위치를 알린다. */
	UFUNCTION()
	void HandleHovered();

	/** 현재 카드에 표시하는 코어 데이터. Transient이므로 실행 중에만 보관하고 저장하지 않는다. */
	UPROPERTY(Transient)
	TObjectPtr<UCoreDataAsset> CoreData;

	/** 데이터 교체 시 기본 이미지로 돌아갈 수 있도록 WBP에 지정된 아이콘을 보관한다. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultCoreIcon;

	/** 마우스로 가리킨 카드의 위치를 선택 화면에 알릴 때 사용하는 순서다. */
	int32 DisplayIndex = 0;
};
