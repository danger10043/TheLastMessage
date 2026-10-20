// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_InteractGimmick.generated.h"

class AActor;
/**
 * 
 */
UCLASS(blueprintable)
class THELASTMESSAGE_API UGA_InteractGimmick : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
    UGA_InteractGimmick();

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    // Blueprint에서 CanInteract를 재확인한 뒤
    // 대상에게 BPI의 Interact 메시지를 전달
    UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Interaction")
    void SendInteractionMessage(AActor* Target, AActor* Interactor);

    // 이번 요청에서 상호작용할 대상
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Gimmick|Interaction")
    TWeakObjectPtr<AActor> TargetActor;
};
