// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "GimmickBase.generated.h"

class ACharacter;
class USceneComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class AGimmickBase;

// 종합 명세서의 이벤트 타입과 매개변수
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnGimmickActiveStateChanged,
	AGimmickBase*, Source,
	bool, bActive
);

UCLASS(Blueprintable)
class THELASTMESSAGE_API AGimmickBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGimmickBase();

	// BPI CanInteract에서 호출
	UFUNCTION(BlueprintPure, Category = "Gimmick|Interaction")
	virtual bool CanInteractInternal(ACharacter* Player) const;

	// BPI Interact에서 호출
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Interaction")
	virtual void PerformInteraction(ACharacter* Player);

	// BPI SetGimmickActive에서 호출
	UFUNCTION(BlueprintCallable, Category = "Gimmick|State")
	virtual void ApplyActiveState(bool bNewActive);

	// BPI SetInteractionHighlight에서 호출
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Interaction")
	virtual void UpdateInteractionHighlight(bool bVisible);

	// 실제 연결 장치의 BPI 호출은 Blueprint에서 구현
	UFUNCTION(BlueprintImplementableEvent,
		Category = "Gimmick|Connections")
	void NotifyLinkedDevices(bool bNewActive);

	// 자식 기믹에서 자신에게 맞는 설정 행을 처리
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Config")
	virtual bool ApplyConfigOverride(FName ConfigSlot, const FDataTableRowHandle& Row);

	UPROPERTY(BlueprintAssignable, Category = "Gimmick|Events")
	FOnGimmickActiveStateChanged OnActiveStateChanged;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	// 액터 위치와 회전의 기준
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// 상호작용 가능 상태의 주변 효과
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UNiagaraComponent> InteractionEffect;

	
	// 스테이지 안에서 구분할 고유 이름
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Config")
	FName GimmickId = NAME_None;


	// 기믹의 기능 사용 가능 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|State")
	bool bIsEnabled = true;

	// 현재 활성 상태
	// BPI GetGimmickActive에서 이 값을 반환
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|State")
	bool bIsActive = false;


	// 플레이어가 버튼으로 직접 조작하는 기믹인지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Interaction")
	bool bSupportsInteraction = false;


	// 활성 상태를 전달할 장치
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Connections")
	TArray<TObjectPtr<AActor>> LinkedDevices;

	
	// 실제 머티리얼의 Scalar Parameter 이름과 일치해야 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Highlight")
	FName GlowParameterName = TEXT("InteractionGlow");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Highlight", meta = (ClampMin = "0.0"))
	float GlowIntensity = 1.0f;

	// 실행 중 생성한 발광용 동적 머티리얼
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Highlight")
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMaterials;
	
	// 현재 강조 상태를 확인하기 위한 보조 변수
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Highlight")
	bool bIsHighlighted = false;

};
