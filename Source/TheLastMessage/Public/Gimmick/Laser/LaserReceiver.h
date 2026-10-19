// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "LaserReceiver.generated.h"

class ALaserEmitter;
class UStaticMeshComponent;
/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ALaserReceiver : public AGimmickBase
{
	GENERATED_BODY()
	
public:
	ALaserReceiver();

	// 발신기가 레이저 접촉의 시작과 종료를 전달
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Laser")
	void SetLaserContact(ALaserEmitter* Emitter, bool bHasContact);

	// 현재 접촉 목록을 기준으로 활성 상태 갱신
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Laser")
	void UpdateReceiverState();

	UFUNCTION(BlueprintPure, Category = "Gimmick|Laser")
	bool HasLaserContactFrom(const ALaserEmitter* Emitter) const;

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	// 레이저를 받는 수신기 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UStaticMeshComponent> ReceiverMesh;

	// 현재 이 수신기에 레이저를 보내는 발신기 목록
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Gimmick|Laser")
	TArray<TWeakObjectPtr<ALaserEmitter>> ActiveEmitters;
};
