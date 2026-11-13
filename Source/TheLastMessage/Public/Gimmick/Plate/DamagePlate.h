// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "Gimmick/Data/DamagePlateConfigRow.h"
#include "TimerManager.h"
#include "DamagePlate.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ADamagePlate : public AGimmickBase
{
	GENERATED_BODY()
	
public:
    ADamagePlate();

    // 데이터 테이블에서 피해 설정 읽기
    UFUNCTION(BlueprintCallable, Category = "Gimmick|DamagePlate")
    bool LoadDamageConfig();

    // 스위치에서 전달받은 활성 상태 적용
    virtual void ApplyActiveState(bool bNewActive) override;

protected:
    virtual void BeginPlay() override;
    // +추가----
    virtual void RefreshGimmickAudioState() override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Audio")
    TObjectPtr<USoundBase> DamagePlateLoopSound;
    // +--------

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 플레이어가 밟을 발판 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> PlateMesh;

    // 피해를 받을 플레이어를 감지하는 범위
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UBoxComponent> DamageTrigger;

    // 사용할 데이터 테이블과 행
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Config")
    FDataTableRowHandle DamageConfigRow;

    // 데이터 테이블에서 읽은 설정
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|DamagePlate")
    FDamagePlateConfigRow DamageConfig;

    // 데미지 발판의 시각 효과
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UNiagaraComponent> DamagePlateEffect;

    // 스위치 없이 사용하는 발판은 시작부터 작동하도록 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|DamagePlate")
    bool bStartActive = true;

private:
    // 범위 안의 플레이어에게 피해 적용
    void ApplyDamageToPlayers();

    // 피해 적용 간격을 관리하는 타이머
    FTimerHandle DamageTimerHandle;

    // 설정을 정상적으로 읽었는지 확인
    bool bDamageConfigLoaded = false;

    // 현재 작동 상태에 맞춰 나이아가라 갱신
    void UpdateDamagePlateEffect();
};
