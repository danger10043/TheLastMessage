#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PlayerHUD.generated.h"

class APlayerBase;
class UPlayerStatusWidget;
class UWeaponChargeWidget;

/** The derived HUD Blueprint chooses the widget Blueprints. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API APlayerHUD : public AHUD
{
	GENERATED_BODY()
public:
	APlayerHUD();
	virtual void Tick(float DeltaSeconds) override;
	virtual void ShowHUD() override;
	UFUNCTION(BlueprintPure, Category="Player UI") UPlayerStatusWidget* GetStatusWidget() const { return StatusWidget; }
	UFUNCTION(BlueprintPure, Category="Player UI") UWeaponChargeWidget* GetChargeWidget() const { return ChargeWidget; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI") TSubclassOf<UPlayerStatusWidget> StatusWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI") TSubclassOf<UWeaponChargeWidget> ChargeWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI|Charge") FVector2D ChargeWidgetSize = FVector2D(160, 36);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI|Charge", meta=(ClampMin="0")) float ChargeHeightAboveCapsule = 22.0f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Player UI") TObjectPtr<UPlayerStatusWidget> StatusWidget;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Player UI") TObjectPtr<UWeaponChargeWidget> ChargeWidget;
private:
	void EnsureWidgets();
	void UpdatePlayerUI();
	TWeakObjectPtr<APlayerBase> ObservedPlayer;
};
