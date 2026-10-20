#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Character/Boss/BossTypes.h"
#include "Character/Boss/BossUIWidget.h"
#include "BossMonster.generated.h"

class UAbilitySystemComponent;
class UBossAttributeSet;
class UBossPhaseComponent;
class UBossPatternComponent;
class UBossShieldComponent;
class UBossCorePuzzleComponent;
class UGameplayAbility;
class UGameplayEffect;
class UStaticMeshComponent;
class ABossActivationCore;
class UAnimInstance;
class UAnimMontage;
class ALaserEmitter;
class ALaserReceiver;
class ALaserReflector;
class UMaterialInterface;
struct FOnAttributeChangeData;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnBossPhaseChangedSignature, EBossPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnBossStateChangedSignature, EBossState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnBossPuzzleStartedSignature, EBossPuzzleType, PuzzleType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnBossPuzzleCompletedSignature, EBossPuzzleType, PuzzleType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnBossPuzzleFailedSignature, EBossPuzzleType, PuzzleType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossDiedSignature);

UCLASS()
class THELASTMESSAGE_API ABossMonster
    : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ABossMonster();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    void InitializeBoss();

    UFUNCTION(BlueprintPure, Category = "Boss|Phase")
    EBossPhase GetBossPhase();

    FBossMissileConfig GetMissileConfig(EBossPhase Phase);

    EBossState GetBossState();

    void SetBossState(EBossState NewState);

    // 현재 페이즈의 미완료 전환 임계값을 반환한다.
    float GetDamageHealthFloorRatio() const;

    void CheckBossPhaseThreshold();

    AActor* GetCurrentTarget();

    bool IsDistanceInAttackRange(EBossAttackType AttackType, float TargetDistance) const;

    UFUNCTION(BlueprintPure, Category = "Boss|Attack")
    float GetAttackCooldownRemaining(EBossAttackType AttackType) const;

    bool TryStartAttackCooldown(EBossAttackType AttackType);

    void SetCurrentTarget(AActor* NewTarget);

    void StartShieldPuzzle(EBossPuzzleType PuzzleType);

    void StopLaserPuzzle();

    UFUNCTION(BlueprintCallable, Category = "Boss|Puzzle")
    void CompleteShieldPuzzle();

    UFUNCTION(BlueprintCallable, Category = "Boss|Puzzle")
    void FailShieldPuzzle();

    void StartPhaseCinematic(EBossPhase NextPhase);

    UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
    void FinishPhaseCinematic();

    void SetRangedImmunity(bool bEnabled);

    bool IsRangedImmune();

    UFUNCTION(BlueprintCallable, Category = "Boss|Lifecycle")
    void HandleBossDeath();

    // 명세 6.12의 UI 중계 함수.
    UFUNCTION(BlueprintPure, Category = "Boss|Puzzle")
    float GetRemainingPuzzleTime() const;

    int32 GetRemainingCoreCount() const;

    UPROPERTY(BlueprintAssignable)
    FOnBossPhaseChangedSignature OnBossPhaseChanged;

    UPROPERTY(BlueprintAssignable)
    FOnBossStateChangedSignature OnBossStateChanged;

    UPROPERTY(BlueprintAssignable)
    FOnBossPuzzleStartedSignature OnBossPuzzleStarted;

    UPROPERTY(BlueprintAssignable)
    FOnBossPuzzleCompletedSignature OnBossPuzzleCompleted;

    UPROPERTY(BlueprintAssignable)
    FOnBossPuzzleFailedSignature OnBossPuzzleFailed;

    UPROPERTY(BlueprintAssignable)
    FOnBossDiedSignature OnBossDied;

protected:
    virtual void BeginPlay() override;

    virtual void PostInitializeComponents() override;

    bool CanStartLaserPuzzle() const;

    bool StartLaserPuzzle();

    void CheckLaserPuzzleCompletion();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss|Puzzle|Laser")
    TArray<TObjectPtr<ALaserEmitter>> LaserEmitters;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss|Puzzle|Laser")
    TArray<TObjectPtr<ALaserReceiver>> LaserReceivers;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss|Puzzle|Laser")
    TArray<TObjectPtr<ALaserReflector>> LaserReflectors;

    bool bLaserPuzzleActive = false;

    virtual void Tick(float DeltaSeconds) override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void UpdatePuzzlePresentation();

    void ResumeCombatAfterPuzzleBlend();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Puzzle|Presentation")
    TObjectPtr<UStaticMeshComponent> PuzzleShieldMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Puzzle|Presentation")
    TObjectPtr<UAnimMontage> PuzzleGroggyMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Puzzle|Presentation")
    FName PuzzleLoopSection = TEXT("Loop");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Puzzle|Presentation")
    float PuzzleShieldYawSpeed = 20.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Puzzle|Presentation",
        meta = (ClampMin = "0.0"))
    float PuzzleGroggyBlendOutTime = 0.8f;

    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActivePuzzleMontage;

    TWeakObjectPtr<UAnimInstance> PuzzleAnimInstance;
    bool bPuzzlePresentationActive = false;
    bool bPuzzleExitPending = false;
    int32 PuzzleMontageInstanceID = INDEX_NONE;

    void UpdateEquipmentForPhase(EBossPhase Phase);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Equipment")
    TObjectPtr<UStaticMeshComponent> GunMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Equipment")
    TObjectPtr<UStaticMeshComponent> ShieldMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBossAttributeSet> AttributeSet;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBossPhaseComponent> PhaseComponent;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBossPatternComponent> PatternComponent;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBossShieldComponent> ShieldComponent;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBossCorePuzzleComponent> CorePuzzleComponent;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss|Puzzle|Cores")
    TArray<TObjectPtr<ABossActivationCore>> ActivationCores;

    UPROPERTY(EditDefaultsOnly)
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

    // 초기 스탯 설정용 Instant GE. 명세에 추가하는 설정 멤버다.
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Initialization")
    TSubclassOf<UGameplayEffect> InitialStatsEffect;

private:
    void HandleBossHealthChanged(const FOnAttributeChangeData& Data);

    void StartHitFlash();

    void EndHitFlash();

    UPROPERTY(EditDefaultsOnly, Category = "Boss|HitEffect")
    TObjectPtr<UNiagaraSystem> HitEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|HitFlash")
    TObjectPtr<UMaterialInterface> HitFlashMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|HitFlash",
        meta = (ClampMin = "0.01"))
    float HitFlashDuration = 0.08f;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> PreviousHitOverlay;

    FDelegateHandle HealthChangedDelegateHandle;
    FTimerHandle HitFlashTimerHandle;
    bool bHitFlashActive = false;

    void UpdateBossUI();
    void RemoveBossUI();

    UPROPERTY(EditDefaultsOnly, Category = "Boss|UI")
    TSubclassOf<UBossUIWidget> BossUIWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UBossUIWidget> BossUIWidget;

    // 실행 중인 보스 인스턴스에서만 사용하는 초기화 상태다.
    UPROPERTY(Transient)
    bool bInitialStatsApplied = false;

    UPROPERTY(Transient)
    bool bInitializationInProgress = false;

    UPROPERTY()
    EBossState CurrentState = EBossState::Idle;

    UPROPERTY()
    bool bIsRangedImmune = false;

    UPROPERTY()
    TObjectPtr<AActor> CurrentTarget = nullptr;
};