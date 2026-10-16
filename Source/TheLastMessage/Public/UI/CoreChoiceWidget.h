#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Layout/Margin.h"
#include "CoreChoiceWidget.generated.h"

class APlayerController;
class UButton;
class UCoreChoiceCardWidget;
class UCoreDataAsset;
class UHorizontalBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoreChoiceSelected, UCoreDataAsset*, CoreData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCoreChoiceCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoreRerollRequested, int32, ChoiceIndex);

/**
 * 코어 후보를 최대 3개 보여 주고, 선택·취소·리롤 요청을 전달하는 화면이다.
 * WBP가 배치와 배경 블러를 담당하며, 이 클래스는 카드 데이터와 화면 입력을 관리한다.
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API UCoreChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 지정한 화면 WBP로 로컬 플레이어의 선택 화면을 연다. 유효한 후보가 없으면 열지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Core|UI")
	static UCoreChoiceWidget* OpenCoreChoiceUI(APlayerController* PlayerController,
		const TArray<UCoreDataAsset*>& Choices, TSubclassOf<UCoreChoiceWidget> ScreenClass);

	/** 지급 성공 후 호출하여 선택 화면을 닫고 플레이어 조작을 게임으로 돌려준다. */
	UFUNCTION(BlueprintCallable, Category = "Core|UI")
	void CloseChoiceUI();

	/** 선택한 코어를 알린다. 호출한 쪽에서 지급 성공 여부를 확인한 뒤 화면을 닫는다. */
	UPROPERTY(BlueprintAssignable, Category = "Core|UI")
	FOnCoreChoiceSelected OnCoreSelected;

	/** 선택 취소를 알린다. 호출한 쪽에서 임시 후보와 보상 선택 상태를 정리한다. */
	UPROPERTY(BlueprintAssignable, Category = "Core|UI")
	FOnCoreChoiceCancelled OnChoiceCancelled;

	/** 리롤을 요청한 카드의 위치(0부터 시작)를 알린다. 실제 재추첨은 보상 처리 쪽에서 진행한다. */
	UPROPERTY(BlueprintAssignable, Category = "Core|UI")
	FOnCoreRerollRequested OnRerollRequested;

protected:
	/** WBP의 버튼을 C++ 처리 함수와 연결하고, 화면이 키보드 입력을 받을 수 있게 한다. */
	virtual void NativeOnInitialized() override;
	/** 화면이 제거되면 게임 입력과 마우스 커서 상태를 복구한다. */
	virtual void NativeDestruct() override;
	/** 방향키·확인·취소 키 입력을 화면 동작으로 바꾼다. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** 후보를 표시할 카드 WBP 클래스. 카드의 모양은 해당 WBP에서 편집한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|UI")
	TSubclassOf<UCoreChoiceCardWidget> CardWidgetClass;

	/** 동적으로 생성하는 카드의 바깥 여백. WBP의 클래스 기본값에서 카드 간격을 조절한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|Layout")
	FMargin CardPadding = FMargin(12.0f, 0.0f);

	/** 후보 카드들을 가로로 배치할 WBP 영역. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> CardsBox;

	/** 현재 가리키는 카드의 순서와 전체 후보 수를 표시한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SelectionCounter;

	/** 선택을 취소하고 게임으로 돌아가는 버튼. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

	/** 카드 아래의 리롤 버튼들을 배치할 영역. 후보 수에 맞춰 표시한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> RerollButtonsBox;

	/** 첫 번째 카드 아래에 있는 리롤 버튼. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> FirstRerollButton;

	/** 두 번째 카드 아래에 있는 리롤 버튼. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SecondRerollButton;

	/** 세 번째 카드 아래에 있는 리롤 버튼. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ThirdRerollButton;

private:
	/** 표시할 후보마다 카드 WBP를 만들어 코어 데이터와 선택·마우스 이벤트를 연결한다. */
	void PopulateCards();
	/** 입력 대상과 카드별 강조 상태를 바꾸고 화면 상단의 선택 위치를 갱신한다. */
	void SetActiveCardIndex(int32 Index);
	/** 선택한 코어를 외부에 알린다. 지급 결과에 따라 외부에서 화면을 닫을 수 있다. */
	void HandleCardChosen(UCoreDataAsset* CoreData);
	/** 화면을 조작하는 동안 입력 대상을 UI로 바꾸고 마우스 커서를 표시한다. */
	void CapturePlayerInput();
	/** 화면 입력 관리가 끝나면 게임 전용 입력으로 바꾸고 이전 커서 표시 상태를 복구한다. */
	void RestorePlayerInput();

	/** 취소를 알리고 화면을 닫는다. */
	UFUNCTION()
	void HandleCancel();

	/** 유효한 카드 위치의 리롤 요청을 외부에 전달한다. */
	void HandleReroll(int32 ChoiceIndex);

	// 버튼의 OnClicked에는 카드 위치가 없으므로, 각 버튼에서 공통 함수로 위치를 전달한다.
	UFUNCTION()
	void HandleFirstReroll();

	UFUNCTION()
	void HandleSecondReroll();

	UFUNCTION()
	void HandleThirdReroll();

	/** 현재 화면에 표시하는 후보 데이터. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCoreDataAsset>> DisplayedChoices;

	/** 화면에 생성한 카드 위젯들. 현재 카드 수와 유효한 선택 위치를 확인할 때 사용한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCoreChoiceCardWidget>> Cards;

	/** 방향키 또는 마우스로 가리킨 카드의 위치. 확인 키를 누르면 이 위치의 코어를 선택한다. */
	int32 ActiveCardIndex = 0;
	/** 이 화면이 플레이어 입력을 UI로 전환했는지 기록한다. 중복 복구를 막는 데 사용한다. */
	bool bCapturedInput = false;
	/** 화면을 열기 전 마우스 커서 표시 상태. */
	bool bPreviousMouseCursorVisible = false;
	/** 화면을 닫는 중인지 나타낸다. 종료 중 추가 선택·취소·리롤 요청을 막는다. */
	bool bClosing = false;
};
