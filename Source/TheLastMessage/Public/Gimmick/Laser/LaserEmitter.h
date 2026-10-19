
#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "Gimmick/Data/LaserConfigRow.h"
#include "Gimmick/Laser/LaserSegment.h"
#include "LaserEmitter.generated.h"

class ACharacter;
class ALaserReceiver;
class USceneComponent;
class UNiagaraComponent;

// 레이저 경로, 수신기 접촉, 시각 효과는 이후 구현한다.
UCLASS(Blueprintable)
class THELASTMESSAGE_API ALaserEmitter : public AGimmickBase
{
    GENERATED_BODY()

public:
	ALaserEmitter();

	// 데이터 테이블에서 레이저 설정 읽기
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Laser")
	bool LoadLaserConfig();

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 발사 및 표면 반사 경로 계산
	void UpdateLaserPath();

	// 레이저를 받는 수신기에 접촉 상태 전달
	void UpdateReceiverContacts();

	// 계산한 레이저 경로 표시
	void UpdateLaserVisuals();

	// 레이저 한 구간의 충돌 판정
	bool TraceLaserSegment(const FVector& Start, const FVector& Direction, float Distance, FHitResult& OutHit) const;

	// 일반 피해 계산을 거치지 않고 플레이어 사망 처리
	void KillPlayerImmediately(ACharacter* Player);

	// 레이저 발사 위치와 방향
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<USceneComponent> LaserOrigin;

	// 게임 시작 시 자동으로 레이저를 켤지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser")
	bool bStartActive = true;

	// 레이저가 충돌을 검사할 트레이스 채널
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser")
	TEnumAsByte<ECollisionChannel> LaserTraceChannel = ECC_Visibility;

	// 사용할 데이터 테이블과 행
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser")
	FDataTableRowHandle LaserConfigRow;

	// 데이터 테이블에서 읽은 설정
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Laser")
	FLaserConfigRow LaserConfig;

	// 이번 프레임에 계산한 레이저 구간
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Laser")
	TArray<FLaserSegment> LaserSegments;

	// 레이저 구간별로 사용하는 Niagara 컴포넌트
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Laser")
	TArray<TObjectPtr<UNiagaraComponent>> LaserEffectComponents;

	// 이전 프레임에 레이저를 받던 수신기
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ALaserReceiver>> PreviousReceivers;

private:
	// 이번 프레임에 레이저를 받는 수신기
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ALaserReceiver>> CurrentReceivers;

	bool bHasValidLaserConfig = false;
};