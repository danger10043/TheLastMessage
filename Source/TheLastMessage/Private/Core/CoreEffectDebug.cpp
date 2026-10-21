// 에디터 플레이에서 코어 설정값과 실제 적용 결과를 비교하기 위한 도구다.
// 게임의 전투 계산에는 관여하지 않으며, 패키징한 게임에는 포함하지 않는다.
#if WITH_EDITOR

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CanvasItem.h"
#include "Character/Player/PlayerBase.h"
#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "HAL/IConsoleManager.h"

namespace CoreEffectDebug
{
struct FMonitor
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UAbilitySystemComponent> PlayerASC;
	TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
	FDelegateHandle DrawHandle;
	FDelegateHandle CleanupHandle;
	FDelegateHandle PlayerHealthHandle;
	FDelegateHandle TargetHealthHandle;
	float LastReceived = 0.0f;
	float LastTargetDrop = 0.0f;
	double EnergySampleTime = -1.0;
	float EnergySampleValue = 0.0f;
	float ObservedRechargeRate = 0.0f;
	bool bAdjustingTestHealth = false;
	bool bWasCharging = false;
	double ChargeObservedStart = 0.0;
	float LastObservedChargeSeconds = 0.0f;
	FString Message;
};

FMonitor Monitor;

APlayerBase* GetPlayer(UWorld* World)
{
	APlayerController* PC = World && World->IsGameWorld() ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<APlayerBase>(PC->GetPawn()) : nullptr;
}

void Unbind(TWeakObjectPtr<UAbilitySystemComponent>& ASC, FDelegateHandle& Handle, const FGameplayAttribute& Attribute)
{
	if (UAbilitySystemComponent* Component = ASC.Get(); Component && Handle.IsValid() && Attribute.IsValid())
	{
		Component->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(Handle);
	}
	Handle.Reset();
	ASC.Reset();
}

FGameplayAttribute FindHealth(UAbilitySystemComponent* ASC)
{
	TArray<FGameplayAttribute> Attributes;
	if (ASC) ASC->GetAllAttributes(Attributes);
	for (const FGameplayAttribute& Attribute : Attributes)
	{
		if (Attribute.GetName() == TEXT("Health")) return Attribute;
	}
	return FGameplayAttribute();
}

void Disable()
{
	Unbind(Monitor.PlayerASC, Monitor.PlayerHealthHandle, UCombatAttributeSet::GetHealthAttribute());
	Unbind(Monitor.TargetASC, Monitor.TargetHealthHandle, FindHealth(Monitor.TargetASC.Get()));
	if (Monitor.DrawHandle.IsValid()) UDebugDrawService::Unregister(Monitor.DrawHandle);
	if (Monitor.CleanupHandle.IsValid()) FWorldDelegates::OnWorldCleanup.Remove(Monitor.CleanupHandle);
	Monitor = FMonitor();
}

void WorldCleanup(UWorld* World, bool, bool)
{
	if (Monitor.World.Get() == World) Disable();
}

void ObservePlayer(APlayerBase* Player)
{
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (Monitor.PlayerASC.Get() == ASC) return;
	Unbind(Monitor.PlayerASC, Monitor.PlayerHealthHandle, UCombatAttributeSet::GetHealthAttribute());
	Monitor.PlayerASC = ASC;
	Monitor.LastReceived = 0.0f;
	Monitor.PlayerHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetHealthAttribute())
		.AddLambda([](const FOnAttributeChangeData& Data)
		{
			// 배율로 계산한 예상값이 아니라, 실제 Health 변경 알림의 차이를 기록한다.
			if (!Monitor.bAdjustingTestHealth && Data.NewValue < Data.OldValue) Monitor.LastReceived = Data.OldValue - Data.NewValue;
		});
}

