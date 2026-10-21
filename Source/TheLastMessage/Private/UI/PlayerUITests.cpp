#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/PlayerStatusWidget.h"
#include "UI/WeaponChargeWidget.h"
#include "UI/PlayerHUD.h"
#include "UI/PlayerResourceBarWidget.h"
#include "UI/PlayerWeaponSlotWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Character/Player/PlayerBase.h"
#include "DataAsset/WeaponDataAsset.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerUITest, "TheLastMessage.UI.StatusBindingAndCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerUITest::RunTest(const FString& Parameters)
{
	UClass* StatusClass = LoadClass<UPlayerStatusWidget>(nullptr, TEXT("/Game/Test_Jihong/UI/WBP_PlayerUI.WBP_PlayerUI_C"));
	UClass* ChargeClass = LoadClass<UWeaponChargeWidget>(nullptr, TEXT("/Game/Test_Jihong/UI/WBP_WeaponCharge.WBP_WeaponCharge_C"));
	UClass* HUDClass = LoadClass<APlayerHUD>(nullptr, TEXT("/Game/Test_Jihong/UI/BP_PlayerHUD.BP_PlayerHUD_C"));
	if (!TestNotNull(TEXT("Status BP exists"), StatusClass) || !TestNotNull(TEXT("Charge BP exists"), ChargeClass)
		|| !TestNotNull(TEXT("HUD BP exists"), HUDClass)) return false;
	TestEqual(TEXT("HUD selects derived status BP"),
		FindFProperty<FClassProperty>(APlayerHUD::StaticClass(), TEXT("StatusWidgetClass"))->GetObjectPropertyValue_InContainer(HUDClass->GetDefaultObject()), static_cast<UObject*>(StatusClass));
	TestEqual(TEXT("HUD selects derived charge BP"),
		FindFProperty<FClassProperty>(APlayerHUD::StaticClass(), TEXT("ChargeWidgetClass"))->GetObjectPropertyValue_InContainer(HUDClass->GetDefaultObject()), static_cast<UObject*>(ChargeClass));
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player = World->SpawnActor<APlayerBase>();
	UPlayerStatusWidget* Status = CreateWidget<UPlayerStatusWidget>(World, StatusClass);
	UWeaponChargeWidget* Charge = CreateWidget<UWeaponChargeWidget>(World, ChargeClass);
	if (!Status || !Charge)
	{
		AddError(TEXT("Could not create UI from derived Widget Blueprints"));
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
	}
	Status->SetObservedPlayer(Player);
	Player->GetCombatAttributes()->SetMaxHealth(100);
	Player->GetCombatAttributes()->SetHealth(63);
	TestEqual(TEXT("HP listens to GAS changes"), Status->Health, 63.0f);
	UPlayerResourceBarWidget* HealthResource = Cast<UPlayerResourceBarWidget>(Status->GetWidgetFromName(TEXT("HealthResource")));
	UProgressBar* HealthBar = HealthResource ? Cast<UProgressBar>(HealthResource->GetWidgetFromName(TEXT("ValueBar"))) : nullptr;
	if (TestNotNull(TEXT("Designer HP bar binds to native property"), HealthBar)) TestEqual(TEXT("HP fraction"), HealthBar->GetPercent(), 0.63f);
	UWeaponDataAsset* Melee = NewObject<UWeaponDataAsset>(Player);
	Melee->WeaponType = EWeaponType::LightMelee;
	Melee->WeaponId = TEXT("Pipe");
	Melee->WeaponIcon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Test_Jihong/Blueprints/Weapon/Icon/T_UI_Weapon_Pipe_v1.T_UI_Weapon_Pipe_v1"));
	UWeaponDataAsset* Ranged = NewObject<UWeaponDataAsset>(Player);
	Ranged->WeaponType = EWeaponType::HeavyRanged;
	Ranged->WeaponId = TEXT("Railgun");
	Player->WeaponChoices[0] = Melee;
	Player->WeaponChoices[1] = DuplicateObject<UWeaponDataAsset>(Melee, Player);
	Player->WeaponChoices[2] = Ranged;
	TestTrue(TEXT("Equip primary melee"), Player->EquipWeapon(EWeaponSlot::Primary, Melee));
	TestTrue(TEXT("Equip secondary ranged"), Player->EquipWeapon(EWeaponSlot::Secondary, Ranged));
	UPlayerWeaponSlotWidget* Card = Cast<UPlayerWeaponSlotWidget>(Status->GetWidgetFromName(TEXT("WeaponSlot1")));
	UImage* Icon = Card ? Cast<UImage>(Card->GetWidgetFromName(TEXT("WeaponIcon"))) : nullptr;
	if (TestNotNull(TEXT("Editable weapon card exists"), Icon)) TestEqual(TEXT("Owned weapon icon is visible"), Icon->GetVisibility(), ESlateVisibility::HitTestInvisible);
	UPlayerWeaponSlotWidget* EmptyCard = Cast<UPlayerWeaponSlotWidget>(Status->GetWidgetFromName(TEXT("WeaponSlot2")));
	UImage* EmptyIcon = EmptyCard ? Cast<UImage>(EmptyCard->GetWidgetFromName(TEXT("WeaponIcon"))) : nullptr;
	if (TestNotNull(TEXT("Unowned card exists"), EmptyIcon)) TestEqual(TEXT("Unowned weapon stays blank"), EmptyIcon->GetVisibility(), ESlateVisibility::Hidden);
	UImage* MouseIcon = Card ? Cast<UImage>(Card->GetWidgetFromName(TEXT("LeftMouseIcon"))) : nullptr;
	if (TestNotNull(TEXT("Replaceable mouse badge exists"), MouseIcon)) TestEqual(TEXT("Left assignment badge is maintained"), MouseIcon->GetVisibility(), ESlateVisibility::HitTestInvisible);
	UTextBlock* HPText = HealthResource ? Cast<UTextBlock>(HealthResource->GetWidgetFromName(TEXT("ValueText"))) : nullptr;
	if (TestNotNull(TEXT("Editable left-aligned resource text exists"), HPText)) TestEqual(TEXT("Resource count formatting"), HPText->GetText().ToString(), FString(TEXT("63 / 100")));
	Player->GetWeaponSlotAttributes()->SetSharedEnergy(37);
	TestEqual(TEXT("Shared energy listens to GAS changes"), Status->DisplayEnergy, 37.0f);
	UPlayerResourceBarWidget* EnergyResource = Cast<UPlayerResourceBarWidget>(Status->GetWidgetFromName(TEXT("EnergyResource")));
	UProgressBar* EnergyBar = EnergyResource ? Cast<UProgressBar>(EnergyResource->GetWidgetFromName(TEXT("ValueBar"))) : nullptr;
	if (TestNotNull(TEXT("Designer energy bar"), EnergyBar)) TestEqual(TEXT("Energy fraction"), EnergyBar->GetPercent(), 0.37f);
	Player->BeginWeaponCharge(EWeaponSlot::Secondary, 1.0f);
	TestTrue(TEXT("Charge state begins"), Player->IsChargingWeapon());
	for (int32 I = 0; I < 50; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.01f); }
	TestTrue(TEXT("Authoritative charge clock advances"), FMath::IsNearlyEqual(Player->GetWeaponChargeRatio(), 0.5f, 0.02f));
	Charge->SetChargeProgress(Player->GetWeaponChargeRatio());
	UProgressBar* ChargeBar = Cast<UProgressBar>(Charge->GetWidgetFromName(TEXT("ChargeBar")));
	if (TestNotNull(TEXT("Designer charge bar binds"), ChargeBar)) TestTrue(TEXT("Charge fraction reaches bar"), FMath::IsNearlyEqual(ChargeBar->GetPercent(), 0.5f, 0.02f));
	Charge->SetChargeProgress(1.2f);
	TestEqual(TEXT("Full charge clamped"), Charge->ChargeRatio, 1.0f);
	TestTrue(TEXT("Full charge ready state"), Charge->bFullyCharged);
	Player->ClearWeaponCharge(EWeaponSlot::Secondary);
	TestFalse(TEXT("Charge cleared on release/cancel"), Player->IsChargingWeapon());
	APlayerBase* Replacement = World->SpawnActor<APlayerBase>(FVector(1000, 0, 0), FRotator::ZeroRotator);
	Replacement->GetCombatAttributes()->SetMaxHealth(80);
	Replacement->GetCombatAttributes()->SetHealth(50);
	Status->SetObservedPlayer(Replacement);
	Player->GetCombatAttributes()->SetHealth(1);
	TestEqual(TEXT("Possession switch removes previous ASC bindings"), Status->Health, 50.0f);
	Replacement->GetCombatAttributes()->SetHealth(42);
	TestEqual(TEXT("New pawn ASC binding works"), Status->Health, 42.0f);
	Status->SetObservedPlayer(nullptr);
	Replacement->GetCombatAttributes()->SetHealth(20);
	TestEqual(TEXT("Unbind clears resource values"), Status->Health, 0.0f);
	Player->BeginWeaponCharge(EWeaponSlot::Secondary, 1);
	Player->HandleDeath();
	TestFalse(TEXT("Death clears presentation charge state"), Player->IsChargingWeapon());
	TestEqual(TEXT("Death clears charging slot"), Player->GetChargingWeaponSlot(), EWeaponSlot::None);
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
