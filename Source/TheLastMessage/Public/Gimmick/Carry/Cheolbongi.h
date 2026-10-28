// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/Carry/CarryableGimmick.h"
#include "Cheolbongi.generated.h"

/**
 * 
 */
 // 플레이어가 들고 다닐 수 있는 철붕이
UCLASS(Blueprintable)
class THELASTMESSAGE_API ACheolbongi : public ACarryableGimmick
{
	GENERATED_BODY()

public:
    virtual bool UsesWideCarryPose() const override { return true; }
	
};
