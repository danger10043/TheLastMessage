#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "CoreDataAsset.generated.h"

class UTexture2D;

/** 코어의 정의 데이터. Asset Manager에 등록되며, 실제 보유 상태와 부여 기록은 UCoreComponent에서 관리한다. */
UCLASS(BlueprintType)
class THELASTMESSAGE_API UCoreDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core")
	FName CoreId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core")
	FText CoreName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core", meta = (MultiLine = "true"))
	FText CoreDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UTexture2D> CoreIcon = nullptr;

	/** 선택형 보상의 전체 후보에 포함할지 정한다. 고정 보상 전용 코어는 끈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Reward")
	bool bIncludeInRandomPool = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Grants")
	TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Grants")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;
};
