#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "CoreDataAsset.generated.h"

class UTexture2D;

/** Definition data for a core. Ownership and granted handles belong to UCoreComponent. */
UCLASS(BlueprintType)
class THELASTMESSAGE_API UCoreDataAsset : public UDataAsset
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Grants")
	TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core|Grants")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;
};
