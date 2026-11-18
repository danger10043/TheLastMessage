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
	ChargeAttack UMETA(DisplayName = "차징 공격 / 망치"),
	Drop         UMETA(DisplayName = "하향 점프"),
	Interact     UMETA(DisplayName = "상호작용"),
	WeaponEquip  UMETA(DisplayName = "무기 장착"),
	// 기존에 저장된 안내 종류의 숫자 값을 유지하도록 새 종류는 끝에 추가한다.
	RifleChargeAttack UMETA(DisplayName = "차징 공격 / 라이플")
};
