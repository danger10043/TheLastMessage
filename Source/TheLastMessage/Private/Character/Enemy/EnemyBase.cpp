#include "Character/Enemy/EnemyBase.h"
#include "Character/Enemy/EnemyAIController.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "Character/Enemy/EnemyProjectile.h"
#include "Framework/ActorPoolSubsystem.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInterface.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UEnemyAttributeSet>(TEXT("AttributeSet"));

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

UAbilitySystemComponent* AEnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	DefaultCapsuleCollision = GetCapsuleComponent()->GetCollisionEnabled();

	InitializeEnemy();

	OnEnemyDamaged.AddUniqueDynamic(this, &AEnemyBase::HandleDamagedEffect);
}

void AEnemyBase::InitializeEnemy()
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;

	HomeLocation = GetActorLocation();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	ApplyLevelScaling(false); // MaxHealth, Health(가득), DamageMultiplier
	AttributeSet->InitAttackPower(GetStatConfig().AttackDamage);
	AttributeSet->InitMoveSpeed(GetStatConfig().MoveSpeed);

	GetCharacterMovement()->MaxWalkSpeed = GetStatConfig().MoveSpeed;

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}
}

void AEnemyBase::ApplyLevelScaling(bool bKeepHealthRatio)
{
	const FEnemyStatConfig& Stats = GetStatConfig();
	const float DamageScale = EnemyData ? EnemyData->DamageScalePerLevel : 0.2f;
	const float HealthScale = EnemyData ? EnemyData->HealthScalePerLevel : 0.15f;
	const int32 LevelOffset = FMath::Max(EnemyLevel, 1) - 1;

	// 이미 초기화된 상태에서 레벨이 바뀌면 현재 체력 비율을 유지한다.
	const float HealthRatio = (bKeepHealthRatio && AttributeSet->GetMaxHealth() > 0.f) ? AttributeSet->GetHealthPercent() : 1.f;
	const float NewMaxHealth = Stats.MaxHealth * (1.f + HealthScale * LevelOffset);

	AttributeSet->InitMaxHealth(NewMaxHealth);
	AttributeSet->InitHealth(NewMaxHealth * HealthRatio);
	AttributeSet->InitDamageMultiplier(1.f + DamageScale * LevelOffset);
}

void AEnemyBase::SetEnemyLevel(int32 NewLevel)
{
	EnemyLevel = FMath::Max(NewLevel, 1);
	if (bInitialized)
	{
		ApplyLevelScaling();
	}
}

float AEnemyBase::GetScaledDamage(float BaseDamage) const
{
	const float Multiplier = AttributeSet ? AttributeSet->GetDamageMultiplier() : 1.f;
	return BaseDamage * (Multiplier > 0.f ? Multiplier : 1.f);
}

void AEnemyBase::ApplyEnemyDamage(AActor* Target, float Damage)
{
	if (!Target || Damage <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (DamageEffectClass && TargetASC)
	{
		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddSourceObject(this);

		// GE 레벨에 몬스터 레벨을 넘겨 커브 테이블 방식도 쓸 수 있게 한다.
		const FGameplayEffectSpecHandle SpecHandle =
			AbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, static_cast<float>(FMath::Max(EnemyLevel, 1)), Context);

		if (SpecHandle.IsValid())
		{
			if (DamageSetByCallerTag.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(DamageSetByCallerTag, Damage);
			}

			AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
			return;
		}
	}

	// 예비 경로: GE가 없거나 대상에 ASC가 없으면 엔진 기본 피해를 사용한다.
	UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, nullptr);
}

void AEnemyBase::SetEnemyState(EEnemyState NewState)
{
	if (CurrentState == NewState || IsDead())
	{
		return;
	}

	CurrentState = NewState;
	OnEnemyStateChanged.Broadcast(NewState);
}

void AEnemyBase::SetCurrentTarget(AActor* NewTarget)
{
	CurrentTarget = NewTarget;

	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->UpdateTargetActor(NewTarget);
	}
}

void AEnemyBase::OnPlayerDetected(AActor* Player)
{
	if (IsDead() || bInPool || !Player)
	{
		return;
	}

	SetCurrentTarget(Player);
	SetEnemyState(EEnemyState::Chase);
}

void AEnemyBase::OnPlayerLost()
{
	if (IsDead())
	{
		return;
	}

	CancelAttack();
	SetCurrentTarget(nullptr);
	SetEnemyState(EEnemyState::Patrol);
}

