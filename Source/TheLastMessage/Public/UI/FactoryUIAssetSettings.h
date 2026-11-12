#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FactoryUIAssetSettings.generated.h"

class UFont;
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
};
