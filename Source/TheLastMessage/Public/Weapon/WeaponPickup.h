#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon/WeaponTypes.h"
#include "WeaponPickup.generated.h"

class APlayerBase;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;
class UWeaponDataAsset;
class UCorePickupStateSubsystem;
class UPrimitiveComponent;

/**
 * 맵에 배치하여 플레이어에게 무기를 주는 액터다. 플레이어가 닿으면 바로 획득한다.
 * - 어떤 무기인지는 Weapon Data로 정한다. 플레이어 블루프린트의 Weapon Choices 안에 있는 무기여야 한다.
 * - 획득 여부는 코어 픽업과 같은 기록(UCorePickupStateSubsystem)을 쓴다. 체크포인트에 저장되고, 부활하면 그 시점으로 돌아간다.
 * - 획득한 무기는 비어 있는 슬롯(왼쪽 클릭, 오른쪽 클릭 순)에 자동으로 장착된다. 슬롯이 둘 다 차 있으면 보유만 하고, 1~4번 키를 누른 채 클릭해서 바꾼다.
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	AWeaponPickup();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif
	virtual void Tick(float DeltaSeconds) override;

	/** 배치 인스턴스 ID. 맵에 저장되며 PIE에서도 유지된다. */
	UFUNCTION(BlueprintPure, Category = "Weapon Pickup|Persistence")
	FGuid GetPickupId() const { return PickupId; }

	/** 체크포인트를 불러올 때 그 시점의 획득 여부로 되돌린다(획득했으면 숨기고, 아직이면 다시 보이게). */
	void RestoreCheckpointConsumption(bool bWasConsumed);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 주울 무기. 플레이어 블루프린트의 Weapon Choices 안에 있는 무기 데이터여야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Pickup")
	TObjectPtr<UWeaponDataAsset> WeaponData;

	/** 켜면 획득한 무기를 비어 있는 슬롯에 자동으로 장착한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Pickup")
	bool bAutoEquip = true;

	/** 켜면 무기 데이터의 무기 메시를 픽업 외형으로 쓴다. 끄면 Pickup Mesh에 직접 지정한 메시를 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Display")
	bool bUseWeaponMesh = true;

	/** 외형 메시의 크기 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Display")
	FVector DisplayScale = FVector(1.0, 1.0, 1.0);

	/** 위아래로 떠다니는 높이(cm). 0이면 움직이지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Idle", meta = (ClampMin = "0.0"))
	float BobHeight = 12.0f;

	/** 1초에 위아래로 왕복하는 횟수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Idle", meta = (ClampMin = "0.0"))
	float BobSpeed = 0.8f;

	/** 제자리에서 도는 속도(도/초). 0이면 돌지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Idle")
	float SpinSpeed = 45.0f;

	/** 획득할 때 재생할 소리. 비워 두면 재생하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Feedback")
	TObjectPtr<USoundBase> PickupSound;

	/** 획득할 때 재생할 이펙트. 비워 두면 재생하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Feedback")
	TObjectPtr<UNiagaraSystem> PickupEffect;

	/** 획득했을 때 블루프린트에서 연출(알림 문구 등)을 추가할 수 있다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon Pickup", meta = (DisplayName = "On Weapon Picked Up"))
	void BP_OnWeaponPickedUp(APlayerBase* Player, UWeaponDataAsset* Weapon);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Components")
	TObjectPtr<USceneComponent> PickupRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/** 플레이어가 무기를 획득할 수 있는 범위 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Pickup|Components")
	TObjectPtr<USphereComponent> PickupCollision;

private:
	UFUNCTION()
	void OnPickupBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool TryGrant(APlayerBase* Player);
	void MarkConsumed();
	void HideConsumedPickup();
	void ShowPickup();
	void ApplyDisplayMesh();
	void EnsurePickupId();
	UCorePickupStateSubsystem* GetPickupStateSubsystem() const;

	/** 획득한 무기를 비어 있는 슬롯에 장착한다. 공격 중처럼 지금 장착할 수 없으면 잠시 뒤 다시 시도한다. */
	void BeginAutoEquip(APlayerBase* Player, int32 WeaponIndex);
	bool TryAutoEquip();
	void RetryAutoEquip();

	/** 배치 인스턴스 ID. 코어 픽업 기록과 같은 저장소에 이 ID로 획득 여부를 남긴다. */
	UPROPERTY(VisibleAnywhere, Category = "Weapon Pickup|Persistence")
	FGuid PickupId;

	bool bConsumed = false;
	bool bPersistenceInitialized = false;
	float IdleTime = 0.0f;

	FTimerHandle AutoEquipTimer;
	TWeakObjectPtr<APlayerBase> AutoEquipPlayer;
	int32 AutoEquipIndex = INDEX_NONE;
	int32 AutoEquipAttempts = 0;
};
