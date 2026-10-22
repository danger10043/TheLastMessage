// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/Carry/CarryableGimmick.h"
#include "FuseItem.generated.h"

/**
 * 
 */
 // 플레이어가 운반하고 퓨즈 회로에 삽입할 수 있는 아이템
UCLASS(Blueprintable)
class THELASTMESSAGE_API AFuseItem : public ACarryableGimmick
{
    GENERATED_BODY()
};
