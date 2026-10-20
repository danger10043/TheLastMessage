#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RangedFireFeedbackData.generated.h"

class UNiagaraSystem;
class USoundBase;
class USoundAttenuation;
class UMaterialInterface;

/** Editable presentation only; never changes damage, energy or projectile spawning. */
UCLASS(BlueprintType)
class THELASTMESSAGE_API URangedFireFeedbackData : public UDataAsset
{
	GENERATED_BODY()
public:
	// Default energy pulse replaces the old gunpowder Niagara flash.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Energy Flash")
	bool bUseEnergyFlash = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Energy Flash")
	TSoftObjectPtr<UMaterialInterface> EnergyMaterial { FSoftObjectPath(TEXT("/Game/Test_Jihong/Blueprints/Weapon/Effects/M_EnergyMuzzleFlash.M_EnergyMuzzleFlash")) };
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Energy Flash", meta=(ClampMin="1"))
	float EnergySize = 14.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Energy Flash", meta=(ClampMin="0"))
	float EnergyBrightness = 12.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Energy Flash")
	bool bAutomaticEnergyColor = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Energy Flash")
	FLinearColor EnergyColor = FLinearColor(0.02f, 0.7f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge Sound", meta=(ClampMin="0"))
	float LocalChargeVolumeMultiplier = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge Sound")
	bool bLocalChargeSound2D = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Muzzle Flash", meta=(EditCondition="!bUseEnergyFlash", EditConditionHides))
	TObjectPtr<UNiagaraSystem> MuzzleFlash;
	// Local X follows the shot direction. Translation/rotation/scale are relative to the muzzle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Muzzle Flash")
	FTransform MuzzleFlashOffset = FTransform::Identity;
	// Zero lets the Niagara system control its own lifetime.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Muzzle Flash", meta=(ClampMin="0"))
	float FlashDuration = 0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire Sound")
	TObjectPtr<USoundBase> FireSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire Sound", meta=(ClampMin="0"))
	float FireVolume = 0.8f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire Sound", meta=(ClampMin="0.1"))
	float FirePitch = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge Sound")
	TObjectPtr<USoundBase> ChargeSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge Sound", meta=(ClampMin="0"))
	float ChargeVolume = 0.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge Sound", meta=(ClampMin="0.1"))
	float ChargePitch = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge Sound")
	bool bFitChargeSoundToDuration = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;
};
