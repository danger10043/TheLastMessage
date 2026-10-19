#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponChargeWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class THELASTMESSAGE_API UWeaponChargeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Player UI|Charge") void SetChargeProgress(float Ratio);
	UPROPERTY(BlueprintReadOnly, Category="Player UI|Charge") float ChargeRatio = 0;
	UPROPERTY(BlueprintReadOnly, Category="Player UI|Charge") bool bFullyCharged = false;
protected:
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UProgressBar> ChargeBar;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UTextBlock> ChargeText;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI|Charge") FLinearColor ChargingColor = FLinearColor(0.18f, 0.65f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI|Charge") FLinearColor ReadyColor = FLinearColor(0.2f, 1.0f, 0.55f);
	UFUNCTION(BlueprintImplementableEvent, Category="Player UI|Charge") void OnChargeUpdated();
};
