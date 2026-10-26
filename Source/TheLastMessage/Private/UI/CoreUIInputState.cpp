#include "UI/CoreUIInputState.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Reply.h"
#include "Widgets/SViewport.h"

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
