#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Interface/PoolableInterface.h"
#include "Character/Enemy/DataAsset/EnemyTypes.h"
#include "Character/Enemy/DataAsset/EnemyDataAsset.h"
#include "EnemyBase.generated.h"

class UAbilitySystemComponent;
class UEnemyAttributeSet;
class UGameplayAbility;
class UGameplayEffect;
class UAnimMontage;
class UNiagaraSystem;
class UMaterialInterface;
struct FGameplayEffectSpec;
class UWidgetComponent;
class UEnemyHealthBarWidget;
class APlayerBase;
class AEnemyBase;
class AEnemyProjectile;
class AEnemyHealthPickup;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyStateChangedSignature, EEnemyState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDamagedSignature, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDiedSignature, AEnemyBase*, Enemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyReleasedSignature, AEnemyBase*, Enemy);

UCLASS()
class THELASTMESSAGE_API AEnemyBase : public ACharacter, public IAbilitySystemInterface, public IPoolableInterface
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
	virtual void OnPlayerDetected(AActor* Player);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	virtual void OnPlayerLost();

	/** 플레이어 사망 시 타겟 해제 후 정찰 복귀 */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	virtual void OnTargetDied();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetDistanceToTarget() const;

	/** 지금 공격을 시작할 수 있는지. 몬스터마다 조건이 다르면 오버라이드한다(예: Junker의 돌격 단계). */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	virtual bool CanAttackTarget() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FVector GetHomeLocation() const { return HomeLocation; }

	/** 정찰 기준점을 바꾼다. Blackboard의 HomeLocation도 함께 갱신한다. */
	void SetHomeLocation(const FVector& NewHome);

	/** 착지했을 때 호출된다. 홈과 높이가 많이 다른 곳에 떨어졌으면 그 자리를 새 홈으로 삼는다. */
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	virtual void HandleDeath();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const { return CurrentState == EEnemyState::Dead; }

	// --- 명세서에 없던 추가 접근자 (AI 컨트롤러·BT 태스크용) ---
	/** 데이터 에셋이 지정돼 있으면 그 값을, 없으면 이 클래스의 StatConfig를 사용한다. */
	const FEnemyStatConfig& GetStatConfig() const { return EnemyData ? EnemyData->StatConfig : StatConfig; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Level")
	int32 GetEnemyLevel() const { return EnemyLevel; }

	/** 레벨을 바꾸고 체력·공격력 배율을 다시 계산한다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Level")
	void SetEnemyLevel(int32 NewLevel);

	/** 레벨 배율이 적용된 피해량을 반환한다. 모든 공격은 이 값을 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Level")
	float GetScaledDamage(float BaseDamage) const;

	/**
	 * 대상에게 피해를 준다. Damage에는 GetScaledDamage가 적용된 최종 피해량을 넘긴다.
	 * DamageEffectClass가 있고 대상에 ASC가 있으면 GE를 적용하고, 아니면 엔진 기본 피해로 대신한다.
	 * DamageTypeTag가 지정되면 이 피해에 동적 에셋 태그로 붙인다(예: Damage.Type.Explosion).
	 */
	void ApplyEnemyDamage(AActor* Target, float Damage, FGameplayTag DamageTypeTag = FGameplayTag());

	/**
	 * 받는 피해를 몬스터 특성에 맞게 조정한다(예: 플레이어 원거리 피해 감소).
	 * AttributeSet이 체력을 깎기 전에 호출한다. 서브클래스가 오버라이드할 수 있다.
	 */
	virtual float ModifyIncomingDamage(float Damage, const FGameplayEffectSpec& Spec) const;
	float GetPatrolRadius() const { return PatrolRadius; }
	FVector GetPatrolAxis() const { return PatrolAxis.IsNearlyZero() ? FVector::ForwardVector : PatrolAxis.GetSafeNormal(); }

	/** 공격을 시작할 때 호출해 쿨다운을 시작한다. 공격 GA 도입 후 GE 쿨다운으로 대체. */
	void StartAttackCooldown();

	/** BT 공격 태스크가 호출한다. 서브클래스가 오버라이드해 실제 공격을 시작한다. */
	virtual void StartAttack();

	/** 진행 중인 공격(타이머·충전·돌진 등)을 취소한다. 서브클래스가 확장한다. */
	virtual void CancelAttack();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsAttacking() const { return bIsAttacking; }

	// --- 오브젝트 풀 ---
	virtual void OnAcquiredFromPool() override;
	virtual void OnReturnedToPool() override;

	/** 몬스터를 처음 상태로 되돌린다(체력·상태·AI·충돌). 풀에서 꺼낼 때 호출된다. */
	virtual void ResetEnemy();

	/** 풀로 돌려보낸다. 풀이 없으면 제거한다. */
	void ReturnToPool();

	/** 풀에 보관 중(비활성)인지 */
	UFUNCTION(BlueprintPure, Category = "Enemy|Pool")
	bool IsPooled() const { return bInPool; }

	/** 풀로 돌아갈 때 호출된다(스폰 포인트가 참조를 정리하는 데 사용). */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyReleasedSignature OnEnemyReleased;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyStateChangedSignature OnEnemyStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyDamagedSignature OnEnemyDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyDiedSignature OnEnemyDied;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	/**
	 * 데이터 에셋의 수치를 이 몬스터의 멤버에 복사한다. 몬스터 생성 직후(BeginPlay 전)에 한 번 호출된다.
	 * 서브클래스는 Super를 먼저 부르고 데이터 에셋의 자기 몬스터 항목을 복사한다. Data가 없으면 코드 기본값을 그대로 쓴다.
	 */
	virtual void ApplyEnemyData(const UEnemyDataAsset* Data);

	/** 공격 쿨다운(초). 서브클래스가 다른 값을 쓰려면 오버라이드한다. */
	virtual float GetAttackCooldown() const { return GetStatConfig().AttackCooldown; }

	/** 월드 위치에 나이아가라 이펙트를 한 번 재생한다. System이 비어 있으면 아무것도 하지 않는다. */
	void SpawnEffectAtLocation(UNiagaraSystem* System, const FVector& Location,
		const FRotator& Rotation = FRotator::ZeroRotator, const FVector& Scale = FVector::OneVector);

	/**
	 * 스켈레탈 메시 소켓에 나이아가라 이펙트를 붙여 한 번 재생한다(애니메이션을 따라 움직인다).
	 * 소켓이 없으면 몬스터 기준 로컬 오프셋 위치에서 FallbackRotation으로 재생한다.
	 */
	void SpawnEffectAtSocket(UNiagaraSystem* System, FName SocketName,
		const FVector& FallbackLocalOffset, const FRotator& FallbackRotation);

	/** 총구 위치: 소켓(본)이 있으면 그 위치, 없으면 몬스터 기준 로컬 오프셋 */
	FVector GetMuzzleLocation(FName SocketName, const FVector& LocalOffset) const;

	/** 피해를 받았을 때 피격 이펙트를 재생한다. */
	UFUNCTION()
	void HandleDamagedEffect(float Damage);

	/** 현재 타겟이 플레이어일 때, 그 플레이어의 사망과 부활 이벤트를 받도록 연결한다. */
	void BindTargetPlayerEvents(AActor* Target);

	/** 연결해 둔 플레이어 이벤트를 모두 해제한다. */
	void UnbindTargetPlayerEvents();

	/** 타겟 플레이어가 죽으면 타겟을 해제하고 정찰로 복귀한다. */
	UFUNCTION()
	void HandleTargetPlayerDied(APlayerBase* Player);

	/** 죽었던 플레이어가 부활했을 때, 아직 시야 안에 있으면 다시 추격한다. */
	UFUNCTION()
	void HandleTargetPlayerRespawned(APlayerBase* Player);

	/** 머리 위 체력바를 갱신한다. bInstant가 true면 지연 바도 즉시 맞춘다. */
	void UpdateHealthBar(bool bInstant);

	/** 체력바를 보여 준다. 항상 표시 모드가 아니면 HealthBarVisibleTime 뒤에 다시 숨긴다. */
	void ShowHealthBar();
	void HideHealthBar();

	UEnemyHealthBarWidget* GetHealthBarWidget() const;

	/** 피격 몽타주를 재생한다(조건을 만족할 때만). */
	void PlayHitReaction();

	/** 메시에 번쩍임 오버레이를 씌우고, HitFlashDuration 뒤에 제거한다. */
	void StartHitFlash();
	void EndHitFlash();

	/** 공격 몽타주를 재생한다(지정돼 있을 때만). 공격 클래스가 공격을 시작할 때 호출한다. */
	void PlayAttackMontage();

	/** 공격이 끝났을 때 호출: 공격 중 해제 + 쿨다운 시작 */
	void FinishAttack();

	/** 타겟 방향(수평)으로 몸을 돌린다 */
	void FaceTarget();

	/** 총구에서 현재 타겟까지의 조준 방향(단위 벡터). 타겟이 없으면 ZeroVector. */
	FVector ComputeAimDirection(const FVector& MuzzleLocation) const;

	/**
	 * 투사체 1발을 생성해 발사한다.
	 * FixedDirection이 ZeroVector면 지금 타겟 방향으로 쏘고, 값이 있으면 그 방향으로 쏜다(연발 조준 고정용).
	 */
	AEnemyProjectile* SpawnEnemyProjectile(const FEnemyProjectileConfig& Config, const FVector& MuzzleLocation, const FVector& FixedDirection = FVector::ZeroVector);

	bool bIsAttacking = false;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UEnemyAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	EEnemyType EnemyType = EEnemyType::Flea;

	/** 몬스터 설정 데이터. 이 몬스터의 모든 수치(능력치, 공격 수치, 타이밍 등)가 들어 있다. 몬스터마다 에셋을 하나씩 만들고, 에셋 안에서 자기 몬스터 항목을 채운다. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	TObjectPtr<UEnemyDataAsset> EnemyData;

	/** 데이터 에셋이 없을 때 쓰는 기본 능력치(생성자의 코드 기본값). 수치는 데이터 에셋에서 편집한다. */
	FEnemyStatConfig StatConfig;

	/** 공격 시작 시 재생할 몽타주 (애니메이션 블루프린트에 DefaultSlot이 있어야 한다) */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 사망 시 재생할 몽타주. 마지막 자세를 유지하려면 몽타주의 Enable Auto Blend Out을 끈다. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** 피해를 받을 때 재생할 피격 몽타주(비워 두면 재생하지 않음). 몽타주는 DefaultSlot을 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Animation")
	TObjectPtr<UAnimMontage> HitMontage;

	/** 머리 위 체력바. 블루프린트에서 Widget Class에 WBP_EnemyHealthBar를 지정하고 위치를 조정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|UI")
	TObjectPtr<UWidgetComponent> HealthBarComponent;

	/** 켜면 체력바를 항상 표시하고, 끄면 맞은 뒤 HealthBarVisibleTime 동안만 표시한다. */
	bool bAlwaysShowHealthBar = false;

	/** 맞은 뒤 체력바가 유지되는 시간(초) */
	float HealthBarVisibleTime = 3.f;

	/** 피격 몽타주를 다시 재생할 수 있기까지의 최소 간격(초). 연속 피격 때 계속 끊기는 것을 막는다. */
	float HitReactionCooldown = 0.4f;

	/** 켜면 공격 중에 맞아도 피격 몽타주가 재생되고 공격이 취소된다. 끄면 공격 중에는 피격 몽타주를 재생하지 않는다. */
	bool bHitReactionInterruptsAttack = false;

	/**
	 * 피격 시 몬스터 메시 위에 잠깐 덧씌울 오버레이 머티리얼(번쩍임). 비워 두면 번쩍이지 않는다.
	 * 몬스터마다 머티리얼을 고칠 필요 없이 하나를 공용으로 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Effects")
	TObjectPtr<UMaterialInterface> HitFlashMaterial;

	/** 번쩍임이 유지되는 시간(초) */
	float HitFlashDuration = 0.08f;

	/** 피해를 받을 때 재생할 이펙트 */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Effects")
	TObjectPtr<UNiagaraSystem> HitEffect;

	/** 사망 후 시체가 남아 있는 시간(초). 이 시간이 지나면 몬스터가 풀로 돌아간다. 0 이하이면 계속 남긴다. */
	float CorpseLifeSpan = 5.f;

	/** 사망할 때 재생할 이펙트 */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Effects")
	TObjectPtr<UNiagaraSystem> DeathEffect;

	/**
	 * 착지한 높이가 홈과 이 값보다 많이 다르면(낭떠러지에서 떨어진 경우 등) 착지한 자리를 새 홈으로 삼는다.
	 * 다른 층에 떨어진 몬스터가 원래 홈으로 돌아가려다 막히는 것을 막는다.
	 */
	float HomeReanchorHeight = 150.f;

	/** 플레이어에게 피해를 줄 때 적용할 GameplayEffect (IncomingDamage를 올리는 GE) */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * DamageEffectClass의 규모가 Set By Caller일 때 피해량을 전달할 태그 (예: Data.Damage).
	 * 비워 두면 피해량을 전달하지 않고 GE에 설정된 값을 그대로 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Damage")
	FGameplayTag DamageSetByCallerTag;

	/**
	 * 플레이어 원거리 피해(Damage.Category.Player.Ranged)에 곱할 배율. 1이면 그대로, 0.5면 절반으로 줄어든다.
	 * 근접 피해에는 적용되지 않는다.
	 */
	float RangedDamageMultiplier = 1.f;

	/** 몬스터 레벨. 후반 지역일수록 높게 배치해 체력·공격력을 키운다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Level", meta = (ClampMin = "1", ExposeOnSpawn = "true"))
	int32 EnemyLevel = 1;

	float PatrolRadius = 600.f;

	/** 정찰 이동 축(월드 기준). 사이드뷰 기본은 X축 왕복. 벽·천장 이동형은 (0,0,1)로 설정. */
	FVector PatrolAxis = FVector(1.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** 죽었을 때 일정 확률로 떨어뜨릴 체력 회복 아이템 블루프린트. 비워 두면 아무것도 떨어뜨리지 않는다. 확률·회복량은 데이터 에셋에서 정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Drop")
	TSubclassOf<AEnemyHealthPickup> HealDropClass;

	/** 체력 회복 아이템을 떨어뜨릴 확률(0~1) */
	float HealDropChance = 0.f;

	/** 아이템 하나가 회복시키는 체력 */
	float HealDropAmount = 20.f;

	/** 바닥에서 아이템이 떠 있는 높이(cm). 죽은 자리 아래의 바닥을 찾아 그 위에 놓는다. */
	float HealDropHeight = 40.f;

	/** false면 죽어도 아이템을 떨어뜨리지 않는다(예: 신더가 스스로 자폭할 때). 풀에서 재사용될 때 true로 돌아간다. */
	bool bCanDropOnDeath = true;

	/** 확률에 따라 체력 회복 아이템을 죽은 자리에 떨어뜨린다(서버). */
	void TryDropHeal();

private:
	/** 레벨에 맞춰 MaxHealth·Health·DamageMultiplier Attribute를 설정한다. */
	/** bKeepHealthRatio가 true면 현재 체력 비율을 유지하고, false면 체력을 가득 채운다. */
	void ApplyLevelScaling(bool bKeepHealthRatio = true);

	UPROPERTY(VisibleInstanceOnly, Category = "Enemy|State")
	EEnemyState CurrentState = EEnemyState::Patrol;

	UPROPERTY(VisibleInstanceOnly, Category = "Enemy|State")
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(VisibleInstanceOnly, Category = "Enemy|State")
	FVector HomeLocation = FVector::ZeroVector;

	bool bInitialized = false;
	double LastAttackTime = -1000000.0;

	/** 마지막으로 피격 몽타주를 재생한 시각 */
	double LastHitReactionTime = -1000000.0;

	FTimerHandle HitFlashTimerHandle;

	FTimerHandle HealthBarTimerHandle;

	/** 사망·부활 이벤트를 연결해 둔 플레이어 */
	TWeakObjectPtr<APlayerBase> BoundPlayer;
	TWeakObjectPtr<APlayerBase> DeadPlayer;

	/** 풀에 보관 중인지 */
	bool bInPool = false;

	/** 사망으로 꺼 두는 캡슐 충돌을 풀에서 다시 꺼낼 때 복구하기 위한 원래 값 */
	ECollisionEnabled::Type DefaultCapsuleCollision = ECollisionEnabled::QueryAndPhysics;

	FTimerHandle CorpseTimerHandle;
};
