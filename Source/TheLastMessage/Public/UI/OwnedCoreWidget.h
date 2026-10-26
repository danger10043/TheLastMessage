#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/GameCoreTypes.h"
#include "GameplayEffectTypes.h"
#include "InputCoreTypes.h"
#include "OwnedCoreWidget.generated.h"

class UAbilitySystemComponent;
class UButton;
class UCoreComponent;
class UImage;
class UOwnedCoreItemWidget;
class UScrollBox;
class UTextBlock;
class UWrapBox;
class UWidgetSwitcher;

/** 보유 데이터의 조회와 선택 표시를 담당한다. 입력과 일시정지는 로컬 플레이어 서브시스템이 관리한다. */
UCLASS(Abstract)
class THELASTMESSAGE_API UOwnedCoreWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	bool SetSources(UCoreComponent* Core, UAbilitySystemComponent* ASC, FKey InToggleKey);
	/** 화면 표시 실패나 종료 시 획득·스탯 알림 구독을 해제한다. 여러 번 호출해도 안전하다. */
	void ReleaseSources();
	FSimpleMulticastDelegate OnClosed;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	UPROPERTY(EditDefaultsOnly, Category = "Core|UI") TSubclassOf<UOwnedCoreItemWidget> ItemWidgetClass;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWrapBox> AbilityCoresBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWrapBox> StatCoresBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> AbilityTab;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> StatTab;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> CategoryPages;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> AbilityTabIndicator;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> StatTabIndicator;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> CoreDetails;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> DetailIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CoreName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CoreLevel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CurrentEffect;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NextEffect;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UScrollBox> EffectScroll;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UScrollBox> NextScroll;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CloseButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MaxHealthValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CriticalChanceValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MaxEnergyValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MoveSpeedValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DamageDealtValue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DamageTakenValue;

private:
	UFUNCTION() void Close();
	UFUNCTION() void HandleCoreChanged(FName CoreId, int32 OldLevel, int32 NewLevel);
	UFUNCTION() void ShowAbilityCores();
	UFUNCTION() void ShowStatCores();
	void SetActiveCategory(ECoreCategory Category);
	void RefreshCores(bool bChooseInitialCategory = false);
	void SelectCore(FName CoreId);
	void RefreshStats();
	UPROPERTY(Transient) TWeakObjectPtr<UCoreComponent> CoreSource;
	UPROPERTY(Transient) TWeakObjectPtr<UAbilitySystemComponent> AttributeSource;
	UPROPERTY(Transient) TArray<FOwnedCoreEntry> Entries;
	UPROPERTY(Transient) TArray<TObjectPtr<UOwnedCoreItemWidget>> Items;
	TMap<ECoreCategory, FName> SelectedByCategory;
	TMap<FGameplayAttribute, FDelegateHandle> AttributeSubscriptions;
	ECoreCategory ActiveCategory = ECoreCategory::Ability;
	FKey ToggleKey;
	FSlateBrush DefaultIcon;
};
