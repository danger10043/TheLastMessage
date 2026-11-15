#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldFlagSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldFlagChanged, FGameplayTag, Flag, bool, bValue);
DECLARE_MULTICAST_DELEGATE(FOnWorldFlagsResynced);
/** 플래그가 켜질 때 호출된다. true를 반환하면 그 플래그의 "보여 주기"를 보류한다(EndFlagDeferral까지 받는 쪽은 꺼진 것으로 본다). */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FWorldFlagDeferHandler, FGameplayTag /*Flag*/, bool /*bValue*/);

/**
 * 방이 로드/언로드되어도 유지되는 월드 플래그. 스위치(FlagEmitter)가 켜고 문(FlagReceiver)이 읽는다.
 * 현재 값은 여기에 있고, 체크포인트를 저장할 때 세이브 파일로 복사된다(CorePickupStateSubsystem과 같은 방식).
 * 따라서 죽어서 체크포인트로 돌아가면 플래그도 그 시점으로 돌아간다.
 */
UCLASS()
class THELASTMESSAGE_API UWorldFlagSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UWorldFlagSubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintPure, Category = "Gimmick|Flag")
	bool IsFlagSet(FGameplayTag Flag) const;

	/** Flags 중 조건을 만족하는지. 비어 있으면 false. */
	UFUNCTION(BlueprintPure, Category = "Gimmick|Flag")
	bool AreFlagsSet(const TArray<FGameplayTag>& Flags, bool bRequireAll = true) const;

	/** 값이 바뀔 때만 OnFlagChanged를 보낸다. */
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Flag")
	void SetFlag(FGameplayTag Flag, bool bValue = true);

	/**
	 * 받는 쪽(FlagReceiver)이 쓰는 판정. 켜져 있어도 연출 보류 중인 플래그는 꺼진 것으로 본다.
	 * 저장되는 값(IsFlagSet)은 보류와 상관없이 이미 켜져 있다.
	 */
	bool AreFlagsPresented(const TArray<FGameplayTag>& Flags, bool bRequireAll) const;

	/** 보류를 끝내고 받는 쪽에 알린다(문이 열리는 연출이 이때 재생된다). */
	void EndFlagDeferral(FGameplayTag Flag, bool bNotify = true);

	/** 연출 시스템이 연결한다. 한 곳만 연결할 수 있다. */
	FWorldFlagDeferHandler DeferHandler;

	/** 세이브 포인트에서 복사할 현재 플래그. */
	const TSet<FName>& GetFlags() const { return ActiveFlags; }

	/** 세이브 불러오기/체크포인트 복원 때 호출한다. 이벤트는 보내지 않고, 이후 ResyncReceivers로 알린다. */
	void RestoreFlags(const TSet<FName>& SavedFlags);

	/** 새 게임이나 세션 종료 때 기록을 비운다. */
	void ResetFlags() { ActiveFlags.Reset(); DeferredFlags.Reset(); }

	/** 기믹 복원이 끝난 뒤 모든 받는 쪽이 현재 플래그에 맞춰 상태를 다시 맞추게 한다. */
	void ResyncReceivers() { OnFlagsResynced.Broadcast(); }

	UPROPERTY(BlueprintAssignable, Category = "Gimmick|Flag")
	FOnWorldFlagChanged OnFlagChanged;

	FOnWorldFlagsResynced OnFlagsResynced;

private:
	bool EvaluateFlags(const TArray<FGameplayTag>& Flags, bool bRequireAll, bool bPresentedOnly) const;

	UPROPERTY(Transient)
	TSet<FName> ActiveFlags;

	/** 켜졌지만 연출이 끝나기 전이라 받는 쪽에는 아직 꺼진 것으로 보이는 플래그. 저장하지 않는다. */
	TSet<FName> DeferredFlags;

	FDelegateHandle GimmickRestoreHandle;
};
