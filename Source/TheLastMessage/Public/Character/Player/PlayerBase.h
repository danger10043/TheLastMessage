#pragma once

#include "CoreMinimal.h"
#include "Character/Player/SideScrollingCharacter.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "PlayerBase.generated.h"

class UAbilitySystemComponent;
class UCombatAttributeSet;
class UInteractionComponent;
class UCoreComponent;
class UGameplayAbility;
class UGameplayEffect;
class UWeaponBase;
class UWeaponDataAsset;
class APlayerBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDiedSignature, APlayerBase*, Player);

/** Player GAS foundation. Movement remains in ASideScrollingCharacter. */
UCLASS()
class THELASTMESSAGE_API APlayerBase : public ASideScrollingCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APlayerBase();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Player|GAS")
	UCombatAttributeSet* GetCombatAttributes() const { return CombatAttributes; }

	UFUNCTION(BlueprintPure, Category = "Player|Weapon")
	UWeaponBase* GetPrimaryWeapon() const { return PrimaryWeapon; }

	/** Safe to call again after possession; startup grants are applied once. */
	UFUNCTION(BlueprintCallable, Category="Player|GAS")
	void InitializePlayer();

	/** Also callable by lethal gimmicks, independently of ordinary damage. */
	UFUNCTION(BlueprintCallable, Category="Player|Death")
	void HandleDeath();

	UFUNCTION(BlueprintPure, Category="Player|Death")
	bool IsDead() const;

	UPROPERTY(BlueprintAssignable, Category="Player|Events")
	FOnPlayerDiedSignature OnPlayerDied;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Aim")
	FVector AimDirection = FVector::ForwardVector;

	virtual void DoMove(float Forward) override;
	virtual void DoDrop(float Value) override;
	virtual void DoJumpStart() override;
	virtual void DoInteract() override;

	UCoreComponent* GetCoreComponent() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	/** Future carry/push-pull cleanup must finish before OnPlayerDied broadcasts. */
	virtual void CleanupPlayerActions();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Player|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|GAS")
	TObjectPtr<UCombatAttributeSet> CombatAttributes;

	// 시작할 때 장착할 근접 무기의 설정 데이터.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Weapon")
	TObjectPtr<UWeaponDataAsset> InitialPrimaryWeaponData;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Player|Weapon")
	TObjectPtr<UWeaponBase> PrimaryWeapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Interaction")
	TObjectPtr<UInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Core")
	TObjectPtr<UCoreComponent> CoreComponent;

	UPROPERTY(EditDefaultsOnly, Category="Player|GAS")
	TSubclassOf<UGameplayEffect> InitialCombatStatsEffect;

	UPROPERTY(EditDefaultsOnly, Category="Player|GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Reserved for the slot recharge step; never used as an attack cooldown. */
	FTimerHandle EnergyRechargeTimerHandle;

private:
	void HandleDoubleJumpTagChanged(const FGameplayTag Tag, int32 NewCount);
	void UpdateJumpLimit();
	FDelegateHandle DoubleJumpTagChangedHandle;
	bool bStartupGranted = false;
	/** Reentrancy guard for cleanup/event delivery; gameplay state lives on ASC. */
	bool bDeathHandled = false;
};
