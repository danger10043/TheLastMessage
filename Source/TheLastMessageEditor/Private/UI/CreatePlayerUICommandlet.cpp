#include "UI/CreatePlayerUICommandlet.h"
#include "UI/PlayerStatusWidget.h"
#include "UI/WeaponChargeWidget.h"
#include "UI/PlayerHUD.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Interfaces/ISlateRHIRendererModule.h"

namespace
{
const FLinearColor Ink(0.87f, 0.94f, 0.96f);
const FLinearColor Muted(0.4f, 0.56f, 0.62f);
const FLinearColor Cyan(0.12f, 0.66f, 1.0f);

template<typename T> T* Widget(UWidgetTree* Tree, const TCHAR* Name)
{
	T* Result = Tree->ConstructWidget<T>(T::StaticClass(), FName(Name));
	Result->bIsVariable = true;
	return Result;
}

UTextBlock* Text(UWidgetTree* Tree, const TCHAR* Name, const TCHAR* Value, int32 Size, FLinearColor Color = Ink)
{
	UTextBlock* Result = Widget<UTextBlock>(Tree, Name);
	Result->SetText(FText::FromString(Value));
	FSlateFontInfo Font = Result->GetFont();
	Font.Size = Size;
	Result->SetFont(Font);
	Result->SetColorAndOpacity(FSlateColor(Color));
	return Result;
}

void Gap(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Name, float Height)
{
	USpacer* Spacer = Widget<USpacer>(Tree, Name);
	Spacer->SetSize(FVector2D(1, Height));
	Parent->AddChildToVerticalBox(Spacer);
}

UProgressBar* Bar(UWidgetTree* Tree, const TCHAR* Name, FLinearColor Color)
{
	UProgressBar* Result = Widget<UProgressBar>(Tree, Name);
	FProgressBarStyle Style;
	Style.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(0.035f, 0.065f, 0.08f), 3.0f));
	Style.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 3.0f));
	Result->SetWidgetStyle(Style);
	Result->SetBarFillStyle(EProgressBarFillStyle::Scale);
	Result->SetFillColorAndOpacity(Color);
	Result->SetPercent(0);
	return Result;
}

USizeBox* Sized(UWidgetTree* Tree, const TCHAR* Name, UWidget* Content, float Height, float Width = 0)
{
	USizeBox* Box = Widget<USizeBox>(Tree, Name);
	Box->SetHeightOverride(Height);
	if (Width > 0) Box->SetWidthOverride(Width);
	Box->AddChild(Content);
	return Box;
}

void WeaponRow(UWidgetTree* Tree, UVerticalBox* Body, bool bPrimary)
{
	const FString Prefix = bPrimary ? TEXT("Primary") : TEXT("Secondary");
	UHorizontalBox* Title = Widget<UHorizontalBox>(Tree, *(Prefix + TEXT("WeaponTitle")));
	Body->AddChildToVerticalBox(Title);
	UTextBlock* Key = Text(Tree, *(Prefix + TEXT("InputLabel")), bPrimary ? TEXT("LMB") : TEXT("RMB"), 10, Muted);
	Title->AddChildToHorizontalBox(Sized(Tree, *(Prefix + TEXT("InputSize")), Key, 28, 34))->SetVerticalAlignment(VAlign_Center);
	UImage* Icon = Widget<UImage>(Tree, *(Prefix + TEXT("WeaponIcon")));
	Title->AddChildToHorizontalBox(Sized(Tree, *(Prefix + TEXT("IconSize")), Icon, 28, 32))->SetPadding(FMargin(0, 0, 8, 0));
	UTextBlock* Name = Text(Tree, *(Prefix + TEXT("WeaponText")), TEXT("--"), 13);
	Title->AddChildToHorizontalBox(Name)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox* Energy = Widget<UHorizontalBox>(Tree, *(Prefix + TEXT("EnergyRow")));
	Body->AddChildToVerticalBox(Energy)->SetPadding(FMargin(34, 4, 0, 0));
	UProgressBar* Progress = Bar(Tree, *(Prefix + TEXT("EnergyBar")), Cyan);
	UHorizontalBoxSlot* ProgressSlot = Energy->AddChildToHorizontalBox(Sized(Tree, *(Prefix + TEXT("EnergySize")), Progress, 7));
	ProgressSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ProgressSlot->SetVerticalAlignment(VAlign_Center);
	UTextBlock* Count = Text(Tree, *(Prefix + TEXT("EnergyText")), TEXT("0 / 0"), 11, Muted);
	Count->SetJustification(ETextJustify::Right);
	Energy->AddChildToHorizontalBox(Sized(Tree, *(Prefix + TEXT("CountSize")), Count, 18, 80))->SetPadding(FMargin(8, 0, 0, 0));
}

