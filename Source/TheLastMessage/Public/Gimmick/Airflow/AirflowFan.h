// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "Gimmick/Data/AirflowConfigRow.h"
#include "AirflowFan.generated.h"

class APlayerBase;
class UArrowComponent;
class UBoxComponent;
class UNiagaraComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class URotatingMovementComponent;
class UMaterialInstanceDynamic;
/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AAirflowFan : public AGimmickBase
{
	GENERATED_BODY()
	
public:
	AAirflowFan();

	// 데이터 테이블에서 기류 설정 읽기
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Airflow")
	bool LoadAirflowConfig();

	// 활성 상태에 기류와 효과 연결
	virtual void ApplyActiveState(bool bNewActive) override;

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 환풍기 외형
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UStaticMeshComponent> FanMesh;

	// 플레이어에게 바람을 적용할 범위
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UBoxComponent> AirflowArea;

	// 바람이 향하는 방향을 표시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UArrowComponent> AirflowDirection;

	// 바람을 표현할 나이아가라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UNiagaraComponent> AirflowEffect;

	// 사용할 데이터 테이블과 행
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Config")
	FDataTableRowHandle AirflowConfigRow;

	// 데이터 테이블에서 읽은 설정
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Airflow")
	FAirflowConfigRow AirflowConfig;

	// 게임 시작부터 환풍기를 켤지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Airflow")
	bool bStartActive = true;

	// 현재 기류 범위 안에 있는 플레이어
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Gimmick|Airflow")
	TArray<TWeakObjectPtr<APlayerBase>> AffectedPlayers;

	// 회전하는 날개 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UStaticMeshComponent> RotorMesh;

	// 로터의 회전을 담당
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<URotatingMovementComponent> RotorMovement;

	// FanMesh에서 표시등 재질이 들어 있는 슬롯 번호
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Airflow|Visual", meta = (ClampMin = "0"))
	int32 IndicatorMaterialIndex = 1;

private:
	bool bAirflowConfigLoaded = false;

	// 활성 상태에 따라 효과, 대상 목록, Tick 갱신
	void RefreshAirflowState();

	UFUNCTION()
	void OnAirflowBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnAirflowEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// 로터와 표시등을 실제 작동 상태에 맞춰 갱신
	void UpdateFanVisuals(bool bShouldRun);

	// 표시등 재질을 실행 중에 변경하기 위한 인스턴스
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IndicatorMaterial;
};
