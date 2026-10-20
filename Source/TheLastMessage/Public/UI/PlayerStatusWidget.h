#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "PlayerStatusWidget.generated.h"

class APlayerBase;
class UAbilitySystemComponent;
class UPlayerResourceBarWidget;
class UPlayerWeaponSlotWidget;


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
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float DisplayEnergy = 0;
	UPROPERTY(BlueprintReadOnly, Category="Player UI") float DisplayMaxEnergy = 0;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UPlayerResourceBarWidget> HealthResource;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UPlayerResourceBarWidget> EnergyResource;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UPlayerWeaponSlotWidget> WeaponSlot1;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UPlayerWeaponSlotWidget> WeaponSlot2;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UPlayerWeaponSlotWidget> WeaponSlot3;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UPlayerWeaponSlotWidget> WeaponSlot4;
	UFUNCTION(BlueprintImplementableEvent, Category="Player UI") void OnStatusUpdated();

private:
	void RefreshLoadout();
	void RefreshEnergyDisplay();
	void UnbindPlayer();
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	TWeakObjectPtr<APlayerBase> ObservedPlayer;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeBindings;
};
