// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "GravityFlipPlate.generated.h"

class ACharacter;
class UBoxComponent;
class UPrimitiveComponent;
class UNiagaraComponent;
struct FHitResult;
/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AGravityFlipPlate : public AGimmickBase
{
	GENERATED_BODY()

public:
	AGravityFlipPlate();

	// 플레이어가 새로 진입하면 중력 반전 요청
	UFUNCTION(BlueprintCallable, Category = "Gimmick|GravityPlate")
	void HandlePlateEnter(ACharacter* Player);

	// 이탈한 플레이어를 진입 목록에서 제거
	UFUNCTION(BlueprintCallable, Category = "Gimmick|GravityPlate")
	void HandlePlateExit(ACharacter* Player);

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UBoxComponent> PlateTrigger;

	// 중력 반전 발판의 시각 효과
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UNiagaraComponent> GravityEffect;

	// 현재 발판 안에 있는 플레이어 목록
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Gimmick|GravityPlate")
	TArray<TWeakObjectPtr<ACharacter>> OverlappingPlayers;

private:
	UFUNCTION()
	void OnPlateBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnPlateEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
	
};
