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
class UStaticMesh;
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
	// 원거리 발사 시 에너지 섬광을 사용한다, 끄면 AttackEffect의 Niagara 이펙트를 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|MuzzleFlash", meta=(ToolTip="원거리 발사 시 에너지 섬광을 사용한다, 끄면 AttackEffect의 Niagara 이펙트를 사용한다"))
	bool bUseEnergyFlash = true;
	// 에너지 총구 섬광에 사용할 발광 머티리얼
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|MuzzleFlash", meta=(ToolTip="에너지 총구 섬광에 사용할 발광 머티리얼"))
	TSoftObjectPtr<UMaterialInterface> EnergyFlashMaterial;
	// 총구 섬광에 사용할 메시를 무기 데이터에서 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|MuzzleFlash")
	TObjectPtr<UStaticMesh> EnergyFlashMesh = nullptr;
	// 에너지 섬광의 기본 크기, 중형 원거리 무기에는 추가 크기 배율이 적용된다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|MuzzleFlash", meta=(ToolTip="에너지 섬광의 기본 크기, 중형 원거리 무기에는 추가 크기 배율이 적용된다", ClampMin="1"))
	float EnergyFlashSize = 14.0f;
	// 에너지 섬광 머티리얼의 발광 밝기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|MuzzleFlash", meta=(ToolTip="에너지 섬광 머티리얼의 발광 밝기", ClampMin="0"))
	float EnergyFlashBrightness = 12.0f;
	// 무기 종류에 맞는 기본 섬광 색을 사용한다, 끄면 EnergyFlashColor를 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|MuzzleFlash", meta=(ToolTip="무기 종류에 맞는 기본 섬광 색을 사용한다, 끄면 EnergyFlashColor를 사용한다"))
	bool bUseAutomaticEnergyFlashColor = true;
	// 자동 색상 사용을 껐을 때 적용할 에너지 섬광 색
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|MuzzleFlash", meta=(ToolTip="자동 색상 사용을 껐을 때 적용할 에너지 섬광 색"))
	FLinearColor EnergyFlashColor = FLinearColor(0.02f, 0.7f, 1.0f);
	// 로컬 플레이어의 2D 차지 사운드에 적용할 추가 볼륨 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge", meta=(ToolTip="로컬 플레이어의 2D 차지 사운드에 적용할 추가 볼륨 배율", ClampMin="0"))
	float LocalChargeSoundVolumeMultiplier = 2.0f;
	// 조작 중인 플레이어의 차지 소리를 거리 감쇠 없는 2D 사운드로 재생한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge", meta=(ToolTip="조작 중인 플레이어의 차지 소리를 거리 감쇠 없는 2D 사운드로 재생한다"))
	bool bLocalChargeSound2D = true;
	// 총구 기준 섬광의 위치·회전·크기 보정, 로컬 X축은 발사 방향이며 에너지 섬광 크기는 EnergyFlashSize로 조절한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|MuzzleFlash", meta=(ToolTip="총구 기준 섬광의 위치·회전·크기 보정, 로컬 X축은 발사 방향이며 에너지 섬광 크기는 EnergyFlashSize로 조절한다"))
	FTransform MuzzleFlashOffset = FTransform::Identity;
	// 총구 섬광 지속 시간(초), 0이면 Niagara는 자체 수명을 사용하고 에너지 섬광은 기본 0.1초를 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|MuzzleFlash", meta=(ToolTip="총구 섬광 지속 시간(초), 0이면 Niagara는 자체 수명을 사용하고 에너지 섬광은 기본 0.1초를 사용한다", ClampMin="0"))
	float MuzzleFlashDuration = 0.1f;
	// 휘두르기·발사 사운드의 볼륨 배율, 1은 원래 볼륨, 0은 음소거
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Attack", meta=(ToolTip="휘두르기·발사 사운드의 볼륨 배율, 1은 원래 볼륨, 0은 음소거", ClampMin="0"))
	float AttackSoundVolume = 1.0f;
	// 휘두르기·발사 사운드의 피치 배율, 1은 원래 높낮이이며 재생 속도에도 영향을 준다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Attack", meta=(ToolTip="휘두르기·발사 사운드의 피치 배율, 1은 원래 높낮이이며 재생 속도에도 영향을 준다", ClampMin="0.1"))
	float AttackSoundPitch = 1.0f;
	// 차지가 실제로 시작된 뒤 재생할 소리, 짧은 클릭이나 차지 시작 대기 중에는 재생하지 않는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge", meta=(ToolTip="차지가 실제로 시작된 뒤 재생할 소리, 짧은 클릭이나 차지 시작 대기 중에는 재생하지 않는다"))
	TObjectPtr<USoundBase> ChargeSound;
	// 차지 사운드의 기본 볼륨 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge", meta=(ToolTip="차지 사운드의 기본 볼륨 배율", ClampMin="0"))
	float ChargeSoundVolume = 0.5f;
	// 차지 사운드의 기본 피치 배율, 충전 시간 맞춤을 켜면 추가 속도 보정이 적용된다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge", meta=(ToolTip="차지 사운드의 기본 피치 배율, 충전 시간 맞춤을 켜면 추가 속도 보정이 적용된다", ClampMin="0.1"))
	float ChargeSoundPitch = 1.0f;

	// 차지 사운드의 피치를 보정해 충전 완료 시간에 재생 길이를 맞춘다, 보정 범위에 따라 길이가 정확히 일치하지 않을 수 있다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Charge", meta=(ToolTip="차지 사운드의 피치를 보정해 충전 완료 시간에 재생 길이를 맞춘다, 보정 범위에 따라 길이가 정확히 일치하지 않을 수 있다"))
	bool bFitChargeSoundToDuration = true;

	// 공격·차지·타격 사운드에 공통 적용할 거리 감쇠 설정, 로컬 2D 차지 사운드에는 적용되지 않는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|SharedSound", meta=(ToolTip="공격·차지·타격 사운드에 공통 적용할 거리 감쇠 설정, 로컬 2D 차지 사운드에는 적용되지 않는다"))
	TObjectPtr<USoundAttenuation> WeaponSoundAttenuation;

	// 원거리 무기 준비·대기 애니메이션, 비워두면 플레이어에 설정된 무기 종류별 기본값을 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation", meta=(ToolTip="원거리 무기 준비·대기 애니메이션, 비워두면 플레이어에 설정된 무기 종류별 기본값을 사용한다"))
	TObjectPtr<UAnimSequence> RangedIdleAnimation;

	// 무기 외형 설정을 사용하는 투사체에 적용할 머티리얼
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Projectile Visual", meta=(ToolTip="무기 외형 설정을 사용하는 투사체에 적용할 머티리얼"))
	TObjectPtr<UMaterialInterface> ProjectileMaterial;

	// 투사체 머티리얼의 ProjectileColor 파라미터에 전달할 색상
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Projectile Visual", meta=(ToolTip="투사체 머티리얼의 ProjectileColor 파라미터에 전달할 색상"))
	FLinearColor ProjectileColor = FLinearColor(0.1f, 0.8f, 1.0f);

	// 투사체 메시의 외형 크기, 충돌 판정 구체의 크기는 바꾸지 않는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Projectile Visual", meta=(ToolTip="투사체 메시의 외형 크기, 충돌 판정 구체의 크기는 바꾸지 않는다"))
	FVector ProjectileVisualScale = FVector(0.45f, 0.12f, 0.12f);

