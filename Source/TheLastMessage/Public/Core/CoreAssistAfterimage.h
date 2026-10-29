#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "CoreAssistAfterimage.generated.h"

class APlayerBase;
class UAbilitySystemComponent;
class UCoreComponent;
class UMeleeAttackEventData;
class UAnimMontage;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 잔상의 외형과 공격 모션을 각 화면에서 동일하게 재생하기 위한 정보다. */
USTRUCT()
struct FCoreAfterimageAppearance
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<USkeletalMesh> CharacterMesh = nullptr;
	UPROPERTY() TObjectPtr<UStaticMesh> WeaponMesh = nullptr;
	UPROPERTY() TObjectPtr<UAnimMontage> Montage = nullptr;
	UPROPERTY() FName Section = NAME_None;
	UPROPERTY() FTransform CharacterTransform = FTransform::Identity;
	UPROPERTY() FName WeaponSocket = NAME_None;
	UPROPERTY() FTransform WeaponTransform = FTransform::Identity;
};

/** 본체의 행동 상태와 분리하여 근접 공격 모션을 한 번 재생하는 잔상이다. */
UCLASS()
class THELASTMESSAGE_API ACoreAssistAfterimage : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACoreAssistAfterimage();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 지연 생성 중 호출한다. 코어 보유와 외형을 확인하고 원래 공격 정보를 보관한다. */
	bool Initialize(APlayerBase* Player, const UMeleeAttackEventData* Attack, float Damage, FName CoreId);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** 기존 공격 노티파이가 보내는 타격 알림을 잔상에서 받는다. 플레이어 능력은 부여하지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterimage|Visual")
	TObjectPtr<UMaterialInterface> AfterimageMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterimage|Visual")
	FLinearColor Tint = FLinearColor(0.08f, 0.65f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterimage|Visual", meta = (ClampMin = "0", ClampMax = "1"))
	float Opacity = 0.42f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterimage|Visual", meta = (ClampMin = "0"))
	float Brightness = 1.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterimage|Visual", meta = (ClampMin = "0"))
	float FadeOutDuration = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterimage|Animation", meta = (ClampMin = "0.01"))
	float PlaybackRate = 1.0f;

private:
	void StartPresentation();
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION() void OnRep_Appearance();
	UFUNCTION() void HandlePlayerDied(APlayerBase* Player);
	UFUNCTION() void HandleSourceDestroyed(AActor* Actor);
	UFUNCTION() void HandleCoreChanged(FName CoreId, int32 OldLevel, int32 NewLevel);

	UPROPERTY(ReplicatedUsing = OnRep_Appearance)
	FCoreAfterimageAppearance Appearance;

	/** 원래 공격의 판정 정보와 이미 계산된 추가 피해를 잔상의 수명 동안 유지한다. */
	UPROPERTY(Transient)
	TObjectPtr<UMeleeAttackEventData> AttackRecord;

	UPROPERTY(VisibleInstanceOnly, Category = "Afterimage|Attack")
	float StrikeDamage = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> VisualMaterial;

	TWeakObjectPtr<APlayerBase> SourcePlayer;
	TWeakObjectPtr<UCoreComponent> SourceCore;
	FName SourceCoreId = NAME_None;
	float PresentationEndTime = 0.0f;
	bool bPresentationStarted = false;
};
