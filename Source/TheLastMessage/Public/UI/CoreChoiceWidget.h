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
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoreChoiceSelected, UCoreDataAsset*, CoreData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCoreChoiceClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoreRerollRequested, int32, ChoiceIndex);

/**
 * 게임을 일시정지하고 코어 후보를 최대 3개 보여 주며, 선택·리롤 요청을 전달하는 화면이다.
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

	/** 기본적으로 페이드를 마친 뒤 화면을 닫는다. 강제 정리가 필요하면 bAnimate를 false로 전달한다. */
	UFUNCTION(BlueprintCallable, Category = "Core|UI")
	void CloseChoiceUI(bool bAnimate = true);

	/** 재추첨 결과를 해당 카드 한 장에 반영한다. 나머지 카드는 그대로 유지한다. */
	UFUNCTION(BlueprintCallable, Category = "Core|UI")
	bool UpdateCoreChoice(int32 ChoiceIndex, UCoreDataAsset* CoreData);

	/** 보상 쪽에서 확인한 카드별 리롤 가능 여부를 해당 버튼에 반영한다. */
	UFUNCTION(BlueprintCallable, Category = "Core|UI")
	void SetRerollEnabled(int32 ChoiceIndex, bool bEnabled);

	/** 선택한 코어를 알린다. 호출한 쪽에서 지급 성공 여부를 확인한 뒤 화면을 닫는다. */
	UPROPERTY(BlueprintAssignable, Category = "Core|UI")
	FOnCoreChoiceSelected OnCoreSelected;

	/** 획득 완료나 외부 제거로 화면이 닫혔음을 알린다. 보상 쪽에서 진행 중인 선택 기록을 정리한다. */
	UPROPERTY(BlueprintAssignable, Category = "Core|UI")
	FOnCoreChoiceClosed OnChoiceClosed;

	/** 리롤을 요청한 카드의 위치(0부터 시작)를 알린다. 실제 재추첨은 보상 처리 쪽에서 진행한다. */
	UPROPERTY(BlueprintAssignable, Category = "Core|UI")
	FOnCoreRerollRequested OnRerollRequested;

protected:
	/** WBP의 버튼을 C++ 처리 함수와 연결하고, 화면이 키보드 입력을 받을 수 있게 한다. */
	virtual void NativeOnInitialized() override;
	/** 화면이 제거되면 입력과 커서를 복구하고, 이 화면이 건 일시정지를 해제한다. */
	virtual void NativeDestruct() override;
	/** 화면을 닫는 동안 투명도를 줄이고, 페이드가 끝나면 위젯을 제거한다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** Space·Enter·ESC 입력을 먼저 소비해 자식 버튼이나 다른 입력 경로로 넘기지 않는다. 선택은 마우스로만 하고 취소는 없다. */
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	/** 방향키·Tab으로 강조 카드를 옮긴다. 선택은 마우스로만 한다. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** 후보를 표시할 카드 WBP 클래스. 카드의 모양은 해당 WBP에서 편집한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|UI")
	TSubclassOf<UCoreChoiceCardWidget> CardWidgetClass;

	/** 동적으로 생성하는 카드의 바깥 여백. WBP의 클래스 기본값에서 카드 간격을 조절한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|Layout")
	FMargin CardPadding = FMargin(12.0f, 0.0f);

	/** 선택 화면이 사라지는 데 걸리는 시간. 0이면 즉시 닫는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|Appearance", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CloseFadeDuration = 0.35f;

	/** 후보 카드들을 가로로 배치할 WBP 영역. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> CardsBox;

	/** 현재 가리키는 카드의 순서와 전체 후보 수를 표시한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SelectionCounter;

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
	/** 후보마다 카드와 입력 이벤트를 준비한다. 모든 카드 생성에 성공하면 true를 반환한다. */
	bool PopulateCards();
	/** 표시 대상 코어의 현재 보유 단계를 조회한다. 최초 표시와 리롤에 같은 기준을 사용한다. */
	int32 GetCurrentCoreLevel(const UCoreDataAsset* CoreData) const;
	/** 입력 대상과 카드별 강조 상태를 바꾸고 화면 상단의 선택 위치를 갱신한다. */
	void SetActiveCardIndex(int32 Index);
	/** 선택한 코어를 외부에 알린다. 지급 결과에 따라 외부에서 화면을 닫을 수 있다. */
	void HandleCardChosen(UCoreDataAsset* CoreData);
	/** 게임을 일시정지하고 입력을 UI로 옮긴다. 일시정지에 실패하면 화면을 열지 않는다. */
	bool CapturePlayerInput();
	/** 화면 입력 관리가 끝나면 게임 전용 입력으로 바꾸고 이전 커서 표시 상태를 복구한다. */
	void RestorePlayerInput();
	/** 선택 화면이 열려 있는 동안 다른 곳에서 일시정지를 풀지 못하게 한다. */
	bool CanUnpauseAfterClose() const;
	/** 추가 입력을 막고 페이드 또는 즉시 종료를 시작한다. */
	void BeginClose(bool bAnimate);

	/** 유효한 카드 위치의 리롤 요청을 외부에 전달한다. */
	void HandleReroll(int32 ChoiceIndex);

	/** 카드 위치에 해당하는 WBP 리롤 버튼을 찾는다. */
	UButton* GetRerollButton(int32 ChoiceIndex) const;

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

	/** 방향키 또는 마우스로 가리킨 카드의 위치. 강조 표시와 카드 순번 표시에 사용한다. */
	int32 ActiveCardIndex = 0;
	/** 이 화면이 플레이어 입력을 UI로 전환했는지 기록한다. 중복 복구를 막는 데 사용한다. */
	bool bCapturedInput = false;
	/** 입력을 가져온 컨트롤러와 월드. 맵이 바뀐 뒤 새 월드의 입력·일시정지를 건드리지 않는다. */
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> InputOwner;
	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> InputWorld;
	/** 이미 멈춰 있던 게임은 화면 종료 시에도 멈춘 상태를 유지한다. */
	bool bPausedByThisScreen = false;
	/** 화면을 열기 전 마우스 커서 표시 상태. */
	bool bPreviousMouseCursorVisible = false;
	/** 화면을 닫는 중인지 나타낸다. 종료 중 추가 선택·리롤 요청을 막는다. */
	bool bClosing = false;
	/** 페이드 경과 시간과 시작 투명도. 현재 투명도에서 자연스럽게 사라지도록 계산한다. */
	float CloseFadeElapsed = 0.0f;
	float CloseStartOpacity = 1.0f;
};
