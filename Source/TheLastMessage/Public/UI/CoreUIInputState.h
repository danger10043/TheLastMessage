#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

/** 코어 화면을 열기 전의 입력 설정을 보관하고 같은 설정으로 복원한다. 일시정지의 소유권은 화면이 관리한다. */
struct THELASTMESSAGE_API FCoreUIInputState : public FInputModeDataBase
{
	void Capture(const APlayerController& Controller);
	void Restore(APlayerController& Controller) const;

protected:
	virtual void ApplyInputMode(FReply& SlateOperations, UGameViewportClient& Viewport) const override;
	virtual bool ShouldFlushInputOnViewportFocus() const override { return bFlushKeysOnFocusChange; }
#if UE_ENABLE_DEBUG_DRAWING
	virtual const FString& GetDebugDisplayName() const override { return InputModeDebugName; }
#endif

private:
	TWeakPtr<SWidget> FocusedWidget;
	EMouseCaptureMode CaptureMode = EMouseCaptureMode::NoCapture;
	EMouseLockMode LockMode = EMouseLockMode::DoNotLock;
	bool bShowCursor = false;
	bool bIgnoreInput = false;
	bool bHideCursorDuringCapture = true;
	bool bFlushKeysOnFocusChange = true;
#if UE_ENABLE_DEBUG_DRAWING
	FString InputModeDebugName;
#endif
};
