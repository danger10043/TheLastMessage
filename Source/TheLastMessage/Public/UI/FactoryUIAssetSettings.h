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

	/** Mouse-over and keyboard focus moves on menu buttons. */
	UPROPERTY(Config, EditAnywhere, Category="Sounds")
	TSoftObjectPtr<USoundBase> HoverSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/_ExternalAssets/MEGA_FULL_BUNDLE/SPACE_GAME_SOUNDS_/CUES/space_beep_Cue.space_beep_Cue")));

	/** Buttons that open a page or switch a tab. */
	UPROPERTY(Config, EditAnywhere, Category="Sounds")
	TSoftObjectPtr<USoundBase> ClickSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/_ExternalAssets/MEGA_FULL_BUNDLE/SPACE_GAME_SOUNDS_/CUES/touch_screen_button_Cue.touch_screen_button_Cue")));

	/** Buttons that commit a decision, such as starting a game or confirming a prompt. */
	UPROPERTY(Config, EditAnywhere, Category="Sounds")
	TSoftObjectPtr<USoundBase> ConfirmSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/_ExternalAssets/MEGA_FULL_BUNDLE/SPACE_GAME_SOUNDS_/CUES/spaceship_button_v7_Cue.spaceship_button_v7_Cue")));

	/** Back, cancel, close, and resume. */
	UPROPERTY(Config, EditAnywhere, Category="Sounds")
	TSoftObjectPtr<USoundBase> CancelSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/_ExternalAssets/MEGA_FULL_BUNDLE/SPACE_GAME_SOUNDS_/CUES/spaceship_button_Cue.spaceship_button_Cue")));
};
