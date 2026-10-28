// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/PushPull/PushPullGimmick.h"
#include "HeavyObject.generated.h"

/**
 * 
 */
 // 밀기·당기기로 이동하며 무게 발판을 누르는 무거운 상자
UCLASS(Blueprintable)
class THELASTMESSAGE_API AHeavyObject : public APushPullGimmick
{
    GENERATED_BODY()
};
