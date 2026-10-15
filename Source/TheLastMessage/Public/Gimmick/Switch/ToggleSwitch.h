// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "ToggleSwitch.generated.h"

class UStaticMeshComponent;

/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AToggleSwitch : public AGimmickBase
{
	GENERATED_BODY()
	
public:
	AToggleSwitch();

	// 현재 활성 상태를 반전
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Switch")
	void ToggleSwitch();

	// 스위치를 꺼진 상태로 초기화
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Switch")
	void ResetSwitch();

	// 부모의 실제 상호작용 함수를 재정의
	virtual void PerformInteraction(ACharacter* Player) override;

	// 그룹의 C++ 코드에서 현재 상태를 읽는 보조 함수
	bool IsSwitchActive() const { return bIsActive; }

protected:
	// 스위치 외형
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UStaticMeshComponent> SwitchMesh;

};