void Draw(UCanvas* Canvas, APlayerController*)
{
	APlayerBase* Player = GetPlayer(Monitor.World.Get());
	if (!Canvas || !Canvas->Canvas || !Player || !Player->IsLocallyControlled() || !GEngine) return;
	ObservePlayer(Player);
	const UCombatAttributeSet* Stats = Player->GetCombatAttributes();
	const UWeaponSlotAttributeSet* Energy = Player->GetWeaponSlotAttributes();
	const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	const double Now = Monitor.World->GetTimeSeconds();
	const bool bCharging = Player->IsChargingWeapon();
	if (bCharging && !Monitor.bWasCharging) Monitor.ChargeObservedStart = Now;
	if (!bCharging && Monitor.bWasCharging) Monitor.LastObservedChargeSeconds = float(Now - Monitor.ChargeObservedStart);
	Monitor.bWasCharging = bCharging;
	const float CurrentEnergy = Energy->GetSharedEnergy();
	if (Monitor.EnergySampleTime < 0.0 || CurrentEnergy < Monitor.EnergySampleValue || Now < Monitor.EnergySampleTime)
	{
		Monitor.EnergySampleTime = Now;
		Monitor.EnergySampleValue = CurrentEnergy;
		Monitor.ObservedRechargeRate = 0.0f;
	}
	else if (Now - Monitor.EnergySampleTime >= 0.5)
	{
		Monitor.ObservedRechargeRate = (CurrentEnergy - Monitor.EnergySampleValue) / float(Now - Monitor.EnergySampleTime);
		Monitor.EnergySampleTime = Now;
		Monitor.EnergySampleValue = CurrentEnergy;
	}
	const float HealthRatio = Stats->GetMaxHealth() > 0.0f ? Stats->GetHealth() / Stats->GetMaxHealth() : 0.0f;
	const bool bHigh = HealthRatio >= Stats->GetHighHealthThresholdRatio() && Stats->GetHighHealthDamageMultiplier() != 1.0f;
	const bool bLow = HealthRatio <= Stats->GetLowHealthThresholdRatio() && Stats->GetLowHealthDamageMultiplier() != 1.0f;
	TArray<FString> Lines;
	Lines.Add(TEXT("CORE TEST  |  tlm.CoreDebug 0: hide"));
	Lines.Add(FString::Printf(TEXT("HP %.1f / %.1f (%.1f%%) | last HP loss: %.2f"), Stats->GetHealth(), Stats->GetMaxHealth(), HealthRatio * 100.0f, Monitor.LastReceived));
	Lines.Add(FString::Printf(TEXT("Taken x%.3f | Dealt x%.3f | Melee x%.3f"), Stats->GetDamageTakenMultiplier(), Stats->GetDamageDealtMultiplier(), Stats->GetMeleeDamageMultiplier()));
	Lines.Add(FString::Printf(TEXT("High HP: %s x%.3f | Low HP: %s x%.3f"), bHigh ? TEXT("ON") : TEXT("OFF"), Stats->GetHighHealthDamageMultiplier(), bLow ? TEXT("ON") : TEXT("OFF"), Stats->GetLowHealthDamageMultiplier()));
	Lines.Add(FString::Printf(TEXT("Critical chance %.1f%% | Critical damage x%.2f"), FMath::Clamp(Stats->GetCriticalChance(), 0.0f, 1.0f) * 100.0f, Stats->GetCriticalDamageMultiplier()));
	Lines.Add(FString::Printf(TEXT("Move x%.3f | actual MaxWalkSpeed %.1f | horizontal speed %.1f"), Stats->GetMoveSpeedMultiplier(), Movement->MaxWalkSpeed, Movement->Velocity.Size2D()));
	Lines.Add(FString::Printf(TEXT("Shared energy %.2f / %.2f | Recharge x%.3f"), Energy->GetSharedEnergy(), Energy->GetSharedMaxEnergy(), Stats->GetEnergyRechargeRateMultiplier()));
	Lines.Add(FString::Printf(TEXT("Observed energy gain %.2f / game second (idle, below max)"), Monitor.ObservedRechargeRate));
	Lines.Add(FString::Printf(TEXT("Charge x%.3f | actual charging: %s | actual progress %.1f%%"), Stats->GetChargeTimeMultiplier(), Player->IsChargingWeapon() ? TEXT("YES") : TEXT("NO"), Player->GetWeaponChargeRatio() * 100.0f));
	Lines.Add(FString::Printf(TEXT("Last observed charge hold: %.3fs (release/cancel; frame sampled)"), Monitor.LastObservedChargeSeconds));
	for (EWeaponSlot Slot : {EWeaponSlot::Primary, EWeaponSlot::Secondary})
	{
		const UWeaponDataAsset* Weapon = Player->GetWeaponDataInSlot(Slot);
		if (Weapon) Lines.Add(FString::Printf(TEXT("%s %s | charge %s | base %.2fs -> expected %.2fs"), Slot == EWeaponSlot::Primary ? TEXT("LMB") : TEXT("RMB"), *Weapon->WeaponId.ToString(), Weapon->bSupportsChargeAttack ? TEXT("YES") : TEXT("NO"), Weapon->MaxChargeTime, Weapon->MaxChargeTime * Stats->GetChargeTimeMultiplier()));
	}
	UAbilitySystemComponent* Target = Monitor.TargetASC.Get();
	Lines.Add(Target ? FString::Printf(TEXT("Target %s | HP %.2f | last actual HP loss: %.2f"), *GetNameSafe(Target->GetAvatarActor()), Target->GetNumericAttribute(FindHealth(Target)), Monitor.LastTargetDrop) : TEXT("Target: not selected (tlm.CoreTarget)"));
	Lines.Add(TEXT("Target HP loss includes ALL damage sources; shields/overkill affect the result."));
	Lines.Add(TEXT("tlm.CoreGive 2..11 | tlm.CoreHealth 80 | tlm.CoreHit 100 | tlm.CoreReset"));
	if (!Monitor.Message.IsEmpty()) Lines.Add(Monitor.Message);

	const FVector2D Origin(24.0f, 120.0f);
	FCanvasTileItem Background(Origin - FVector2D(8.0f), FVector2D(900.0f, Lines.Num() * 21.0f + 16.0f), FLinearColor(0.01f, 0.02f, 0.03f, 0.88f));
	Background.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Background);
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		FCanvasTextItem Text(Origin + FVector2D(0.0f, Index * 21.0f), FText::FromString(Lines[Index]), GEngine->GetSmallFont(), Index == 0 ? FLinearColor::Yellow : FLinearColor::White);
		Text.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Text);
	}
}

