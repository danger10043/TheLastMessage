#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayAssetSettings.generated.h"

class ACoreAssistAfterimage;
class UGameplayEffect;
class USoundBase;
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

	// Initial defaults inherited by player Blueprints. Gameplay reads the Blueprint properties.
	UPROPERTY(Config)
	TSoftObjectPtr<USoundBase> FootstepSound;

	UPROPERTY(Config)
	TArray<float> FootstepStartTimes;

	UPROPERTY(Config)
	float FootstepDuration = 0.36f;

	UPROPERTY(Config)
	TSoftObjectPtr<USoundBase> LandingSound;

	UPROPERTY(Config)
	TArray<TSoftObjectPtr<USoundBase>> GrappleLaunchSounds;

	UPROPERTY(Config)
	TSoftObjectPtr<USoundBase> GrappleAttachSound;

	UPROPERTY(Config)
	TSoftObjectPtr<USoundBase> GrappleReleaseSound;

	UPROPERTY(Config)
	float GrappleLaunchVolume = 0.8f;

	UPROPERTY(Config)
	float GrappleAttachVolume = 1.4f;

	UPROPERTY(Config)
	float GrappleReleaseVolume = 1.5f;
};
