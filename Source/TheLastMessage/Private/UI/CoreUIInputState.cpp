#include "UI/CoreUIInputState.h"

#include "Blueprint/UserWidget.h"
#include "Character/Player/PlayerBase.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Reply.h"
#include "Widgets/SViewport.h"
#include "UI/FactoryCursor.h"

void FCoreUIInputState::Capture(const APlayerController& Controller)
{
	bShowCursor = Controller.bShowMouseCursor;
	bFlushKeysOnFocusChange = Controller.ShouldFlushKeysWhenViewportFocusChanges();
#if UE_ENABLE_DEBUG_DRAWING
	InputModeDebugName = Controller.GetCurrentInputModeDebugString();
#endif
	FocusedWidget.Reset();
	if (const ULocalPlayer* LocalPlayer = Controller.GetLocalPlayer())
	{
		// 같은 프레임에 요청한 포커스는 아직 Slate 사용자에게 반영되지 않았을 수 있다.
		FocusedWidget = LocalPlayer->GetSlateOperations().GetUserFocusRecepient();
		if (!FocusedWidget.IsValid())
		{
			if (TSharedPtr<const FSlateUser> User = LocalPlayer->GetSlateUser()) FocusedWidget = User->GetFocusedWidget();
		}
	}
	if (UGameViewportClient* Viewport = Controller.GetWorld()->GetGameViewport())
	{
		CaptureMode = Viewport->GetMouseCaptureMode();
		LockMode = Viewport->GetMouseLockMode();
		bIgnoreInput = Viewport->IgnoreInput();
		bHideCursorDuringCapture = Viewport->HideCursorDuringCapture();
	}
}

void FCoreUIInputState::Restore(APlayerController& Controller) const
{
	Controller.SetInputMode(*this);
	Controller.SetShowMouseCursor(bShowCursor);
	Controller.FlushPressedKeys();
	// 이전 포커스가 뷰포트였거나 사라졌다면 클릭 없이 바로 게임 커서가 갱신되도록 한다.
	const TSharedPtr<SWidget> Previous = FocusedWidget.Pin();
	UGameViewportClient* Viewport = Controller.GetWorld() ? Controller.GetWorld()->GetGameViewport() : nullptr;
	if (Controller.IsLocalController() && Viewport && (!Previous.IsValid() || Previous == Viewport->GetGameViewportWidget()))
	{
		FactoryCursor::FocusGameViewport(Viewport);
	}
}

void FCoreUIInputState::EnterUIOnly(APlayerController& Controller, UUserWidget& Screen)
{
	// 키 초기화만으로는 UI가 가져간 release 이벤트를 기다리는 공격이 끝나지 않는다.
	if (APlayerBase* Pawn = Cast<APlayerBase>(Controller.GetPawn())) Pawn->CancelWeaponInputsForUI();
	// UIOnly는 입력을 UI에만 전달한다. TakeWidget은 이 UMG 위젯의 내부 Slate 위젯을 가져온다.
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Screen.TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller.SetInputMode(InputMode);
	Controller.SetShowMouseCursor(true);
	// 화면을 열기 전에 누르고 있던 이동·공격 키 상태가 남지 않도록 비운다.
	Controller.FlushPressedKeys();
	Screen.SetKeyboardFocus();
}

void FCoreUIInputState::ApplyInputMode(FReply& SlateOperations, UGameViewportClient& Viewport) const
{
	const TSharedPtr<SViewport> ViewportWidget = Viewport.GetGameViewportWidget();
	if (ViewportWidget.IsValid())
	{
		TSharedPtr<SWidget> Focus = FocusedWidget.Pin();
		if (!Focus.IsValid()) Focus = ViewportWidget;
		const bool bLockMouse = LockMode == EMouseLockMode::LockAlways || LockMode == EMouseLockMode::LockOnCapture
			|| (LockMode == EMouseLockMode::LockInFullscreen && Viewport.IsExclusiveFullscreenViewport());
		SetFocusAndLocking(SlateOperations, Focus, bLockMouse, ViewportWidget.ToSharedRef());
		if (CaptureMode == EMouseCaptureMode::CapturePermanently
			|| CaptureMode == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown)
		{
			SlateOperations.UseHighPrecisionMouseMovement(ViewportWidget.ToSharedRef());
		}
		else SlateOperations.ReleaseMouseCapture();
	}
	// 캡처 방식과 포커스 변경 시 키 초기화 정책은 독립된 설정이므로 각각 복원한다.
	Viewport.SetMouseCaptureMode(CaptureMode);
	Viewport.SetMouseLockMode(LockMode);
	Viewport.SetIgnoreInput(bIgnoreInput);
	Viewport.SetHideCursorDuringCapture(bHideCursorDuringCapture);
}
