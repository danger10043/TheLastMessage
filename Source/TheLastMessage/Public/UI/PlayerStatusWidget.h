#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "PlayerStatusWidget.generated.h"

class APlayerBase;
class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
class UImage;
class UWeaponDataAsset;

/** Gameplay binding only. Layout belongs to the derived Widget Blueprint. */
UCLASS(Abstract, Blueprintable)
class THELASTMESSAGE_API UPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Player UI")
	void SetObservedPlayer(APlayerBase* Player);
	UFUNCTION(BlueprintCallable, Category="Player UI")
	void RefreshStatus();
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float Health = 0;
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float MaxHealth = 0;
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float PrimaryEnergy = 0;
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float PrimaryMaxEnergy = 0;
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float SecondaryEnergy = 0;
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float SecondaryMaxEnergy = 0;
	UPROPERTY(BlueprintReadOnly, Category="Player UI") bool bPrimaryRanged = false;
	UPROPERTY(BlueprintReadOnly, Category="Player UI") bool bSecondaryRanged = false;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UFUNCTION(BlueprintImplementableEvent, Category="Player UI") void OnStatusUpdated();
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UTextBlock> PrimaryWeaponText;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UTextBlock> SecondaryWeaponText;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UImage> PrimaryWeaponIcon;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UImage> SecondaryWeaponIcon;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UWidget> PrimaryEnergyRow;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UWidget> SecondaryEnergyRow;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UProgressBar> PrimaryEnergyBar;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UProgressBar> SecondaryEnergyBar;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UTextBlock> PrimaryEnergyText;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UTextBlock> SecondaryEnergyText;

private:
	void UnbindPlayer();
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	TWeakObjectPtr<APlayerBase> ObservedPlayer;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeBindings;
};