void BuildStatus(UWidgetBlueprint* BP)
{
	UWidgetTree* Tree = BP->WidgetTree;
	UCanvasPanel* Root = Widget<UCanvasPanel>(Tree, TEXT("StatusCanvas"));
	Tree->RootWidget = Root;
	UBorder* Panel = Widget<UBorder>(Tree, TEXT("StatusPanel"));
	Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.012f, 0.025f, 0.035f, 0.94f), 8.0f));
	Panel->SetPadding(FMargin(16));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetPosition(FVector2D(32, 32));
	PanelSlot->SetAutoSize(true);
	USizeBox* Width = Widget<USizeBox>(Tree, TEXT("StatusWidth"));
	Width->SetWidthOverride(300);
	Panel->AddChild(Width);
	UVerticalBox* Body = Widget<UVerticalBox>(Tree, TEXT("StatusBody"));
	Width->AddChild(Body);
	UHorizontalBox* Header = Widget<UHorizontalBox>(Tree, TEXT("HealthHeader"));
	Body->AddChildToVerticalBox(Header);
	Header->AddChildToHorizontalBox(Text(Tree, TEXT("HealthLabel"), TEXT("HP"), 12, Muted));
	UTextBlock* Count = Text(Tree, TEXT("HealthText"), TEXT("0 / 0"), 15);
	Count->SetJustification(ETextJustify::Right);
	Header->AddChildToHorizontalBox(Count)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Gap(Tree, Body, TEXT("HealthTopGap"), 7);
	Body->AddChildToVerticalBox(Sized(Tree, TEXT("HealthBarSize"), Bar(Tree, TEXT("HealthBar"), FLinearColor(0.15f, 0.85f, 0.48f)), 12));
	Gap(Tree, Body, TEXT("PrimaryTopGap"), 14);
	WeaponRow(Tree, Body, true);
	Gap(Tree, Body, TEXT("SecondaryTopGap"), 8);
	WeaponRow(Tree, Body, false);
}

void BuildCharge(UWidgetBlueprint* BP)
{
	UWidgetTree* Tree = BP->WidgetTree;
	USizeBox* Root = Widget<USizeBox>(Tree, TEXT("ChargeSize"));
	Root->SetWidthOverride(160);
	Root->SetHeightOverride(36);
	Tree->RootWidget = Root;
	UBorder* Panel = Widget<UBorder>(Tree, TEXT("ChargePanel"));
	Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.012f, 0.025f, 0.035f, 0.94f), 5.0f));
	Panel->SetPadding(FMargin(8, 4));
	Root->AddChild(Panel);
	UVerticalBox* Body = Widget<UVerticalBox>(Tree, TEXT("ChargeBody"));
	Panel->AddChild(Body);
	UHorizontalBox* Header = Widget<UHorizontalBox>(Tree, TEXT("ChargeHeader"));
	Body->AddChildToVerticalBox(Header);
	Header->AddChildToHorizontalBox(Text(Tree, TEXT("ChargeLabel"), TEXT("CHARGE"), 9, Muted));
	UTextBlock* Percent = Text(Tree, TEXT("ChargeText"), TEXT("0%"), 9);
	Percent->SetJustification(ETextJustify::Right);
	Header->AddChildToHorizontalBox(Percent)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Gap(Tree, Body, TEXT("ChargeGap"), 4);
	Body->AddChildToVerticalBox(Sized(Tree, TEXT("ChargeBarSize"), Bar(Tree, TEXT("ChargeBar"), Cyan), 7));
}

