// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapon/WeaponTypes.h"
#include "Projectile/ProjectileBase.h"
#include "WeaponDataAsset.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class UMaterialInterface;
class UAnimSequence;
class URangedFireFeedbackData;
/**
 * 
 */
UCLASS(BlueprintType)
class THELASTMESSAGE_API UWeaponDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UWeaponDataAsset();
	// Optional weapon-specific presentation; empty uses the player's pistol/rifle defaults.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect")
	TObjectPtr<URangedFireFeedbackData> RangedFireFeedback;
	// Optional weapon-specific ready/idle loop; otherwise the player uses pistol/rifle defaults.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	TObjectPtr<UAnimSequence> RangedIdleAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Projectile Visual")
	TObjectPtr<UMaterialInterface> ProjectileMaterial;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Projectile Visual")
	FLinearColor ProjectileColor = FLinearColor(0.1f, 0.8f, 1.0f);
	// Visual dimensions only; the hit sphere remains unchanged.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Projectile Visual")
	FVector ProjectileVisualScale = FVector(0.45f, 0.12f, 0.12f);

public:
	// ------------------------------공통------------------------------
	// 무기 ID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName WeaponId;
	
	// 무기 종류
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EWeaponType WeaponType = EWeaponType::LightMelee;

	// 기본 공격력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float AttackPower = 1.0f;

	// UI 아이콘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UTexture2D> WeaponIcon;

	// 플레이어 캐릭터에서 재생할 일반 공격 몽타주.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// 몽타주의 섹션 중 하나를 균등 추첨하고, 해당 섹션만 재생한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	bool bRandomizeAttackSection = false;

	// 무기 외형
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	TObjectPtr<UStaticMesh> WeaponMesh;

	// 캐릭터에 붙일 소켓
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	FName AttachSocketName;

	// 소켓 기준 위치·회전·크기 보정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	FTransform AttachOffset;

	// 공격 연출
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effect")
	TObjectPtr<UNiagaraSystem> AttackEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effect")
	TObjectPtr<USoundBase> AttackSound;

	// 차지 지원 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bSupportsChargeAttack = false;

	// 최대 충전 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.01", EditCondition = "bSupportsChargeAttack"))
	float MaxChargeTime = 1.0f;

	// 충전 공격 시 피해 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1.0", EditCondition = "bSupportsChargeAttack"))
	float MaxChargeDamageMultiplier = 1.0f;

	// ------------------------------근접------------------------------
	// 근접 공격 사거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float AttackRange;

	// ------------------------------원거리-----------------------------
	// 원거리 사거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Range;

	// 일반 공격 소모량
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float EnergyPerShot;

	// 탄환 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AProjectileBase> ProjectileClass;

	// 탄환 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float ProjectileSpeed;
};
