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
    virtual UAbilitySystemComponent*
        GetAbilitySystemComponent() const override;

    void InitializeBoss();
    EBossPhase GetBossPhase();
    EBossState GetBossState();
    void SetBossState(EBossState NewState);
    AActor* GetCurrentTarget();
    void SetCurrentTarget(AActor* NewTarget);
    void StartShieldPuzzle(EBossPuzzleType PuzzleType);
    void CompleteShieldPuzzle();
    void FailShieldPuzzle();
    void StartPhaseCinematic(EBossPhase NextPhase);
    void FinishPhaseCinematic();
    void SetRangedImmunity(bool bEnabled);
    bool IsRangedImmune();
    void HandleBossDeath();

    // 명세 6.12의 UI 중계 함수.
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

private:
    UPROPERTY()
    EBossState CurrentState = EBossState::Idle;

    UPROPERTY()
    bool bIsRangedImmune = false;

    UPROPERTY()
    TObjectPtr<AActor> CurrentTarget = nullptr;
};