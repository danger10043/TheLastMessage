#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayAssetSettings.generated.h"

class ACoreAssistAfterimage;
class UGameplayEffect;
class UStaticMesh;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Gameplay Assets"))
class THELASTMESSAGE_API UGameplayAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="Core")
	TSoftClassPtr<UGameplayEffect> CoreHealEffect;

	UPROPERTY(Config, EditAnywhere, Category="Core")
	TSoftClassPtr<ACoreAssistAfterimage> CoreAssistAfterimage;

	UPROPERTY(Config, EditAnywhere, Category="Gimmicks")
	TSoftObjectPtr<UStaticMesh> DirectionalStairsMesh;
};
