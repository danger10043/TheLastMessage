#pragma once

#include "CoreMinimal.h"
#include "Character/Player/SideScrollingCharacter.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Inputs/AbilityInputBinding.h"
#include "Weapon/WeaponTypes.h"
#include "PlayerBase.generated.h"

class UAbilitySystemComponent;
class UCombatAttributeSet;
class UInteractionComponent;
class UCoreComponent;
class UGameplayAbility;
class UGameplayEffect;
class UWeaponBase;
class UWeaponDataAsset;
class UStaticMeshComponent;
class UAnimMontage;
class APlayerBase;
struct FGameplayEffectContextHandle;

// 사망 여부와 섹션을 함께 복제하여 수신 순서와 관계없이 같은 사망 모션을 재생한다.
USTRUCT()
struct FPlayerDeathPresentationState
{
	GENERATED_BODY()
	UPROPERTY()
	bool bDead = false;
	UPROPERTY()
	FName Section = NAME_None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDiedSignature, APlayerBase*, Player);

/** Player GAS foundation. Movement remains in ASideScrollingCharacter. */
UCLASS()
class THELASTMESSAGE_API APlayerBase : public ASideScrollingCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APlayerBase();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Player|GAS")
	UCombatAttributeSet* GetCombatAttributes() const { return CombatAttributes; }

	UFUNCTION(BlueprintPure, Category = "Player|Weapon")
	UWeaponBase* GetPrimaryWeapon() const { return PrimaryWeapon; }

	UFUNCTION(BlueprintPure, Category = "Player|Weapon")
	UWeaponBase* GetSecondaryWeapon() const { return SecondaryWeapon; }

	// None 슬롯은 nullptr을 반환한다. 두 슬롯은 동시에 장착된다.
	UFUNCTION(BlueprintPure, Category = "Player|Weapon")
	UWeaponBase* GetWeaponInSlot(EWeaponSlot Slot) const;

	// 실제 공격 활성화에 성공한 슬롯의 외형을 표시한다.
	void ShowWeaponInSlot(EWeaponSlot Slot);

	UFUNCTION(BlueprintPure, Category = "Player|Weapon")
	EWeaponSlot GetVisibleWeaponSlot() const { return VisibleWeaponSlot; }

	TSubclassOf<UGameplayEffect> GetWeaponDamageEffectClass() const { return WeaponDamageEffectClass; }

	/** Safe to call again after possession; startup grants are applied once. */
	UFUNCTION(BlueprintCallable, Category="Player|GAS")
	void InitializePlayer();

	/** Also callable by lethal gimmicks, independently of ordinary damage. */
	UFUNCTION(BlueprintCallable, Category="Player|Death")
	void HandleDeath(FName ReactionSection = NAME_None);

	// 체력이 실제로 감소했고 아직 살아 있을 때 AttributeSet에서 호출한다.
	void HandleHitReaction(FName ReactionSection = NAME_None);
	FName GetDamageReactionSection(const FGameplayEffectContextHandle& Context) const;

	UFUNCTION(BlueprintPure, Category = "Player|Combat")
	bool IsHitReacting() const;

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

	// 공격 시작 시 남은 입력을 지운다. 지상에서는 정지하고 공중에서는 기존 속도를 유지한다.
	void StopMovementForAttack();

	UCoreComponent* GetCoreComponent() const;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Animation", meta = (ClampMin = "0.01"))
	float HitReactPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Animation", meta = (ClampMin = "0.01"))
	float DeathPlayRate = 1.0f;

	// 시작할 때 장착할 근접 무기의 설정 데이터.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Weapon")
	TObjectPtr<UWeaponDataAsset> InitialPrimaryWeaponData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Weapon")
	TObjectPtr<UWeaponDataAsset> InitialSecondaryWeaponData;

	// GE_BP_CommonIncomingDamage: Instant, Modifier 없음, IncomingDamageExecution 하나.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Weapon")
	TSubclassOf<UGameplayEffect> WeaponDamageEffectClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Player|Weapon")
	TObjectPtr<UWeaponBase> PrimaryWeapon;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Player|Weapon")
	TObjectPtr<UWeaponBase> SecondaryWeapon;

	// 손 소켓에 부착할 주무기 외형. 공격 판정은 공격 능력에서 처리한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Weapon")
	TObjectPtr<UStaticMeshComponent> PrimaryWeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Weapon")
	TObjectPtr<UStaticMeshComponent> SecondaryWeaponMesh;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_VisibleWeaponSlot, Category = "Player|Weapon")
	EWeaponSlot VisibleWeaponSlot = EWeaponSlot::Primary;

	// 무기별 소켓이 없을 때 사용할 공통 손 소켓. 이것도 없으면 외형을 숨긴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Weapon")
	FName FallbackWeaponSocketName = TEXT("SK_Weapon");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Interaction")
	TObjectPtr<UInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Core")
	TObjectPtr<UCoreComponent> CoreComponent;

	UPROPERTY(EditDefaultsOnly, Category="Player|GAS")
	TSubclassOf<UGameplayEffect> InitialCombatStatsEffect;

	UPROPERTY(EditDefaultsOnly, Category="Player|GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TArray<FAbilityInputBinding> AbilityInputBindings;

	/** Reserved for the slot recharge step; never used as an attack cooldown. */
	FTimerHandle EnergyRechargeTimerHandle;

private:
	void FinishHitReaction();
	void HandleHitMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void PlayDeathPresentation(FName ReactionSection);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastHitReaction(FName ReactionSection);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDeath(FName ReactionSection);
	UFUNCTION()
	void OnRep_DeathHandled();
	FTimerHandle HitReactionTimer;
	bool bDeathPresentationStarted = false;
	// ServerOnly 공격 능력은 눌림/해제를 동일한 서버 경로로 전달한다.
	UFUNCTION(Server, Reliable)
	void ServerAbilityInput(FGameplayTag InputTag, uint8 InputState);
	void AbilityInputPressed(FGameplayTag InputTag);
	void AbilityInputReleased(FGameplayTag InputTag);
	void AbilityInputCanceled(FGameplayTag InputTag);
	void RefreshPrimaryWeaponVisual();
	void RefreshWeaponVisual(const UWeaponDataAsset* Data, UStaticMeshComponent* WeaponMesh, bool bShow);
	UFUNCTION()
	void OnRep_VisibleWeaponSlot();
	void HandleDoubleJumpTagChanged(const FGameplayTag Tag, int32 NewCount);
	void UpdateJumpLimit();
	FDelegateHandle DoubleJumpTagChangedHandle;
	bool bStartupGranted = false;
	/** Reentrancy guard for cleanup/event delivery; gameplay state lives on ASC. */
	UPROPERTY(ReplicatedUsing = OnRep_DeathHandled)
	FPlayerDeathPresentationState DeathState;
};
