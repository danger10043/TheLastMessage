#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PlayerHUD.generated.h"

class APlayerBase;
class UPlayerStatusWidget;
class UWeaponChargeWidget;
class USaveNotificationWidget;

/** The derived HUD Blueprint chooses the widget Blueprints. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API APlayerHUD : public AHUD
{
	GENERATED_BODY()
public:
	APlayerHUD();
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;
	virtual void ShowHUD() override;
	/** Explicit visibility control for cinematics; also applies to widgets created later. */
	UFUNCTION(BlueprintCallable, Category="Player UI")
	void SetPlayerUIVisible(bool bVisible);
	UFUNCTION(BlueprintPure, Category="Player UI") UPlayerStatusWidget* GetStatusWidget() const { return StatusWidget; }
	UFUNCTION(BlueprintPure, Category="Player UI") UWeaponChargeWidget* GetChargeWidget() const { return ChargeWidget; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI") TSubclassOf<UPlayerStatusWidget> StatusWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI") TSubclassOf<UWeaponChargeWidget> ChargeWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI") TSubclassOf<USaveNotificationWidget> SaveNotificationClass;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Player UI") TObjectPtr<USaveNotificationWidget> SaveNotification;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI|Charge") FVector2D ChargeWidgetSize = FVector2D(160, 36);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player UI|Charge", meta=(ClampMin="0")) float ChargeHeightAboveCapsule = 22.0f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Player UI") TObjectPtr<UPlayerStatusWidget> StatusWidget;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Player UI") TObjectPtr<UWeaponChargeWidget> ChargeWidget;
	/** 조준점에서 그래플링 표시까지의 화면 거리 */
	UPROPERTY(EditDefaultsOnly, Category="Player UI|Grappling", meta=(ToolTip="조준점에서 그래플링 표시까지의 화면 거리"))
	FVector2D GrappleIndicatorOffset = FVector2D(44, 44);
	/** 그래플링 원형 표시의 반지름 */
	UPROPERTY(EditDefaultsOnly, Category="Player UI|Grappling", meta=(ClampMin="8", ToolTip="그래플링 원형 표시의 반지름"))
	float GrappleIndicatorRadius = 18.2f;
private:
	UFUNCTION() void CheckpointSaveFinished(bool bSuccess);
	void DrawGrappleIndicator();
	bool bGrappleWasCoolingDown = false;
	float GrappleReadyFlashEnd = 0;
	void EnsureWidgets();
	void UpdatePlayerUI();
	TWeakObjectPtr<APlayerBase> ObservedPlayer;
};
