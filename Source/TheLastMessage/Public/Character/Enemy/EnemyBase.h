#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Character/Enemy/DataAsset/EnemyTypes.h"
#include "EnemyBase.generated.h"

class UAbilitySystemComponent;
class UEnemyAttributeSet;
class UGameplayAbility;
class AEnemyBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyStateChangedSignature, EEnemyState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDamagedSignature, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDiedSignature, AEnemyBase*, Enemy);

UCLASS()
class THELASTMESSAGE_API AEnemyBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AEnemyBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 스폰 직후 호출. 여러 번 호출해도 한 번만 초기화된다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void InitializeEnemy();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	EEnemyType GetEnemyType() const { return EnemyType; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	EEnemyState GetEnemyState() const { return CurrentState; }

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void SetEnemyState(EEnemyState NewState);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void SetCurrentTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void OnPlayerDetected(AActor* Player);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void OnPlayerLost();

	/** 플레이어 사망 시 타겟 해제 후 정찰 복귀 */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void OnTargetDied();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetDistanceToTarget() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool CanAttackTarget() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FVector GetHomeLocation() const { return HomeLocation; }

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void HandleDeath();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const { return CurrentState == EEnemyState::Dead; }

	// --- 명세서에 없던 추가 접근자 (AI 컨트롤러·BT 태스크용) ---
	const FEnemyStatConfig& GetStatConfig() const { return StatConfig; }
	float GetPatrolRadius() const { return PatrolRadius; }
	FVector GetPatrolAxis() const { return PatrolAxis.IsNearlyZero() ? FVector::ForwardVector : PatrolAxis.GetSafeNormal(); }

	/** 공격을 시작할 때 호출해 쿨다운을 시작한다. 공격 GA 도입 후 GE 쿨다운으로 대체. */
	void StartAttackCooldown();

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyStateChangedSignature OnEnemyStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyDamagedSignature OnEnemyDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyDiedSignature OnEnemyDied;

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UEnemyAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	EEnemyType EnemyType = EEnemyType::Flea;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	FEnemyStatConfig StatConfig;

	UPROPERTY(EditAnywhere, Category = "Enemy")
	float PatrolRadius = 600.f;

	/** 정찰 이동 축(월드 기준). 사이드뷰 기본은 X축 왕복. 벽·천장 이동형은 (0,0,1)로 설정. */
	UPROPERTY(EditAnywhere, Category = "Enemy")
	FVector PatrolAxis = FVector(1.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Enemy|State")
	EEnemyState CurrentState = EEnemyState::Patrol;

	UPROPERTY(VisibleInstanceOnly, Category = "Enemy|State")
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(VisibleInstanceOnly, Category = "Enemy|State")
	FVector HomeLocation = FVector::ZeroVector;

	bool bInitialized = false;
	double LastAttackTime = -1000000.0;
};
