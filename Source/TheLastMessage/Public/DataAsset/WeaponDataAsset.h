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

class USoundAttenuation;
/**
 * 
 */
UCLASS(BlueprintType)
class THELASTMESSAGE_API UWeaponDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UWeaponDataAsset();
	// Default energy pulse replaces the old gunpowder Niagara flash.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Ranged")
	bool bUseEnergyFlash = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Ranged")
	TSoftObjectPtr<UMaterialInterface> EnergyMaterial { FSoftObjectPath(TEXT("/Game/Test_Jihong/Blueprints/Weapon/Effects/M_EnergyMuzzleFlash.M_EnergyMuzzleFlash")) };
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Ranged", meta=(ClampMin="1"))
	float EnergySize = 14.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Ranged", meta=(ClampMin="0"))
	float EnergyBrightness = 12.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Ranged")
	bool bAutomaticEnergyColor = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Ranged")
	FLinearColor EnergyColor = FLinearColor(0.02f, 0.7f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge", meta=(ClampMin="0"))
	float LocalChargeVolumeMultiplier = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge")
	bool bLocalChargeSound2D = true;
	// Local X follows the shot direction. Translation/rotation/scale are relative to the muzzle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Ranged")
	FTransform MuzzleFlashOffset = FTransform::Identity;
	// Zero lets the Niagara system control its own lifetime.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Ranged", meta=(ClampMin="0"))
	float FlashDuration = 0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Sound", meta=(ClampMin="0"))
	float AttackVolume = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Sound", meta=(ClampMin="0.1"))
	float AttackPitch = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge")
	TObjectPtr<USoundBase> ChargeSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge", meta=(ClampMin="0"))
	float ChargeVolume = 0.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge", meta=(ClampMin="0.1"))
	float ChargePitch = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge")
	bool bFitChargeSoundToDuration = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

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
	// Contact feedback, separate from the swing/muzzle and the victim's damage reaction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact")
	TObjectPtr<UNiagaraSystem> ImpactEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact")
	TObjectPtr<USoundBase> ImpactSound;
	// Local X points along the surface normal, away from the hit surface.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact")
	FTransform ImpactEffectOffset = FTransform::Identity;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact", meta=(ClampMin="0"))
	float ImpactVolume = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact", meta=(ClampMin="0.1"))
	float ImpactPitch = 1.0f;

	// Melee swing presentation is attached to the equipped weapon, relative to this socket.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee")
	FName MeleeEffectSocket = NAME_None;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee")
	FTransform MeleeEffectOffset = FTransform::Identity;
	// Seconds after the swing starts. Canceling the attack also cancels pending feedback.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee", meta=(ClampMin="0"))
	float MeleeFeedbackDelay = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee")
	TObjectPtr<UNiagaraSystem> MeleeTrail;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee")
	FName MeleeTrailSocket = NAME_None;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee")
	FTransform MeleeTrailOffset = FTransform::Identity;
	// Zero keeps the trail until the attack ends; otherwise stop after this many seconds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee", meta=(ClampMin="0"))
	float MeleeTrailDuration = 0.25f;

	// 차지 지원 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bSupportsChargeAttack = false;
	// Hold this long before charge audio, HUD and bonus damage begin. Short clicks release a normal attack.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Charge", meta=(ClampMin="0", EditCondition="bSupportsChargeAttack"))
	float ChargeStartDelay = 0.2f;

	// 차지 시작 이후 최대 충전까지 걸리는 시간 (ChargeStartDelay 제외).
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

	// 기본 최대 에너지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float MaxEnergy;

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
