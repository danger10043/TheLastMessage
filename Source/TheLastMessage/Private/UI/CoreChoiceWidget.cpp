#include "UI/CoreChoiceWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "DataAsset/CoreDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UI/CoreChoiceCardWidget.h"



UCoreChoiceWidget* UCoreChoiceWidget::OpenCoreChoiceUI(APlayerController* PlayerController,
	const TArray<FCoreChoiceEntry>& Choices, TSubclassOf<UCoreChoiceWidget> ScreenClass)
{
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController()
		|| !ScreenClass || ScreenClass->HasAnyClassFlags(CLASS_Abstract)) return nullptr;

	// 이전 화면이 사라지는 중에도 새 화면이 입력 복구 상태를 덮어쓰지 않도록 한다.
	TArray<UUserWidget*> ExistingScreens;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayerController, ExistingScreens, StaticClass(), true);
	for (UUserWidget* Existing : ExistingScreens)
	{
		if (Existing->IsInViewport() && Existing->GetOwningPlayer() == PlayerController) return nullptr;
	}

	// 비어 있거나 ID가 중복된 항목을 걸러, 표시 가능한 후보를 최대 3개 준비한다.
	TArray<FCoreChoiceEntry> ValidChoices;
	TSet<FName> Ids;
	for (const FCoreChoiceEntry& Entry : Choices)
	{
		const UCoreDataAsset* Data = Entry.CoreData.Get();
		if (!IsValid(Data) || Data->CoreId.IsNone() || Ids.Contains(Data->CoreId)) continue;
		ValidChoices.Add(Entry);
		Ids.Add(Data->CoreId);
		if (ValidChoices.Num() == CoreRewardRules::MaxChoices) break;
	}
	if (ValidChoices.IsEmpty()) return nullptr;

	// 전달받은 WBP 클래스로 화면을 만들어 디자이너에서 작성한 배치를 사용한다.
	UCoreChoiceWidget* Screen = CreateWidget<UCoreChoiceWidget>(PlayerController, ScreenClass);
	if (!Screen) return nullptr;
	for (const FCoreChoiceEntry& Entry : ValidChoices)
	{
		Screen->DisplayedChoices.Add(Entry.CoreData);
		Screen->DisplayedLevels.Add(FMath::Max(0, Entry.CurrentLevel));
	}
	if (!Screen->PopulateCards()) return nullptr;
	// 카드 준비가 끝나면 화면을 표시하고 플레이어 입력을 이 화면으로 옮긴다.
	Screen->AddToViewport(100);
	if (!Screen->IsInViewport() || !Screen->CapturePlayerInput())
	{
		Screen->CloseChoiceUI(false);
		return nullptr;
	}
	return Screen;
}

void UCoreChoiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (HintText) DefaultStatusMessage = HintText->GetText();
	// WBP에서 만든 버튼에 동작을 연결한다. 배치와 색상은 디자이너에서 관리한다.
	if (FirstRerollButton) FirstRerollButton->OnClicked.AddDynamic(this, &UCoreChoiceWidget::HandleFirstReroll);
	if (SecondRerollButton) SecondRerollButton->OnClicked.AddDynamic(this, &UCoreChoiceWidget::HandleSecondReroll);
	if (ThirdRerollButton) ThirdRerollButton->OnClicked.AddDynamic(this, &UCoreChoiceWidget::HandleThirdReroll);
	for (int32 Index = 0; Index < CoreRewardRules::MaxChoices; ++Index) SetRerollEnabled(Index, false);
}