void AEnemyBase::OnTargetDied()
{
	// TODO: 진행 중인 공격(몽타주·타이머·충전·돌진) 취소는 공격 GA 도입 시 여기서 처리
	OnPlayerLost();
}

float AEnemyBase::GetDistanceToTarget() const
{
	return CurrentTarget ? GetDistanceTo(CurrentTarget) : MAX_FLT;
}

bool AEnemyBase::CanAttackTarget() const
{
	if (!CurrentTarget || IsDead() || bIsAttacking || !GetWorld())
	{
		return false;
	}

	const bool bInRange = GetDistanceToTarget() <= GetStatConfig().AttackRange;
	const bool bCooldownDone = (GetWorld()->GetTimeSeconds() - LastAttackTime) >= GetAttackCooldown();
	return bInRange && bCooldownDone;
}

void AEnemyBase::StartAttackCooldown()
{
	if (GetWorld())
	{
		LastAttackTime = GetWorld()->GetTimeSeconds();
	}
}

void AEnemyBase::StartAttack()
{
	if (IsDead() || bIsAttacking)
	{
		return;
	}

	// 기본 구현: 별도 공격 동작이 없는 몬스터는 쿨다운만 소모한다. 서브클래스가 오버라이드한다.
	StartAttackCooldown();
}

void AEnemyBase::CancelAttack()
{
	if (bIsAttacking && AttackMontage)
	{
		StopAnimMontage(AttackMontage);
	}

	bIsAttacking = false;
}

void AEnemyBase::SpawnEffectAtLocation(UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	if (System && GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Location, Rotation, Scale);
	}
}

void AEnemyBase::SpawnEffectAtSocket(UNiagaraSystem* System, FName SocketName,
	const FVector& FallbackLocalOffset, const FRotator& FallbackRotation)
{
	if (!System)
	{
		return;
	}

	if (!SocketName.IsNone() && GetMesh() && GetMesh()->DoesSocketExist(SocketName))
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(System, GetMesh(), SocketName, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		return;
	}

	SpawnEffectAtLocation(System, GetActorTransform().TransformPosition(FallbackLocalOffset), FallbackRotation);
}

FVector AEnemyBase::GetMuzzleLocation(FName SocketName, const FVector& LocalOffset) const
{
	if (!SocketName.IsNone() && GetMesh() && GetMesh()->DoesSocketExist(SocketName))
	{
		return GetMesh()->GetSocketLocation(SocketName);
	}

	return GetActorTransform().TransformPosition(LocalOffset);
}

void AEnemyBase::HandleDamagedEffect(float Damage)
{
	SpawnEffectAtLocation(HitEffect, GetActorLocation());
	StartHitFlash();
	PlayHitReaction();
}

void AEnemyBase::StartHitFlash()
{
	if (!HitFlashMaterial || !GetMesh() || !GetWorld() || bInPool)
	{
		return;
	}

	GetMesh()->SetOverlayMaterial(HitFlashMaterial);

	// 연속 피격 때는 타이머를 다시 시작해 번쩍임을 이어 간다.
	GetWorldTimerManager().SetTimer(HitFlashTimerHandle, this, &AEnemyBase::EndHitFlash, HitFlashDuration, false);
}

void AEnemyBase::EndHitFlash()
{
	if (GetMesh())
	{
		GetMesh()->SetOverlayMaterial(nullptr);
	}
}

void AEnemyBase::PlayHitReaction()
{
	if (!HitMontage || !GetWorld() || IsDead() || bInPool)
	{
		return;
	}

	// 치명타는 사망 몽타주가 재생되므로 피격 몽타주를 재생하지 않는다.
	if (AttributeSet && AttributeSet->GetHealth() <= 0.f)
	{
		return;
	}

	if (bIsAttacking && !bHitReactionInterruptsAttack)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHitReactionTime < HitReactionCooldown)
	{
		return;
	}
	LastHitReactionTime = Now;

	if (bIsAttacking)
	{
		CancelAttack(); // 진행 중인 공격(몽타주, 타이머, 이펙트)을 취소한다.
	}

	PlayAnimMontage(HitMontage);
}

void AEnemyBase::PlayAttackMontage()
{
	if (AttackMontage)
	{
		PlayAnimMontage(AttackMontage);
	}
}

void AEnemyBase::FinishAttack()
{
	bIsAttacking = false;
	StartAttackCooldown();
}

void AEnemyBase::FaceTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.f;
	if (!ToTarget.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));
	}
}

