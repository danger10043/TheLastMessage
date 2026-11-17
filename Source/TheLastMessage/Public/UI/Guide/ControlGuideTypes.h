#pragma once

#include "CoreMinimal.h"
#include "ControlGuideTypes.generated.h"

/** 안내판에서 표시할 조작법의 종류 */
UENUM(BlueprintType)
enum class EControlGuideType : uint8
{
	Move         UMETA(DisplayName = "이동"),
	Jump         UMETA(DisplayName = "점프"),
	Map          UMETA(DisplayName = "지도"),
	Core         UMETA(DisplayName = "코어 확인"),
	PushPull     UMETA(DisplayName = "밀기 / 당기기"),
	ChargeAttack UMETA(DisplayName = "차징 공격")
};