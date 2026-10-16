// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GravityStateComponent.generated.h"

class UAbilitySystemComponent;
class UCharacterMovementComponent;
class UGameplayEffect;
class USkeletalMeshComponent;

UCLASS( ClassGroup=(Gimmick), meta=(BlueprintSpawnableComponent) )
class THELASTMESSAGE_API UGravityStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UGravityStateComponent();

	// 현재 중력 상태 반전
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Gravity")
	void ToggleGravityState();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 반전 태그가 추가되거나 제거되면 호출
	void HandleGravityTagChanged(const FGameplayTag Tag, int32 NewCount);

	// 현재 태그에 맞는 중력 방향 적용
	void ApplyGravityFromTag();

	// 캐릭터의 위쪽 방향을 중력 반대 방향으로 정렬
	void AlignCharacterToGravity();

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> OwnerASC;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Gravity")
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Gravity")
	FGameplayTag ReversedGravityTag;

	// 무한 지속이며 반전 태그를 부여하는 Gameplay Effect
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Gravity")
	TSubclassOf<UGameplayEffect> GravityEffectClass;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle ActiveGravityEffectHandle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Gravity")
	FVector DefaultGravityDirection = FVector(0.0f, 0.0f, -1.0f);

	// 메시가 뒤집히는 데 걸리는 시간. 0이면 즉시 회전
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Gravity", meta = (ClampMin = "0.0", Units = "s"))
	float GravityRotationDuration = 0.7f;

	FDelegateHandle GravityTagChangedHandle;

private:
	// 메시 위치와 회전에 보정값 적용
	void ApplyMeshRotationOffset(const FQuat& RotationOffset);

	// 회전을 끝내고 메시를 기본 상대 위치와 회전으로 복구
	void FinishMeshRotation();

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;

	// 플레이 시작 시 메시의 기본 상대 위치와 회전
	FVector InitialMeshRelativeLocation = FVector::ZeroVector;
	FQuat InitialMeshRelativeRotation = FQuat::Identity;

	// 회전 시작 시점과 현재 시점의 메시 회전 보정값
	FQuat MeshRotationStartOffset = FQuat::Identity;
	FQuat CurrentMeshRotationOffset = FQuat::Identity;

	float MeshRotationElapsed = 0.0f;
	bool bIsMeshRotating = false;

};
