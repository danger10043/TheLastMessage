#include "Character/Player/PlayerBase.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "DataAsset/WeaponDataAsset.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "Gimmick/Component/GravityStateComponent.h"
#include "Map/AreaMapComponent.h"
#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void APlayerBase::ScheduleProgressRestore()
{
	if (!HasAuthority() || !GetGameInstance()) return;
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (GetGameInstance()) GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->RestorePlayerIfPending(this);
	}));
}

bool APlayerBase::RestoreCheckpointProgress(const UProgressSaveGame* Save)
{
	if (!Save || !HasAuthority() || !bStartupGranted || IsDead() || !Save->CheckpointId.IsValid()
		|| Save->RespawnTransform.ContainsNaN()) return false;
	// Resolve every saved definition before removing existing grants/equipment.
	TArray<FOwnedCoreEntry> Cores;
	for (const auto& Pair : Save->Player.CoreLevels)
	{
		UCoreDataAsset* Data = Cast<UCoreDataAsset>(Pair.Key.TryLoad());
		if (!Data || Pair.Value < 1 || !Data->HasValidGrantSetup()
			|| (Data->UpgradeLimitType == ECoreUpgradeLimitType::Custom && Pair.Value > Data->MaxLevel)) return false;
		FOwnedCoreEntry& Entry = Cores.AddDefaulted_GetRef(); Entry.CoreData = Data; Entry.Level = Pair.Value;
	}
	TArray<UWeaponDataAsset*> Owned;
	for (const FSoftObjectPath& Path : Save->Player.OwnedWeapons)
	{
		UWeaponDataAsset* Data = Cast<UWeaponDataAsset>(Path.TryLoad());
		if (!Data) return false;
		Owned.AddUnique(Data);
	}
	UWeaponDataAsset* Primary = Cast<UWeaponDataAsset>(Save->Player.PrimaryWeapon.TryLoad());
	UWeaponDataAsset* Secondary = Cast<UWeaponDataAsset>(Save->Player.SecondaryWeapon.TryLoad());
	if ((!Save->Player.PrimaryWeapon.IsNull() && !Primary) || (!Save->Player.SecondaryWeapon.IsNull() && !Secondary)) return false;
	CleanupPlayerActions();
	CoreComponent->ResetOwnedCores();
	for (const FOwnedCoreEntry& Entry : Cores)
		for (int32 Level = 0; Level < Entry.Level; ++Level)
			if (!IsCoreAcquireSuccess(CoreComponent->AcquireCore(Entry.CoreData))) return false;
	PrimaryWeapon = nullptr; SecondaryWeapon = nullptr;
	InitialPrimaryWeaponData = nullptr; InitialSecondaryWeaponData = nullptr;
	CachedChoiceWeapons.Reset();
	OwnedWeaponChoices.Init(false, WeaponChoices.Num());
	for (int32 Index = 0; Index < WeaponChoices.Num(); ++Index)
		if (Owned.Contains(WeaponChoices[Index].Get())) AcquireWeaponChoice(Index);
	if (Primary && !EquipWeapon(EWeaponSlot::Primary, Primary)) return false;
	if (Secondary && !EquipWeapon(EWeaponSlot::Secondary, Secondary)) return false;
	RefreshPrimaryWeaponVisual(); OnWeaponDataChanged.Broadcast();
	const FGameplayTag GravityTag = FGameplayTag::RequestGameplayTag(TEXT("State.Gravity.Reversed"));
	const bool bSavedReversed = Save->RespawnGravity.Z > 0;
	if (AbilitySystemComponent->HasMatchingGameplayTag(GravityTag) != bSavedReversed && GravityStateComponent)
		GravityStateComponent->ToggleGravityState();
	GetCharacterMovement()->SetGravityDirection(Save->RespawnGravity.GetSafeNormal(SMALL_NUMBER, FVector(0, 0, -1)));
	if (!TeleportTo(Save->RespawnTransform.GetLocation(), Save->RespawnTransform.Rotator())) return false;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (!SetCheckpoint(Save->RespawnTransform)) return false;
	CombatAttributes->SetHealth(FMath::Clamp(Save->Player.Health, 1.f, CombatAttributes->GetMaxHealth()));
	WeaponSlotAttributes->SetSharedEnergy(FMath::Clamp(Save->Player.Energy, 0.f, WeaponSlotAttributes->GetSharedMaxEnergy()));
	LastSharedEnergyUseTime = GetWorld()->GetTimeSeconds();
	if (AreaMapComponent)
	{
		const FProgressMapState* Map = Save->Maps.Find(FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
		AreaMapComponent->RestoreExploration(Map ? Map->ExploredRooms : TSet<FName>());
	}
	ForceNetUpdate();
	return true;
}