void Enable(UWorld* World)
{
	if (!GetPlayer(World)) return;
	if (Monitor.World.Get() == World && Monitor.DrawHandle.IsValid()) return;
	Disable();
	Monitor.World = World;
	ObservePlayer(GetPlayer(World));
	Monitor.DrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateStatic(&Draw));
	Monitor.CleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&WorldCleanup);
}

void Message(const FString& Text)
{
	Monitor.Message = Text;
	UE_LOG(LogTemp, Display, TEXT("CoreTest: %s"), *Text);
}

FAutoConsoleCommandWithWorldAndArgs ShowCommand(TEXT("tlm.CoreDebug"), TEXT("Show core effect values and observed results: tlm.CoreDebug 1 / 0."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsEmpty() && Args[0] == TEXT("0")) Disable();
		else Enable(World);
	}), ECVF_Cheat);

FAutoConsoleCommandWithWorldAndArgs GiveCommand(TEXT("tlm.CoreGive"), TEXT("Acquire a real core by test-list number 2..11. 9: charge acceleration, 11: energy recharge. Existing cores are kept."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerBase* Player = GetPlayer(World);
		int32 Number = 0;
		if (!Player || !Player->HasAuthority() || Args.Num() != 1 || !LexTryParseString(Number, *Args[0]) || Number < 2 || Number > 11) return;
		const TCHAR* Names[] = {TEXT("ImpactDispersionArmor"), TEXT("OverloadOutput"), TEXT("MeleeAmplifier"), TEXT("StableOutputCircuit"), TEXT("EmergencyOutputCircuit"), TEXT("PrecisionAimingCircuit"), TEXT("HighSpeedDrive"), TEXT("ChargeAccelerationCircuit"), TEXT("EfficientDischargeCircuit"), TEXT("FastRechargeCircuit")};
		UCoreDataAsset* Data = LoadObject<UCoreDataAsset>(nullptr, *FString::Printf(TEXT("/Game/Test_Jihun/Core/Stat/DataAsset/DA_Core_%s.DA_Core_%s"), Names[Number - 2], Names[Number - 2]));
		Enable(World);
		const ECoreAcquireResult Result = Data ? Player->GetCoreComponent()->AcquireCore(Data) : ECoreAcquireResult::InvalidData;
		Message(FString::Printf(TEXT("%d %s: %s (Lv.%d)"), Number, Data ? *Data->CoreName.ToString() : TEXT("missing DA"), *UEnum::GetValueAsString(Result), Data ? Player->GetCoreComponent()->GetCoreLevel(Data->CoreId) : 0));
	}), ECVF_Cheat);