public:
	// 무기를 구분하는 식별 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="무기를 구분하는 식별 이름"))
	FName WeaponId;

	// 근거리·원거리 및 경량·중량 무기 구분, 공격 방식과 기본 연출 선택에 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="근거리·원거리 및 경량·중량 무기 구분, 공격 방식과 기본 연출 선택에 사용한다"))
	EWeaponType WeaponType = EWeaponType::LightMelee;

	// 공격의 기본 피해량, 차지·능력치·치명타 배율을 적용하기 전 값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="공격의 기본 피해량, 차지·능력치·치명타 배율을 적용하기 전 값"))
	float AttackPower = 1.0f;

	// 무기 슬롯과 선택 UI에 표시할 아이콘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="무기 슬롯과 선택 UI에 표시할 아이콘"))
	TObjectPtr<UTexture2D> WeaponIcon;

	// 공격 시 플레이어가 재생할 몽타주, 근거리는 타격 프레임에 Melee Hit 노티파이가 필요하다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation", meta=(ToolTip="공격 시 플레이어가 재생할 몽타주, 근거리는 타격 프레임에 Melee Hit 노티파이가 필요하다"))
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// 공격 몽타주의 섹션 중 하나를 균등 추첨하고 선택한 섹션만 재생한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation", meta=(ToolTip="공격 몽타주의 섹션 중 하나를 균등 추첨하고 선택한 섹션만 재생한다"))
	bool bRandomizeAttackSection = false;

	// 플레이어가 장착할 무기의 스태틱 메시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh", meta=(ToolTip="플레이어가 장착할 무기의 스태틱 메시"))
	TObjectPtr<UStaticMesh> WeaponMesh;

	// 플레이어 캐릭터 메시에 무기를 부착할 소켓 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh", meta=(ToolTip="플레이어 캐릭터 메시에 무기를 부착할 소켓 이름"))
	FName AttachSocketName;

	// 캐릭터 부착 소켓 기준 무기의 위치·회전·크기 보정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh", meta=(ToolTip="캐릭터 부착 소켓 기준 무기의 위치·회전·크기 보정"))
	FTransform AttachOffset;

	// 근거리는 휘두르기 이펙트, 원거리는 에너지 섬광을 끈 경우의 총구 이펙트, 명중 이펙트는 ImpactEffect에서 설정한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effect|Attack", meta=(ToolTip="근거리는 휘두르기 이펙트, 원거리는 에너지 섬광을 끈 경우의 총구 이펙트, 명중 이펙트는 ImpactEffect에서 설정한다"))
	TObjectPtr<UNiagaraSystem> AttackEffect;

	// 근거리 휘두르기 또는 원거리 발사 시 재생할 소리, 명중 여부와 관계없이 재생한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effect|Attack", meta=(ToolTip="근거리 휘두르기 또는 원거리 발사 시 재생할 소리, 명중 여부와 관계없이 재생한다"))
	TObjectPtr<USoundBase> AttackSound;

	// 무기 명중 지점에서 재생할 Niagara 이펙트, 원거리 탄환은 지형 충돌에도 재생하며 몬스터 자체 피격 연출과 별개다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact", meta=(ToolTip="무기 명중 지점에서 재생할 Niagara 이펙트, 원거리 탄환은 지형 충돌에도 재생하며 몬스터 자체 피격 연출과 별개다"))
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	// 무기 명중·탄환 충돌 지점에서 재생할 타격음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact", meta=(ToolTip="무기 명중·탄환 충돌 지점에서 재생할 타격음"))
	TObjectPtr<USoundBase> ImpactSound;

	// 명중 지점 기준 이펙트의 위치·회전·크기 보정, 로컬 X축은 표면 바깥쪽 법선 방향이다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact", meta=(ToolTip="명중 지점 기준 이펙트의 위치·회전·크기 보정, 로컬 X축은 표면 바깥쪽 법선 방향이다"))
	FTransform ImpactEffectOffset = FTransform::Identity;

	// 타격 사운드의 볼륨 배율, 1은 원래 볼륨, 0은 음소거
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact", meta=(ToolTip="타격 사운드의 볼륨 배율, 1은 원래 볼륨, 0은 음소거", ClampMin="0"))
	float ImpactSoundVolume = 1.0f;

	// 타격 사운드의 피치 배율, 1은 원래 높낮이이며 재생 속도에도 영향을 준다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Impact", meta=(ToolTip="타격 사운드의 피치 배율, 1은 원래 높낮이이며 재생 속도에도 영향을 준다", ClampMin="0.1"))
	float ImpactSoundPitch = 1.0f;

	// 근거리 공격 이펙트·사운드를 붙일 무기 메시의 소켓, 없거나 비어 있으면 무기 메시 원점을 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee", meta=(ToolTip="근거리 공격 이펙트·사운드를 붙일 무기 메시의 소켓, 없거나 비어 있으면 무기 메시 원점을 사용한다"))
	FName MeleeAttackEffectSocketName = NAME_None;

	// 근거리 공격 이펙트의 소켓 기준 위치·회전·크기 보정, 사운드에는 위치 보정을 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee", meta=(ToolTip="근거리 공격 이펙트의 소켓 기준 위치·회전·크기 보정, 사운드에는 위치 보정을 사용한다"))
	FTransform MeleeAttackEffectOffset = FTransform::Identity;

	// 근거리 공격 시작 후 이펙트·사운드·트레일 재생까지의 대기 시간(초), 공격 취소 시 예약도 취소된다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee", meta=(ToolTip="근거리 공격 시작 후 이펙트·사운드·트레일 재생까지의 대기 시간(초), 공격 취소 시 예약도 취소된다", ClampMin="0"))
	float MeleeAttackFeedbackDelay = 0.0f;

	// 근거리 무기의 움직임을 따라 재생할 Niagara 트레일 시스템, 머티리얼이나 텍스처를 직접 넣는 항목이 아니다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee|Trail", meta=(ToolTip="근거리 무기의 움직임을 따라 재생할 Niagara 트레일 시스템, 머티리얼이나 텍스처를 직접 넣는 항목이 아니다"))
	TObjectPtr<UNiagaraSystem> MeleeTrailEffect;
	// 켜면 Melee Trail 노티파이 구간에서만 트레일을 재생하며 지속 시간 타이머 대신 애니메이션 구간을 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee|Trail", meta=(ToolTip="켜면 Melee Trail 노티파이 구간에서만 트레일을 재생하며 지속 시간 타이머 대신 애니메이션 구간을 사용한다"))
	bool bUseMeleeTrailNotifyWindow = false;

	// 트레일을 부착할 무기 메시의 소켓, 없거나 비어 있으면 무기 메시 원점을 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee|Trail", meta=(ToolTip="트레일을 부착할 무기 메시의 소켓, 없거나 비어 있으면 무기 메시 원점을 사용한다"))
	FName MeleeTrailSocketName = NAME_None;

	// 트레일 소켓 기준 위치·회전·크기 보정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee|Trail", meta=(ToolTip="트레일 소켓 기준 위치·회전·크기 보정"))
	FTransform MeleeTrailOffset = FTransform::Identity;

	// 트레일 재생 시작 후 유지할 시간(초), 0이면 공격 종료까지 유지하며 공격이 먼저 끝나거나 취소되면 즉시 제거한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effect|Melee|Trail", meta=(ToolTip="트레일 재생 시작 후 유지할 시간(초), 0이면 공격 종료까지 유지하며 공격이 먼저 끝나거나 취소되면 즉시 제거한다", ClampMin="0"))
	float MeleeTrailDuration = 0.25f;

	// 버튼을 누르고 있다가 놓는 차지 공격을 지원하는지 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="버튼을 누르고 있다가 놓는 차지 공격을 지원하는지 여부"))
	bool bSupportsChargeAttack = false;

	// 차지 시작까지 버튼을 누르고 있어야 하는 시간(초), 이보다 짧게 누르고 놓으면 일반 공격을 실행한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Charge", meta=(ToolTip="차지 시작까지 버튼을 누르고 있어야 하는 시간(초), 이보다 짧게 누르고 놓으면 일반 공격을 실행한다", ClampMin="0", EditCondition="bSupportsChargeAttack"))
	float ChargeStartDelay = 0.2f;

	// 차지가 시작된 뒤 완충까지 걸리는 기본 시간(초), ChargeStartDelay는 제외하며 능력치의 차지 시간 배율을 적용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="차지가 시작된 뒤 완충까지 걸리는 기본 시간(초), ChargeStartDelay는 제외하며 능력치의 차지 시간 배율을 적용한다", ClampMin = "0.01", EditCondition = "bSupportsChargeAttack"))
	float MaxChargeTime = 1.0f;

	// 완충 시 적용할 피해 배율, 충전 비율에 따라 1부터 이 값까지 증가한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="완충 시 적용할 피해 배율, 충전 비율에 따라 1부터 이 값까지 증가한다", ClampMin = "1.0", EditCondition = "bSupportsChargeAttack"))
	float MaxChargeDamageMultiplier = 1.0f;

	// 근거리 타격 검사의 전방 거리(cm), 검사 두께는 공격 능력의 MeleeHitRadius에서 설정한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="근거리 타격 검사의 전방 거리(cm), 검사 두께는 공격 능력의 MeleeHitRadius에서 설정한다"))
	float AttackRange;

	// 원거리 투사체의 기본 최대 이동 거리(cm), 투사체 BP에서 사거리 덮어쓰기를 켜면 BP 설정을 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="원거리 투사체의 기본 최대 이동 거리(cm), 투사체 BP에서 사거리 덮어쓰기를 켜면 BP 설정을 사용한다"))
	float Range;

	// 무기 초기화 시 사용할 기본 최대 에너지 값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="무기 초기화 시 사용할 기본 최대 에너지 값"))
	float MaxEnergy;

	// 공격을 실제 실행할 때 소비할 에너지, 차지 대기 중이나 공격 취소 시에는 소비하지 않는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="공격을 실제 실행할 때 소비할 에너지, 차지 대기 중이나 공격 취소 시에는 소비하지 않는다"))
	float EnergyPerShot;

	// 발사할 투사체 클래스, 비어 있으면 플레이어의 무기 종류별 기본 투사체를 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="발사할 투사체 클래스, 비어 있으면 플레이어의 무기 종류별 기본 투사체를 사용한다"))
	TSubclassOf<AProjectileBase> ProjectileClass;

	// 투사체의 발사 속도(cm/초), 기본 사거리와 함께 최대 비행 시간 계산에 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta=(ToolTip="투사체의 발사 속도(cm/초), 기본 사거리와 함께 최대 비행 시간 계산에 사용한다"))
	float ProjectileSpeed;
};
