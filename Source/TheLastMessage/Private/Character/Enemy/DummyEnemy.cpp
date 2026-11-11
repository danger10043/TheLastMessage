#include "Character/Enemy/DummyEnemy.h"
#include "GameFramework/CharacterMovementComponent.h"

ADummyEnemy::ADummyEnemy()
{
	EnemyType = EEnemyType::Dummy;
	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::Disabled;

	StatConfig.MaxHealth = 100.f;
	StatConfig.MoveSpeed = 0.f;
	StatConfig.AttackDamage = 0.f;
	StatConfig.SecondaryAttackDamage = 0.f;
	PatrolRadius = 0.f;
}

void ADummyEnemy::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
}

bool ADummyEnemy::CanAttackTarget() const
{
	return false;
}

void ADummyEnemy::StartAttack()
{
}

void ADummyEnemy::OnPlayerDetected(AActor* Player)
{
}

void ADummyEnemy::ResetEnemy()
{
	Super::ResetEnemy();

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
}