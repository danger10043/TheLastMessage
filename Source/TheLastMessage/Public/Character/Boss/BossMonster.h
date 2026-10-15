#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Character/Boss/BossTypes.h"
#include "BossMonster.generated.h"

class UAbilitySystemComponent;
class UBossAttributeSet;
class UBossPhaseComponent;
class UBossPatternComponent;
class UBossShieldComponent;
class UBossCorePuzzleComponent;
class UGameplayAbility;
class UGameplayEffect;

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

    UPROPERTY(EditDefaultsOnly)
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

    // 초기 스탯 설정용 Instant GE. 명세에 추가하는 설정 멤버다.
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Initialization")
    TSubclassOf<UGameplayEffect> InitialStatsEffect;

private:
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