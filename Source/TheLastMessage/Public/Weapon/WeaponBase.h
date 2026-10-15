// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WeaponBase.generated.h"

class UWeaponDataAsset;
/**
 * 
 */
UCLASS(BlueprintType)
class THELASTMESSAGE_API UWeaponBase : public UObject
{
	GENERATED_BODY()
	
public:
	UWeaponBase();

	// 생성한 무기 객체에 설정 데이터를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool Initialize(UWeaponDataAsset* InWeaponData);

public:
	// 무기 설정 데이터
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponDataAsset> WeaponData;

	// 마지막 발사 성공 시점
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	double LastSuccessfulFireTime = 0.0;

	// 장착 해제 시 보존할 에너지
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	float StoredEnergyOnUnequip = 0.0f;

};