UBlueprint* Blueprint(const FString& Path, UClass* Parent, bool bWidget)
{
	if (UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, *Path)) return Existing->ParentClass == Parent ? Existing : nullptr;
	UPackage* Package = CreatePackage(*Path);
	const FName Name(*FPackageName::GetLongPackageAssetName(Path));
	UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(Parent, Package, Name, BPTYPE_Normal,
		bWidget ? UWidgetBlueprint::StaticClass() : UBlueprint::StaticClass(),
		bWidget ? UWidgetBlueprintGeneratedClass::StaticClass() : UBlueprintGeneratedClass::StaticClass());
	if (BP) FAssetRegistryModule::AssetCreated(BP);
	return BP;
}

bool Save(UBlueprint* BP)
{
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error) return false;
	UPackage* Package = BP->GetOutermost();
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Package, BP, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}

bool RenderWidget(UUserWidget* Instance, FVector2D Size, const FString& Filename)
{
	TSharedRef<SWidget> SlateWidget = Instance->TakeWidget();
	SlateWidget->SlatePrepass(1.0f);
	FWidgetRenderer Renderer(true);
	UTextureRenderTarget2D* Target = Renderer.DrawWidget(SlateWidget, Size);
	if (!Target) return false;
	FlushRenderingCommands();
	TArray<FColor> Pixels;
	FReadSurfaceDataFlags Flags(RCM_UNorm);
	Flags.SetLinearToGamma(false);
	if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) return false;
	TArray64<uint8> Bytes;
	FImageUtils::PNGCompressImageArray(int32(Size.X), int32(Size.Y), Pixels, Bytes);
	return FFileHelper::SaveArrayToFile(Bytes, *Filename);
}