AEnemyProjectile* AEnemyBase::SpawnEnemyProjectile(const FEnemyProjectileConfig& Config, const FVector& MuzzleLocation)
{
	UClass* ProjectileClass = Config.ProjectileClass.Get();
	if (!ProjectileClass || !CurrentTarget || !GetWorld())
	{
		return nullptr;
	}

	const FVector Direction = (CurrentTarget->GetActorLocation() - MuzzleLocation).GetSafeNormal();

	// 투사체는 오브젝트 풀에서 꺼내 쓴다(풀이 없으면 새로 만든다).
	const FTransform SpawnTransform(Direction.Rotation(), MuzzleLocation);
	AEnemyProjectile* Projectile = nullptr;

	if (UActorPoolSubsystem* Pool = GetWorld()->GetSubsystem<UActorPoolSubsystem>())
	{
		Projectile = Cast<AEnemyProjectile>(Pool->AcquireActor(ProjectileClass, SpawnTransform, this, this));
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Projectile = GetWorld()->SpawnActor<AEnemyProjectile>(ProjectileClass, SpawnTransform, Params);
	}

	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->InitializeProjectile(this, GetScaledDamage(Config.Damage));
	Projectile->SetProjectileSpeed(Config.Speed);
	Projectile->SetProjectileLifetime(Config.Lifetime);
	Projectile->LaunchProjectile(Direction);
	return Projectile;
}

void AEnemyBase::HandleDeath()
{
	if (IsDead())
	{
		return;
	}

	SetEnemyState(EEnemyState::Dead);
	CancelAttack();
	SetCurrentTarget(nullptr);

	// 사망 순서: AI 중지 → 어빌리티 취소 → 콜리전 비활성화 → OnEnemyDied
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->StopEnemyAI();
	}

	AbilitySystemComponent->CancelAllAbilities();

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}

	SpawnEffectAtLocation(DeathEffect, GetActorLocation());

	OnEnemyDied.Broadcast(this);

	// 사망 연출이 끝난 뒤 시체를 풀로 돌려보낸다.
	if (CorpseLifeSpan > 0.f)
	{
		GetWorldTimerManager().SetTimer(CorpseTimerHandle, this, &AEnemyBase::ReturnToPool, CorpseLifeSpan, false);
	}
}

void AEnemyBase::ReturnToPool()
{
	if (bInPool)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UActorPoolSubsystem* Pool = World->GetSubsystem<UActorPoolSubsystem>())
		{
			Pool->ReleaseActor(this);
			return;
		}
	}

	Destroy();
}

void AEnemyBase::OnReturnedToPool()
{
	bInPool = true;

	GetWorldTimerManager().ClearAllTimersForObject(this);
	EndHitFlash();
	CancelAttack();

	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->StopEnemyAI();
	}

	StopAnimMontage();
	AbilitySystemComponent->CancelAllAbilities();

	CurrentTarget = nullptr;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 스폰 포인트 등이 참조를 정리하도록 알린 뒤, 이 몬스터에 걸린 외부 바인딩을 모두 끊는다.
	OnEnemyReleased.Broadcast(this);
	OnEnemyDied.Clear();
	OnEnemyReleased.Clear();
}

void AEnemyBase::OnAcquiredFromPool()
{
	bInPool = false;
	ResetEnemy();
}

void AEnemyBase::ResetEnemy()
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	EndHitFlash();
	CancelAttack();
	StopAnimMontage();

	// 상태를 처음으로 되돌린다.
	CurrentTarget = nullptr;
	CurrentState = EEnemyState::Patrol;
	LastAttackTime = -1000000.0;
	LastHitReactionTime = -1000000.0;

	// 사망 처리에서 꺼 둔 충돌과 이동을 복구한다.
	GetCapsuleComponent()->SetCollisionEnabled(DefaultCapsuleCollision);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->SetDefaultMovementMode();

	// 능력과 능력치를 다시 설정한다(능력이 중복 부여되지 않도록 먼저 비운다).
	AbilitySystemComponent->ClearAllAbilities();
	bInitialized = false;
	InitializeEnemy();

	// AI를 처음부터 다시 시작한다.
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->RestartEnemyAI();
	}
}

void AEnemyBase::Destroyed()
{
	// 몬스터가 제거될 때 AI 컨트롤러가 남지 않도록 함께 제거한다.
	if (AController* OwnerController = GetController())
	{
		OwnerController->Destroy();
	}

	Super::Destroyed();
}