FAutoConsoleCommandWithWorldAndArgs ResetCommand(TEXT("tlm.CoreReset"), TEXT("Remove ALL acquired cores in this play session. Does not edit assets."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (APlayerBase* Player = GetPlayer(World); Player && Player->HasAuthority())
		{
			Enable(World);
			Player->GetCoreComponent()->ResetOwnedCores();
			Message(TEXT("All owned cores removed for comparison. Includes ability unlock cores."));
		}
	}), ECVF_Cheat);

FAutoConsoleCommandWithWorldAndArgs HealthCommand(TEXT("tlm.CoreHealth"), TEXT("Set current HP to 1..100 percent of maximum for conditional-core testing."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerBase* Player = GetPlayer(World);
		float Percent = 0.0f;
		if (!Player || !Player->HasAuthority() || Player->IsDead() || Args.Num() != 1 || !LexTryParseString(Percent, *Args[0]) || !FMath::IsFinite(Percent) || Percent < 1.0f || Percent > 100.0f) return;
		Enable(World);
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		TGuardValue<bool> AdjustingHealth(Monitor.bAdjustingTestHealth, true);
		ASC->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), Player->GetCombatAttributes()->GetMaxHealth() * Percent / 100.0f);
		Message(FString::Printf(TEXT("Test HP set to %.1f%%"), Percent));
	}), ECVF_Cheat);

FAutoConsoleCommandWithWorldAndArgs HitCommand(TEXT("tlm.CoreHit"), TEXT("Apply a positive raw damage amount through the player's actual TakeDamage/GAS path."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerBase* Player = GetPlayer(World);
		float Damage = 0.0f;
		if (!Player || !Player->HasAuthority() || Player->IsDead() || Args.Num() != 1 || !LexTryParseString(Damage, *Args[0]) || !FMath::IsFinite(Damage) || Damage <= 0.0f) return;
		Enable(World);
		const float Actual = Player->TakeDamage(Damage, FDamageEvent(), Player->GetController(), Player);
		Message(FString::Printf(TEXT("Raw incoming %.2f -> actual HP loss %.2f"), Damage, Actual));
	}), ECVF_Cheat);

FAutoConsoleCommandWithWorldAndArgs TargetCommand(TEXT("tlm.CoreTarget"), TEXT("Observe HP changes on the nearest other actor with a GAS Health attribute."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		APlayerBase* Player = GetPlayer(World);
		if (!Player) return;
		Enable(World);
		Unbind(Monitor.TargetASC, Monitor.TargetHealthHandle, FindHealth(Monitor.TargetASC.Get()));
		float Nearest = TNumericLimits<float>::Max();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (*It == Player) continue;
			UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(*It);
			const FGameplayAttribute Health = FindHealth(ASC);
			const float Distance = FVector::DistSquared(It->GetActorLocation(), Player->GetActorLocation());
			if (ASC && Health.IsValid() && ASC->GetNumericAttribute(Health) > 0.0f && Distance < Nearest)
			{
				Nearest = Distance;
				Monitor.TargetASC = ASC;
			}
		}
		Monitor.LastTargetDrop = 0.0f;
		if (UAbilitySystemComponent* ASC = Monitor.TargetASC.Get())
		{
			Monitor.TargetHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(FindHealth(ASC))
				.AddLambda([](const FOnAttributeChangeData& Data)
				{
					if (Data.NewValue < Data.OldValue) Monitor.LastTargetDrop = Data.OldValue - Data.NewValue;
				});
		}
		Message(FString::Printf(TEXT("Observed target: %s"), Monitor.TargetASC.IsValid() ? *GetNameSafe(Monitor.TargetASC->GetAvatarActor()) : TEXT("none found")));
	}), ECVF_Cheat);
}

#endif // WITH_EDITOR
