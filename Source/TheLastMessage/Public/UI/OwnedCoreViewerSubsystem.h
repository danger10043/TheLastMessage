#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "InputCoreTypes.h"
#include "UI/CoreUIInputState.h"
#include "OwnedCoreViewerSubsystem.generated.h"

class UInputComponent;
class UOwnedCoreWidget;

/** 로컬 플레이어의 보유 코어 메뉴 입력과 수명을 관리한다. 플레이어·무기 클래스에 UI 코드를 넣지 않는다. */
UCLASS(Config = Game)
class THELASTMESSAGE_API UOwnedCoreViewerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	UOwnedCoreViewerSubsystem();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	UFUNCTION(BlueprintCallable, Category = "Core|UI") bool OpenViewer();
	UFUNCTION(BlueprintCallable, Category = "Core|UI") void CloseViewer();
	UFUNCTION(BlueprintPure, Category = "Core|UI") bool IsViewerOpen() const;

protected:
	UPROPERTY(Config) FKey ToggleKey;
	UPROPERTY(Config) TSoftClassPtr<UOwnedCoreWidget> ViewerClass;

private:
	void ToggleViewer();
	void DetachController();
	void HandleClosed();
	bool CanUnpause() const;
	UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void HandleSourceEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	UPROPERTY(Transient) TObjectPtr<UInputComponent> MenuInput;
	UPROPERTY(Transient) TObjectPtr<UOwnedCoreWidget> Viewer;
	UPROPERTY(Transient) TWeakObjectPtr<APlayerController> InputController;
	UPROPERTY(Transient) TWeakObjectPtr<APawn> SourcePawn;
	UPROPERTY(Transient) TWeakObjectPtr<UWorld> InputWorld;
	bool bOwnsPause = false;
	FCoreUIInputState PreviousInput;
};