bool RenderPreviews(UWidgetBlueprint* StatusBP, UWidgetBlueprint* ChargeBP)
{
	if (!FApp::CanEverRender())
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerUI: preview requires rendering and Slate (-AllowCommandletRendering, without -NullRHI)."));
		return false;
	}
	const bool bCreatedSlate = !FSlateApplication::IsInitialized();
	if (bCreatedSlate)
	{
		ISlateRHIRendererModule& RendererModule = FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer"));
		FSlateApplication::InitializeAsStandaloneApplication(RendererModule.CreateSlateRHIRenderer());
	}
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	// The offscreen preview needs resource data, not a skeletal render scene.
	APlayerBase* Player = World->SpawnActor<APlayerBase>();
	Player->InitializePlayer();
	Player->GetCombatAttributes()->SetMaxHealth(100);
	Player->GetCombatAttributes()->SetHealth(75);
	UWeaponDataAsset* PrimaryData = DuplicateObject<UWeaponDataAsset>(LoadObject<UWeaponDataAsset>(nullptr,
		TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Plasmapistol.DA_Weapon_Plasmapistol")), Player);
	UWeaponDataAsset* SecondaryData = DuplicateObject<UWeaponDataAsset>(LoadObject<UWeaponDataAsset>(nullptr,
		TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Railgun.DA_Weapon_Railgun")), Player);
	if (PrimaryData) { PrimaryData->WeaponMesh = nullptr; Player->EquipWeapon(EWeaponSlot::Primary, PrimaryData); }
	if (SecondaryData) { SecondaryData->WeaponMesh = nullptr; Player->EquipWeapon(EWeaponSlot::Secondary, SecondaryData); }
	Player->GetWeaponSlotAttributes()->SetPrimaryEnergy(72);
	Player->GetWeaponSlotAttributes()->SetSecondaryEnergy(40);
	UPlayerStatusWidget* Status = CreateWidget<UPlayerStatusWidget>(World, StatusBP->GeneratedClass.Get());
	UWeaponChargeWidget* Charge = CreateWidget<UWeaponChargeWidget>(World, ChargeBP->GeneratedClass.Get());
	if (!Status || !Charge) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
	Status->SetObservedPlayer(Player);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("PlayerUIPreview");
	IFileManager::Get().MakeDirectory(*Directory, true);
	bool bSuccess = RenderWidget(Status, FVector2D(400, 260), Directory / TEXT("Status.png"));
	Charge->SetChargeProgress(0.65f);
	bSuccess &= RenderWidget(Charge, FVector2D(160, 36), Directory / TEXT("Charge65.png"));
	Charge->SetChargeProgress(1.0f);
	bSuccess &= RenderWidget(Charge, FVector2D(160, 36), Directory / TEXT("ChargeReady.png"));
	Status->SetObservedPlayer(nullptr);
	Status->ReleaseSlateResources(true);
	Charge->ReleaseSlateResources(true);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	if (bCreatedSlate)
	{
		FlushRenderingCommands();
		FSlateApplication::Shutdown();
	}
	return bSuccess;
}
}

UCreatePlayerUICommandlet::UCreatePlayerUICommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCreatePlayerUICommandlet::Main(const FString& Params)
{
	UWidgetBlueprint* Status = Cast<UWidgetBlueprint>(Blueprint(TEXT("/Game/Test_Jihong/UI/WBP_PlayerStatus"), UPlayerStatusWidget::StaticClass(), true));
	UWidgetBlueprint* Charge = Cast<UWidgetBlueprint>(Blueprint(TEXT("/Game/Test_Jihong/UI/WBP_WeaponCharge"), UWeaponChargeWidget::StaticClass(), true));
	UBlueprint* HUD = Blueprint(TEXT("/Game/Test_Jihong/UI/BP_PlayerHUD"), APlayerHUD::StaticClass(), false);
	if (!Status || !Charge || !HUD) { UE_LOG(LogTemp, Error, TEXT("PlayerUI: an existing asset has a different parent class.")); return 1; }
	if (FParse::Param(*Params, TEXT("PreviewOnly"))) return RenderPreviews(Status, Charge) ? 0 : 1;
	if (!Status->WidgetTree->RootWidget) BuildStatus(Status);
	if (!Charge->WidgetTree->RootWidget) BuildCharge(Charge);
	if (!Save(Status) || !Save(Charge)) return 1;
	FKismetEditorUtilities::CompileBlueprint(HUD);
	UObject* HUDDefaults = HUD->GeneratedClass->GetDefaultObject();
	FindFProperty<FClassProperty>(APlayerHUD::StaticClass(), TEXT("StatusWidgetClass"))->SetObjectPropertyValue_InContainer(HUDDefaults, Status->GeneratedClass);
	FindFProperty<FClassProperty>(APlayerHUD::StaticClass(), TEXT("ChargeWidgetClass"))->SetObjectPropertyValue_InContainer(HUDDefaults, Charge->GeneratedClass);
	if (!Save(HUD)) return 1;
	UBlueprint* GM = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Test_Jihong/GM_Test.GM_Test"));
	if (!GM || !GM->GeneratedClass->IsChildOf(AGameModeBase::StaticClass())) return 1;
	GM->GeneratedClass->GetDefaultObject<AGameModeBase>()->HUDClass = HUD->GeneratedClass;
	if (!Save(GM)) return 1;
	UE_LOG(LogTemp, Display, TEXT("PLAYER_UI_CREATED: WBP_PlayerStatus, WBP_WeaponCharge, BP_PlayerHUD; GM_Test.HUDClass configured."));
	return 0;
}
