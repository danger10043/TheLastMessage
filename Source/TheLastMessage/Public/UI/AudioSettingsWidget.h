#pragma once

#include "Blueprint/UserWidget.h"
#include "AudioSettingsWidget.generated.h"

enum class EGameVolume : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAudioSettingsClosed);

UCLASS(Blueprintable)
class THELASTMESSAGE_API UAudioSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static void BuildDefaultLayout(UWidgetTree* Tree);
	static UAudioSettingsWidget* CreateFor(APlayerController* Player);
	void FocusFirstControl();
	void SetPauseInputAction(class UInputAction* Action) { PauseInputAction = Action; }
	FAudioSettingsClosed OnClosed;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	UPROPERTY(Transient) TObjectPtr<class UInputAction> PauseInputAction;
	UFUNCTION() void Close();
	UFUNCTION() void SetMasterVolume(float Value);
	UFUNCTION() void SetEffectsVolume(float Value);
	UFUNCTION() void SetInterfaceVolume(float Value);
	UFUNCTION() void SetMusicVolume(float Value);
	UFUNCTION() void SetAmbienceVolume(float Value);
	void ChangeVolume(EGameVolume Channel, float Value);
	void RefreshVolumeValues();
};