bool UCoreChoiceWidget::PopulateCards()
{
	if (!CardsBox || !CardWidgetClass || CardWidgetClass->HasAnyClassFlags(CLASS_Abstract)
		|| DisplayedChoices.IsEmpty()) return false;
	// WBP의 미리보기 카드 또는 이전에 생성한 카드를 비우고 현재 후보로 다시 구성한다.
	CardsBox->ClearChildren();
	Cards.Reset();
	for (int32 Index = 0; Index < DisplayedChoices.Num(); ++Index)
	{
		UCoreChoiceCardWidget* Card = CreateWidget<UCoreChoiceCardWidget>(GetOwningPlayer(), CardWidgetClass);
		if (!Card)
		{
			// 일부 카드만 남아 데이터와 화면 순서가 어긋나지 않도록 전체 준비를 취소한다.
			CardsBox->ClearChildren();
			Cards.Reset();
			return false;
		}
		Card->SetCoreData(DisplayedChoices[Index], Index, DisplayedLevels.IsValidIndex(Index) ? DisplayedLevels[Index] : 0);
		// 카드가 보낸 클릭·마우스 알림을 부모 화면의 처리 함수에 연결한다.
		Card->OnCoreChosen.AddUObject(this, &UCoreChoiceWidget::HandleCardChosen);
		Card->OnCoreHovered.AddUObject(this, &UCoreChoiceWidget::SetActiveCardIndex);
		Cards.Add(Card);
		// 카드는 코드에서 추가하되, 간격은 WBP의 클래스 기본값에 지정한 설정을 사용한다.
		CardsBox->AddChildToHorizontalBox(Card)->SetPadding(CardPadding);
	}
	// 후보가 없는 위치의 버튼과 여백을 함께 접어, 카드와 버튼의 가운데 정렬을 맞춘다.
	if (RerollButtonsBox)
	{
		for (int32 Index = 0; Index < RerollButtonsBox->GetChildrenCount(); ++Index)
		{
			RerollButtonsBox->GetChildAt(Index)->SetVisibility(Index < Cards.Num()
				? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	SetActiveCardIndex(0);
	return true;
}

void UCoreChoiceWidget::SetActiveCardIndex(int32 Index)
{
	if (bClosing || !Cards.IsValidIndex(Index) || !IsValid(DisplayedChoices[Index])) return;
	ActiveCardIndex = Index;
	// 어느 카드를 강조할지 전달한다. 카드는 WBP에 지정한 기본 색상과 강조 색상을 적용한다.
	for (int32 CardIndex = 0; CardIndex < Cards.Num(); ++CardIndex)
	{
		Cards[CardIndex]->SetHighlighted(CardIndex == Index);
	}
	if (SelectionCounter) SelectionCounter->SetText(FText::FromString(FString::Printf(TEXT("%02d / %02d"), Index + 1, Cards.Num())));
	// 마우스로 카드를 가리킨 뒤에도 방향키 입력을 부모 화면에서 받을 수 있게 한다.
	if (IsInViewport()) SetKeyboardFocus();
}

void UCoreChoiceWidget::HandleCardChosen(UCoreDataAsset* CoreData)
{
	if (bClosing || !IsValid(CoreData)) return;
	SetKeyboardFocus();
	// 지급 성공 여부에 따라 화면을 유지하거나 닫을 수 있도록 선택 결과만 전달한다.
	OnCoreSelected.Broadcast(CoreData);
}

void UCoreChoiceWidget::HandleReroll(int32 ChoiceIndex)
{
	UButton* RerollButton = GetRerollButton(ChoiceIndex);
	if (bClosing || !DisplayedChoices.IsValidIndex(ChoiceIndex)
		|| !RerollButton || !RerollButton->GetIsEnabled()) return;
	SetKeyboardFocus();
	// 여기서는 어떤 카드의 버튼인지 알린다. 후보 변경은 이 알림을 받은 쪽에서 처리한다.
	OnRerollRequested.Broadcast(ChoiceIndex);
}

bool UCoreChoiceWidget::UpdateCoreChoices(const TArray<FCoreChoiceEntry>& Choices)
{
	if (bClosing || Choices.Num() != Cards.Num()) return false;
	TSet<FName> Ids;
	int32 FirstValidIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		const UCoreDataAsset* Data = Choices[Index].CoreData.Get();
		if (!IsValid(Data)) continue;
		if (Data->CoreId.IsNone() || Ids.Contains(Data->CoreId)) return false;
		Ids.Add(Data->CoreId);
		if (FirstValidIndex == INDEX_NONE) FirstValidIndex = Index;
	}
	if (FirstValidIndex == INDEX_NONE) return false;
	DisplayedLevels.SetNum(Choices.Num());
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		UCoreDataAsset* Data = Choices[Index].CoreData.Get();
		DisplayedChoices[Index] = Data;
		DisplayedLevels[Index] = FMath::Max(0, Choices[Index].CurrentLevel);
		Cards[Index]->SetCoreData(Data, Index, DisplayedLevels[Index]);
		Cards[Index]->SetVisibility(IsValid(Data) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (RerollButtonsBox && Index < RerollButtonsBox->GetChildrenCount())
		{
			RerollButtonsBox->GetChildAt(Index)->SetVisibility(IsValid(Data) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	SetActiveCardIndex(IsValid(DisplayedChoices[ActiveCardIndex]) ? ActiveCardIndex : FirstValidIndex);
	return true;
}

void UCoreChoiceWidget::SetStatusMessage(const FText& Message)
{
	if (HintText) HintText->SetText(Message);
}

void UCoreChoiceWidget::ResetStatusMessage()
{
	SetStatusMessage(DefaultStatusMessage);
}

void UCoreChoiceWidget::MoveActiveCard(int32 Offset)
{
	for (int32 Step = 1; Step <= Cards.Num(); ++Step)
	{
		const int32 Index = (ActiveCardIndex + (Offset * Step) + Cards.Num()) % Cards.Num();
		if (IsValid(DisplayedChoices[Index])) { SetActiveCardIndex(Index); break; }
	}
}

UButton* UCoreChoiceWidget::GetRerollButton(int32 ChoiceIndex) const
{
	switch (ChoiceIndex)
	{
	case 0: return FirstRerollButton;
	case 1: return SecondRerollButton;
	case 2: return ThirdRerollButton;
	default: return nullptr;
	}
}

void UCoreChoiceWidget::SetRerollEnabled(int32 ChoiceIndex, bool bEnabled)
{
	if (UButton* Button = GetRerollButton(ChoiceIndex))
	{
		Button->SetIsEnabled(bEnabled && !bClosing && DisplayedChoices.IsValidIndex(ChoiceIndex)
			&& IsValid(DisplayedChoices[ChoiceIndex]));
	}
}

void UCoreChoiceWidget::HandleFirstReroll()
{
	HandleReroll(0);
}

void UCoreChoiceWidget::HandleSecondReroll()
{
	HandleReroll(1);
}

void UCoreChoiceWidget::HandleThirdReroll()
{
	HandleReroll(2);
}

bool UCoreChoiceWidget::CapturePlayerInput()
{
	if (bCapturedInput) return true;
	if (APlayerController* Player = GetOwningPlayer())
	{
		InputOwner = Player;
		InputWorld = Player->GetWorld();
		bPreviousMouseCursorVisible = Player->bShowMouseCursor;
		bCapturedInput = true;
		if (!UGameplayStatics::IsGamePaused(Player))
		{
			bPausedByThisScreen = Player->SetPause(true, FCanUnpause::CreateUObject(this, &UCoreChoiceWidget::CanUnpauseAfterClose));
			if (!bPausedByThisScreen)
			{
				bCapturedInput = false;
				InputOwner.Reset();
				InputWorld.Reset();
				return false;
			}
		}
		// UIOnly는 입력을 UI에만 전달한다. TakeWidget은 이 UMG 위젯의 내부 Slate 위젯을 가져온다.
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Player->SetInputMode(InputMode);
		Player->SetShowMouseCursor(true);
		// 화면을 열기 전에 누르고 있던 이동·공격 키 상태가 남지 않도록 비운다.
		Player->FlushPressedKeys();
		SetKeyboardFocus();
		return true;
	}
	return false;
}

bool UCoreChoiceWidget::CanUnpauseAfterClose() const
{
	return !bCapturedInput;
}

void UCoreChoiceWidget::RestorePlayerInput()
{
	if (!bCapturedInput) return;
	// CloseChoiceUI뿐 아니라 외부 RemoveFromParent로 제거된 경우에도 한 번만 복구한다.
	bCapturedInput = false;
	UWorld* World = InputWorld.Get();
	if (APlayerController* Player = InputOwner.Get(); IsValid(Player) && IsValid(World)
		&& !World->bIsTearingDown && Player->GetWorld() == World)
	{
		Player->SetInputMode(FInputModeGameOnly());
		Player->SetShowMouseCursor(bPreviousMouseCursorVisible);
		Player->FlushPressedKeys();
	}
	if (bPausedByThisScreen)
	{
		bPausedByThisScreen = false;
		// 컨트롤러가 먼저 파괴됐더라도 원래 월드의 일시정지는 정리한다.
		if (IsValid(World) && !World->bIsTearingDown)
		{
			if (AGameModeBase* GameMode = World->GetAuthGameMode()) GameMode->ClearPause();
		}
	}
	InputOwner.Reset();
	InputWorld.Reset();
}

void UCoreChoiceWidget::CloseChoiceUI(bool bAnimate)
{
	if (bClosing)
	{
		// 액터 파괴나 화면 교체 시에는 진행 중인 페이드도 즉시 끝낼 수 있다.
		if (!bAnimate && IsInViewport()) RemoveFromParent();
		return;
	}
	bClosing = true;
	CloseFadeElapsed = 0.0f;
	CloseStartOpacity = GetRenderOpacity();
	// 비활성화 색상이 입혀지지 않도록, 색상은 유지하면서 마우스 입력만 막는다.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!bAnimate || CloseFadeDuration <= KINDA_SMALL_NUMBER) RemoveFromParent();
}

void UCoreChoiceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bClosing || !IsInViewport()) return;

	CloseFadeElapsed += InDeltaTime;
	const float Progress = FMath::Clamp(CloseFadeElapsed / FMath::Max(CloseFadeDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	// 시작과 끝의 변화 속도를 완만하게 하여 갑자기 사라지는 느낌을 줄인다.
	SetRenderOpacity(CloseStartOpacity * (1.0f - FMath::SmoothStep(0.0f, 1.0f, Progress)));
	if (Progress >= 1.0f) RemoveFromParent();
}

void UCoreChoiceWidget::NativeDestruct()
{
	// 화면이 부모에서 제거되어 내부 Slate 위젯이 해제될 때 입력 관리도 끝낸다.
	bClosing = true;
	RestorePlayerInput();
	OnChoiceClosed.Broadcast();
	Super::NativeDestruct();
}

FReply UCoreChoiceWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	// 확인 키로 버튼이 실행되지 않고 ESC가 다른 입력 경로로 새지 않도록, 자식에게 전달하기 전에 소비한다.
	const FKey Key = Event.GetKey();
	if (bClosing || Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::Escape) return FReply::Handled();
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UCoreChoiceWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (bClosing) return FReply::Handled();
	const FKey Key = Event.GetKey();
	// 나머지 연산(%)으로 처음과 마지막 카드 사이를 순환한다.
	if (Key == EKeys::Left || (Key == EKeys::Tab && Event.IsShiftDown()))
	{
		MoveActiveCard(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::Tab)
	{
		MoveActiveCard(1);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}
