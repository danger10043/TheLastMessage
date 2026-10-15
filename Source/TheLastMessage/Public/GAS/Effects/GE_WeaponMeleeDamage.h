#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GE_WeaponMeleeDamage.generated.h"

/** 첫 근접 무기의 기본 피해 경로. 치명타·조건부 배율은 별도 확장한다. */
UCLASS()
class THELASTMESSAGE_API UWeaponMeleeDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Parameters,
		FGameplayEffectCustomExecutionOutput& Output) const override;
};

UCLASS()
class THELASTMESSAGE_API UGE_WeaponMeleeDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_WeaponMeleeDamage();
};
