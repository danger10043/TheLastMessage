#include "Character/Enemy/EnemyBase.h"
#include "Character/Enemy/EnemyAIController.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
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
	InitializeEnemy();
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

	AttributeSet->InitMaxHealth(StatConfig.MaxHealth);
	AttributeSet->InitHealth(StatConfig.MaxHealth);
	AttributeSet->InitAttackPower(StatConfig.AttackDamage);
	AttributeSet->InitMoveSpeed(StatConfig.MoveSpeed);

	GetCharacterMovement()->MaxWalkSpeed = StatConfig.MoveSpeed;

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}
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
	if (IsDead() || !Player)
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
	if (!CurrentTarget || IsDead() || !GetWorld())
	{
		return false;
	}

	const bool bInRange = GetDistanceToTarget() <= StatConfig.AttackRange;
	const bool bCooldownDone = (GetWorld()->GetTimeSeconds() - LastAttackTime) >= StatConfig.AttackCooldown;
	return bInRange && bCooldownDone;
}

void AEnemyBase::StartAttackCooldown()
{
	if (GetWorld())
	{
		LastAttackTime = GetWorld()->GetTimeSeconds();
	}
}

void AEnemyBase::HandleDeath()
{
	if (IsDead())
	{
		return;
	}

	SetEnemyState(EEnemyState::Dead);
	SetCurrentTarget(nullptr);

	// 사망 순서: AI 중지 → 어빌리티 취소 → 콜리전 비활성화 → OnEnemyDied
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		AIC->StopEnemyAI();
	}

	AbilitySystemComponent->CancelAllAbilities();

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	OnEnemyDied.Broadcast(this);
}
