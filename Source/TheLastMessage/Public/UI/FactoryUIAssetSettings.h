#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FactoryUIAssetSettings.generated.h"

class UFont;
class USoundBase;
class UTexture2D;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Factory UI Assets"))
class THELASTMESSAGE_API UFactoryUIAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="Textures")
	TSoftObjectPtr<UTexture2D> MenuBackground;

	UPROPERTY(Config, EditAnywhere, Category="Textures")
	TSoftObjectPtr<UTexture2D> PausePanel;

	UPROPERTY(Config, EditAnywhere, Category="Textures")
	TSoftObjectPtr<UTexture2D> Plate;

	UPROPERTY(Config, EditAnywhere, Category="Textures")
	TSoftObjectPtr<UTexture2D> SelectedPlate;

	UPROPERTY(Config, EditAnywhere, Category="Textures")
	TSoftObjectPtr<UTexture2D> ScreenPlate;

	UPROPERTY(Config, EditAnywhere, Category="Cursors")
	TSoftObjectPtr<UTexture2D> MenuCursor;

	UPROPERTY(Config, EditAnywhere, Category="Cursors")
	TSoftObjectPtr<UTexture2D> HoverCursor;

	UPROPERTY(Config, EditAnywhere, Category="Cursors")
	TSoftObjectPtr<UTexture2D> AimBase;

	UPROPERTY(Config, EditAnywhere, Category="Cursors")
	TSoftObjectPtr<UTexture2D> AimFill;

	UPROPERTY(Config, EditAnywhere, Category="Textures")
	TMap<FName, TSoftObjectPtr<UTexture2D>> MenuIcons;

	UPROPERTY(Config, EditAnywhere, Category="Fonts")
	TSoftObjectPtr<UFont> BodyFont;

	UPROPERTY(Config, EditAnywhere, Category="Fonts")
	TSoftObjectPtr<UFont> TitleFont;

	UPROPERTY(Config, EditAnywhere, Category="Fonts", meta=(ClampMin="12", ClampMax="48"))
	int32 SettingsButtonFontSize = 38;

	/** Mouse-over and keyboard focus moves on menu buttons. */
	UPROPERTY(Config, EditAnywhere, Category="Sounds")
	TSoftObjectPtr<USoundBase> HoverSound;

	/** Buttons that open a page or switch a tab. */
	UPROPERTY(Config, EditAnywhere, Category="Sounds")
	TSoftObjectPtr<USoundBase> ClickSound;

	/** Buttons that commit a decision, such as starting a game or confirming a prompt. */
	UPROPERTY(Config, EditAnywhere, Category="Sounds")
	TSoftObjectPtr<USoundBase> ConfirmSound;

	/** Back, cancel, close, and resume. */
	UPROPERTY(Config, EditAnywhere, Category="Sounds")
	TSoftObjectPtr<USoundBase> CancelSound;
};
