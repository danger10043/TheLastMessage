// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

class AActor;
class UAbilitySystemComponent;
class UGA_InteractGimmick;

UCLASS( Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THELASTMESSAGE_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInteractionComponent();

	// 플레이어가 상호작용 입력을 전달할 때 호출
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Interaction")
	void RequestInteract();

	UFUNCTION(BlueprintCallable, Category = "Gimmick|Interaction")
	void UpdateInteractionTarget();

	UFUNCTION(BlueprintCallable, Category = "Gimmick|Interaction")
	void UpdateInteractionHighlight();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Blueprint에서 BPI의 CanInteract를 호출하고 결과 반환
    UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Interaction")
    bool QueryCanInteract(AActor* Target, AActor* Interactor) const;

    // Blueprint에서 BPI의 SetInteractionHighlight 호출
    UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Interaction")
    void SendHighlightMessage(AActor* Target, bool bVisible);

    // 탐색 범위.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Interaction", meta = (ClampMin = "1.0"))
    float InteractionDistance = 200.0f;

protected:
    // 대상과 플레이어 사이의 장애물 확인에 사용
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Interaction")
    TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_Visibility;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Interaction")
    TWeakObjectPtr<AActor> FocusedActor;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Gimmick|Interaction")
    TWeakObjectPtr<UAbilitySystemComponent> OwnerASC;

    // 다음 단계에서 GA_InteractGimmick Blueprint를 지정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gimmick|Interaction")
    TSubclassOf<UGA_InteractGimmick> InteractionAbilityClass;

private:
    void CacheOwnerASC();

    bool CanUseInteractionAbility() const;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> HighlightedActor;
		
};
