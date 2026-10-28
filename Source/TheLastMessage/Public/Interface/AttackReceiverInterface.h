// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AttackReceiverInterface.generated.h"

class AActor;

// 공격에 대한 반응을 C++에서 구현하는 공통 인터페이스
UINTERFACE(MinimalAPI, NotBlueprintable)
class UAttackReceiverInterface : public UInterface
{
    GENERATED_BODY()
};

class THELASTMESSAGE_API IAttackReceiverInterface
{
    GENERATED_BODY()

public:
    // 공격이 명중했을 때 호출
    // DamageAmount: 공격 코드에서 계산한 피해량
    // DamageCauser: 공격한 액터
    virtual void ReceiveAttackDamage(float DamageAmount, AActor* DamageCauser) = 0;
};
