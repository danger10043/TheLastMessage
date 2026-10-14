// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

class AActor;

// This class does not need to be modified.
UINTERFACE(BlueprintType, Blueprintable)
class THELASTMESSAGE_API UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class THELASTMESSAGE_API IInteractableInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// 액터와 상호 작용이 가능한지
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Gimmick|Interaction")
	bool CanInteract(AActor* Interactor) const;

	// 상호작용 요청
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Gimmick|Interaction")
	void Interact(AActor* Interactor);

	// 상호작용 대상 강조 표시
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent,
		Category = "Gimmick|Interaction")
	void SetInteractionHighlight(bool bEnable);

	// 기믹 활성 상태 설정
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent,
		Category = "Gimmick|State")
	void SetGimmickActive(bool bNewActive);

	// 기믹 활성 상태 조회
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent,
		Category = "Gimmick|State")
	bool GetGimmickActive() const;

};
