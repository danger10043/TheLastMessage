#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy/EnemyBase.h"
#include "DummyEnemy.generated.h"

UCLASS()
class THELASTMESSAGE_API ADummyEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ADummyEnemy();

	virtual bool CanAttackTarget() const override;
	virtual void StartAttack() override;
	virtual void OnPlayerDetected(AActor* Player) override;
	virtual void ResetEnemy() override;

protected:
	virtual void BeginPlay() override;
};