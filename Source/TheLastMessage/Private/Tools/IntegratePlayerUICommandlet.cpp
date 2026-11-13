#include "Tools/IntegratePlayerUICommandlet.h"
#include "UI/AudioSettingsWidget.h"

#if WITH_EDITOR
#include "UI/FactoryUIStyle.h"
#include "UI/FactoryMenuStyle.h"
#include "Components/ScaleBox.h"
#include "Components/WrapBox.h"
#include "UI/PlayerStatusWidget.h"
#include "UI/PlayerResourceBarWidget.h"
#include "UI/PlayerWeaponSlotWidget.h"
#include "UI/PlayerHUD.h"
#include "UI/StartMenuWidget.h"
#include "UI/StartMenuHUD.h"
#include "UI/SaveNotificationWidget.h"
#include "UI/PauseMenuWidget.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "DataAsset/WeaponDataAsset.h"
#include "TextureCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "RenderingThread.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/App.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"

namespace PlayerUIBuild
{
const TCHAR* PanelPath = TEXT("/Game/Test_Jihong/UI/WBP_PlayerUI");
const TCHAR* CardPath = TEXT("/Game/Test_Jihong/UI/Components/WBP_WeaponSlot");
const TCHAR* BarPath = TEXT("/Game/Test_Jihong/UI/Components/WBP_ResourceBar");

void Backup(const FString& Path)
{
	const FString File = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
	const FString Dest = FPaths::ProjectSavedDir() / TEXT("PlayerUIIntegrationBackup") / Path.RightChop(6).Append(TEXT(".uasset"));
	if (IFileManager::Get().FileExists(*File) && !IFileManager::Get().FileExists(*Dest))
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest), true);
		IFileManager::Get().Copy(*Dest, *File);
	}
}

bool Save(UObject* Asset)
{
	UPackage* Package = Asset->GetOutermost();
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Package, Asset, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}

UWidgetBlueprint* Blueprint(const TCHAR* Path, UClass* Parent)
{
	Backup(Path);
	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, Path);
	if (!BP)
	{
		UPackage* Package = CreatePackage(Path);
		BP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(Parent, Package,
			FName(*FPackageName::GetLongPackageAssetName(Path)), BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
		FAssetRegistryModule::AssetCreated(BP);
	}
	BP->ParentClass = Parent;
	BP->WidgetTree = NewObject<UWidgetTree>(BP, MakeUniqueObjectName(BP, UWidgetTree::StaticClass(), TEXT("IntegratedWidgetTree")), RF_Transactional);
	return BP;
}

template<class T> T* Widget(UWidgetTree* Tree, const TCHAR* Name)
{
	T* Result = Tree->ConstructWidget<T>(T::StaticClass(), FName(Name));
	Result->bIsVariable = true;
	return Result;
}

UTextBlock* Text(UWidgetTree* Tree, const TCHAR* Name, int32 Size)
{
	UTextBlock* Result = Widget<UTextBlock>(Tree, Name);
	Result->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")), Size, TEXT("Regular")));
	Result->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	return Result;
}

bool CompileSave(UWidgetBlueprint* BP)
{
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return BP->Status != BS_Error && Save(BP);
}

bool ApplyProjectFonts()
{
	UFont* Title = LoadObject<UFont>(nullptr, TEXT("/Game/Main/UI/Fonts/F_TLM_Title"));
	UFont* UI = LoadObject<UFont>(nullptr, TEXT("/Game/Main/UI/Fonts/F_TLM_UI"));
	UFont* Body = LoadObject<UFont>(nullptr, TEXT("/Game/Main/UI/Fonts/F_TLM_Body"));
	if (!Title || !UI || !Body || !Title->GetCompositeFont() || !UI->GetCompositeFont() || !Body->GetCompositeFont()) return false;
	if (Title->GetCompositeFont()->FallbackTypeface.Typeface.Fonts.Num() != 1 ||
		UI->GetCompositeFont()->FallbackTypeface.Typeface.Fonts.Num() != 1 ||
		Body->GetCompositeFont()->DefaultTypeface.Fonts.Num() != 4) return false;

	struct FFontTarget { const TCHAR* Path; UFont* DefaultFont; };
	const FFontTarget Targets[] = {
		{TEXT("/Game/Main/UI/Menu/WBP_StartMenu"), Body},
		{TEXT("/Game/Main/UI/Menu/WBP_PauseMenu"), Body},
		{TEXT("/Game/Main/UI/HUD/WBP_PlayerUI"), UI},
		{TEXT("/Game/Main/UI/HUD/WBP_WeaponCharge"), UI},
		{TEXT("/Game/Main/UI/HUD/Components/WBP_WeaponSlot"), UI},
		{TEXT("/Game/Main/UI/HUD/Components/WBP_ResourceBar"), UI},
		{TEXT("/Game/Main/UI/Save/WBP_SaveNotification"), UI},
		{TEXT("/Game/Test_Jihun/Core/WBP/WBP_OwnedCore"), Body},
		{TEXT("/Game/Test_Jihun/Core/WBP/WBP_OwnedCoreItem"), UI},
		{TEXT("/Game/Test_Jihun/Core/WBP/WBP_CoreChoice"), Body},
		{TEXT("/Game/Test_Jihun/Core/WBP/WBP_CoreChoiceCard"), Body}
	};
	const TSet<FName> Headings{TEXT("Heading"), TEXT("SlotHeading"), TEXT("ConfirmHeading"),
		TEXT("SettingsHeading"), TEXT("ScreenTitle"), TEXT("TitleText")};
	const TSet<FName> SystemLabels{TEXT("NavigationHint"), TEXT("PausedLabel"), TEXT("StatusText"),
		TEXT("SelectionText"), TEXT("HintText"), TEXT("SelectionCounter"), TEXT("ModuleNumberText")};
	const TSet<FName> BodyWeights{TEXT("Regular"), TEXT("Medium"), TEXT("SemiBold"), TEXT("Bold")};
	for (const FFontTarget& Target : Targets)
	{
		UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, Target.Path);
		if (!BP || !BP->WidgetTree) return false;
		int32 Count = 0;
		BP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
			if (!TextBlock) return;
			FSlateFontInfo Font = TextBlock->GetFont();
			const FName Name = TextBlock->GetFName();
			UFont* Family = Headings.Contains(Name) ? Title : SystemLabels.Contains(Name) ? UI : Target.DefaultFont;
			Font.FontObject = Family;
			Font.CompositeFont.Reset();
			if (Family != Body || !BodyWeights.Contains(Font.TypefaceFontName)) Font.TypefaceFontName = TEXT("Regular");
			TextBlock->SetFont(Font);
			++Count;
		});
		if (Count > 0 && !CompileSave(BP)) return false;
		UE_LOG(LogTemp, Display, TEXT("TLM_FONT_APPLIED %s (%d text widgets)"), Target.Path, Count);
	}
	return true;
}

bool SaveUITexture(const FString& Path, int32 Size, const TArray<FColor>& Pixels, bool bRefresh = false)
{
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn);
	if (Texture && !bRefresh) return true;
	const bool bCreated = Texture == nullptr;
	if (bCreated)
	{
		Texture = NewObject<UTexture2D>(CreatePackage(*Path),
			FName(*FPackageName::GetLongPackageAssetName(Path)), RF_Public | RF_Standalone);
	}
	else Texture->PreEditChange(nullptr);
	Texture->Source.Init(Size, Size, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
	Texture->CompressionSettings = TC_EditorIcon;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->NeverStream = true;
	Texture->SRGB = true;
	Texture->UpdateResource();
	if (bCreated) FAssetRegistryModule::AssetCreated(Texture);
	else Texture->PostEditChange();
	return Save(Texture);
}

bool SaveFactoryLayout()
{
	for (const FactoryUI::EPlate State : {FactoryUI::EPlate::Normal, FactoryUI::EPlate::Selected, FactoryUI::EPlate::Screen})
	{
		const FString Path = FactoryUI::GetPlateTexture(State).ToSoftObjectPath().GetLongPackageName();
		if (!FPackageName::IsValidLongPackageName(Path))
		{
			UE_LOG(LogTemp, Error, TEXT("Configure the plate texture in Factory UI Assets before generating the UI layout."));
			return false;
		}
		TArray<FColor> Pixels;
		FactoryUI::MakePlatePixels(State, Pixels);
		if (!SaveUITexture(Path, 64, Pixels, true)) return false;
	}
	return true;
}

bool BuildStatusPlate(UWidgetBlueprint* BP)
{
	UWidgetTree* WidgetTree = BP->WidgetTree;
	UPlayerResourceBarWidget* HealthResource = Cast<UPlayerResourceBarWidget>(WidgetTree->FindWidget(TEXT("HealthResource")));
	UPlayerResourceBarWidget* EnergyResource = Cast<UPlayerResourceBarWidget>(WidgetTree->FindWidget(TEXT("EnergyResource")));
	UCanvasPanel* Canvas = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Canvas || !HealthResource || !EnergyResource || WidgetTree->FindWidget(TEXT("UnifiedStatusPlate"))) return Canvas != nullptr && HealthResource != nullptr && EnergyResource != nullptr;

	UOverlay* Plate = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("UnifiedStatusPlate"));
	auto* Position = Canvas->AddChildToCanvas(Plate);
	Position->SetAnchors(FAnchors(0, 1));
	Position->SetAlignment(FVector2D(0, 1));
	Position->SetPosition(FVector2D(28, -28));
	Position->SetSize(FVector2D(530, 116));
	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/HUD/Textures/T_HUD_ResourceFrame")))
		Background->SetBrushFromTexture(Texture);
	else Background->SetBrush(FactoryUI::Chamfer(FactoryUI::EPlate::Screen));
	auto* BackgroundSlot = Plate->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	auto* RowSlot = Plate->AddChildToOverlay(Row);
	RowSlot->SetPadding(FMargin(24, 12));
	RowSlot->SetVerticalAlignment(VAlign_Center);
	RowSlot->SetHorizontalAlignment(HAlign_Center);
	UVerticalBox* Resources = WidgetTree->ConstructWidget<UVerticalBox>();
	Row->AddChildToHorizontalBox(Resources)->SetVerticalAlignment(VAlign_Center);
	for (UPlayerResourceBarWidget* Resource : {HealthResource, EnergyResource})
	{
		Resource->RemoveFromParent();
		auto* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(244);
		Size->SetHeightOverride(31);
		auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
		Scale->SetStretch(EStretch::ScaleToFit);
		Size->AddChild(Scale);
		Scale->AddChild(Resource);
		Resources->AddChildToVerticalBox(Size)->SetPadding(FMargin(0, 1));
	}

	UImage* Divider = WidgetTree->ConstructWidget<UImage>();
	Divider->SetBrush(FactoryUI::Plate(FactoryUI::Edge, FLinearColor::Transparent, 0));
	Divider->SetDesiredSizeOverride(FVector2D(1, 60));
	auto* DividerSlot = Row->AddChildToHorizontalBox(Divider);
	DividerSlot->SetPadding(FMargin(9, 0));
	DividerSlot->SetVerticalAlignment(VAlign_Center);
	for (const TCHAR* Name : {TEXT("WeaponSlot1"), TEXT("WeaponSlot2"), TEXT("WeaponSlot3"), TEXT("WeaponSlot4")})
	{
		UPlayerWeaponSlotWidget* Card = Cast<UPlayerWeaponSlotWidget>(WidgetTree->FindWidget(Name));
		if (!Card) return false;
		Card->RemoveFromParent();
		auto* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(49);
		Size->SetHeightOverride(82);
		auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
		Scale->SetStretch(EStretch::ScaleToFit);
		Size->AddChild(Scale);
		Scale->AddChild(Card);
		Row->AddChildToHorizontalBox(Size)->SetPadding(FMargin(2, 0));
	}
	return true;
}

bool RefreshPlayerHUD()
{
 const TCHAR* Paths[] = {TEXT("/Game/Main/UI/HUD/Components/WBP_ResourceBar"), TEXT("/Game/Main/UI/HUD/Components/WBP_WeaponSlot"), TEXT("/Game/Main/UI/HUD/WBP_PlayerUI")};
 UTexture2D* Metal = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/Menu/Textures/T_UI_MenuMetalPanel"));
 if (!Metal) return false;
 for (int32 Index=0; Index<3; ++Index)
 {
  UWidgetBlueprint* BP=LoadObject<UWidgetBlueprint>(nullptr,Paths[Index]);
  if (!BP || !BP->WidgetTree) return false;
  Backup(Paths[Index]);
  UWidgetTree* Tree=BP->WidgetTree;
  Tree->ForEachWidget([&](UWidget* W)
  {
   W->SetVisibility(W->GetVisibility()==ESlateVisibility::Collapsed ? ESlateVisibility::Collapsed : W->GetVisibility()==ESlateVisibility::Hidden ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
   if (UTextBlock* T=Cast<UTextBlock>(W))
   {
    auto Font=T->GetFont(); Font.FontObject=LoadObject<UFont>(nullptr,TEXT("/Game/Main/UI/Fonts/F_TLM_UI")); Font.Size=Index==0 ? 19 : 20; T->SetFont(Font); T->SetColorAndOpacity(FLinearColor(0.94f,0.91f,0.82f));
   }
  });
  if (Index==0)
  {
   if(auto* Bar=Tree->FindWidget<UProgressBar>(TEXT("ValueBar"))) if(auto* Slot=Cast<UOverlaySlot>(Bar->Slot)) Slot->SetPadding(FMargin(26,3));
   if(auto* Size=Tree->FindWidget<USizeBox>(TEXT("ResourceSize"))) {Size->SetWidthOverride(340);Size->SetHeightOverride(40);}
  }
  if (Index==1)
  {
   if(auto* Size=Tree->FindWidget<USizeBox>(TEXT("CardSize"))) {Size->SetWidthOverride(86);Size->SetHeightOverride(156);}
   if(auto* Mouse=Tree->FindWidget<USizeBox>(TEXT("MouseSize"))) {Mouse->SetWidthOverride(22);Mouse->SetHeightOverride(26);}
   if(auto* Size=Tree->FindWidget<USizeBox>(TEXT("IconSize"))) {Size->SetWidthOverride(81);Size->SetHeightOverride(68);}
   if(auto* B=Tree->FindWidget<UBorder>(TEXT("IconBackground"))) {B->SetBrushColor(FLinearColor::Transparent);B->SetPadding(FMargin(3));}
  }
  if(Index==2)
  {
   UWidget* Plate=Tree->FindWidget(TEXT("UnifiedStatusPlate"));
   if(!Plate) return false;
   if(auto* Slot=Cast<UCanvasPanelSlot>(Plate->Slot)) {Slot->SetSize(FVector2D(790,180));Slot->SetPosition(FVector2D(24,-24));}
   UOverlay* Overlay=Cast<UOverlay>(Plate);
   if(!Overlay) return false;
   if(auto* Image=Cast<UImage>(Overlay->GetChildAt(0)))
   {
    FSlateBrush Brush;Brush.SetResourceObject(Metal);Brush.DrawAs=ESlateBrushDrawType::Box;Brush.Margin=FMargin(0.12f);Brush.TintColor=FLinearColor::White;Image->SetBrush(Brush);
   }
   Tree->ForEachWidget([](UWidget* W)
   {
    USizeBox* Size=Cast<USizeBox>(W);
    if(!Size) return;
    UScaleBox* Scale=Cast<UScaleBox>(Size->GetContent());
    UWidget* Content=Scale ? Scale->GetContent() : nullptr;
    if(Cast<UPlayerResourceBarWidget>(Content)){Size->SetWidthOverride(340);Size->SetHeightOverride(44);}
    if(Cast<UPlayerWeaponSlotWidget>(Content)){Size->SetWidthOverride(86);Size->SetHeightOverride(156);}
   });
  }
  if(!CompileSave(BP)) return false;
 }
 UE_LOG(LogTemp,Display,TEXT("TLM_HUD_REFRESH_SAVED"));
 return true;
}

static FAutoConsoleCommand RefreshHUDCommand(TEXT("TLM.UI.RefreshHUD"),TEXT("Apply and save the player HUD presentation"),FConsoleCommandDelegate::CreateLambda([](){RefreshPlayerHUD();}));

bool IntegrateGrappleStatus()
{
 const TCHAR* Path=TEXT("/Game/Main/UI/HUD/WBP_PlayerUI");
 UWidgetBlueprint* BP=LoadObject<UWidgetBlueprint>(nullptr,Path);
 if(!BP || !BP->WidgetTree)return false;
 UWidgetTree* Tree=BP->WidgetTree;
 if(Tree->FindWidget(TEXT("GrappleStatusRow")))return true;
 auto* Health=Tree->FindWidget(TEXT("HealthResource"));
 UWidget* Parent=Health ? Health->GetParent() : nullptr;
 while(Parent && !Cast<UVerticalBox>(Parent))Parent=Parent->GetParent();
 auto* Resources=Cast<UVerticalBox>(Parent);if(!Resources)return false;
 Backup(Path);
 auto* Size=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("GrappleStatusSize"));
 Size->SetWidthOverride(340);Size->SetHeightOverride(32);
 Resources->AddChildToVerticalBox(Size)->SetPadding(FMargin(0,6,0,0));
 auto* Row=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("GrappleStatusRow"));Size->AddChild(Row);
 auto Place=[&](UWidget* W,FVector2D P,FVector2D S)
 {W->SetVisibility(ESlateVisibility::HitTestInvisible);auto* Slot=Row->AddChildToCanvas(W);Slot->SetPosition(P);Slot->SetSize(S);};
 auto* Icon=Tree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("GrappleStatusIcon"));
 Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Main/UI/Cursors/T_Cursor_Grapple")));
 auto Brush=Icon->GetBrush();Brush.SetUVRegion(FBox2f(FVector2f(.10626487f,.14995990f),FVector2f(.86677240f,.90216520f)));Icon->SetBrush(Brush);
 Place(Icon,FVector2D(26,5),FVector2D(22,22));
 auto MakeText=[&](const TCHAR* Name,const TCHAR* Label,int32 FontSize,FVector2D P,FVector2D S)
 {
  auto* Text=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
  Text->SetText(FText::FromString(Label));Text->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr,TEXT("/Game/Main/UI/Fonts/F_TLM_UI")),FontSize,TEXT("Regular")));
  Text->SetColorAndOpacity(FactoryUI::Cyan);Place(Text,P,S);return Text;
 };
 MakeText(TEXT("GrappleStatusKey"),TEXT("CTRL"),12,FVector2D(56,5),FVector2D(42,24));
 auto* State=MakeText(TEXT("GrappleStatusText"),TEXT("READY"),14,FVector2D(260,3),FVector2D(54,26));State->SetJustification(ETextJustify::Right);
 auto* Bar=Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("GrappleStatusProgress"));
 Bar->SetWidgetStyle(FactoryUI::Meter(Bar->GetWidgetStyle()));Bar->SetPercent(1);Bar->SetFillColorAndOpacity(FactoryUI::Cyan);Bar->SetBorderPadding(FVector2D::ZeroVector);
 Place(Bar,FVector2D(108,14),FVector2D(140,4));
 Row->SetVisibility(ESlateVisibility::HitTestInvisible);Size->SetVisibility(ESlateVisibility::HitTestInvisible);
 const bool Result=CompileSave(BP);UE_LOG(LogTemp,Display,TEXT("TLM_HOOK_INSIDE_HUD_SAVED %d"),Result);return Result;
}
static FAutoConsoleCommand IntegrateGrappleHUDCommand(TEXT("TLM.UI.IntegrateGrappleHUD"),TEXT("Add the grapple status inside the existing HUD"),FConsoleCommandDelegate::CreateLambda([](){IntegrateGrappleStatus();}));

bool ApplyMenuArtwork()
{
	UTexture2D* Normal = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/Menu/Textures/T_UI_MenuMetalNormal"));
	UTexture2D* Focused = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/Menu/Textures/T_UI_MenuMetalSelected"));
	UTexture2D* Panel = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/Menu/Textures/T_UI_MenuMetalPanel"));
	if (!Normal || !Focused || !Panel) return false;
	for (const TCHAR* Path : {TEXT("/Game/Main/UI/Menu/WBP_StartMenu"), TEXT("/Game/Main/UI/Menu/WBP_PauseMenu")})
	{
		UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, Path);
		if (!BP || !BP->WidgetTree) return false;
		const bool bPause = BP->WidgetTree->FindWidget(TEXT("PausePages")) != nullptr;
		UWidget* Pages = BP->WidgetTree->FindWidget(bPause ? TEXT("PausePages") : TEXT("Pages"));
		const UCanvasPanelSlot* PageSlot = Pages ? Cast<UCanvasPanelSlot>(Pages->Slot) : nullptr;
		if (!PageSlot) return false;
		if (UTextBlock* ConfirmText = Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("ConfirmText"))))
		{
			ConfirmText->SetAutoWrapText(false);
			ConfirmText->SetWrapTextAt(FMath::Max(1.0, PageSlot->GetSize().X - 32.0));
		}
		if (bPause)
		{
			for (const TCHAR* Name : {TEXT("PausePanelArtwork"), TEXT("PausePages"), TEXT("NavigationHint")})
			{
				UWidget* Control = BP->WidgetTree->FindWidget(Name);
				UCanvasPanelSlot* Slot = Control ? Cast<UCanvasPanelSlot>(Control->Slot) : nullptr;
				if (Slot && Slot->GetAnchors().Minimum == FVector2D::ZeroVector && Slot->GetAnchors().Maximum == FVector2D::ZeroVector)
				{
					const FVector2D Position = Slot->GetPosition();
					Slot->SetAnchors(FAnchors(0.5f));
					Slot->SetPosition(Position - FVector2D(960, 540));
				}
			}
		}
		BP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (UButton* Button = Cast<UButton>(Widget))
			{
				FButtonStyle Style = Button->GetStyle();
				Style.Normal.SetResourceObject(Normal);
				Style.Normal.DrawAs = ESlateBrushDrawType::Image;
				Style.Normal.Margin = FMargin(0);
				Style.Normal.TintColor = FLinearColor::White;
				Style.Disabled = Style.Normal;
				Style.Disabled.TintColor = FLinearColor(0.48f, 0.48f, 0.48f);
				Style.Hovered = Style.Normal;
				Style.Hovered.SetResourceObject(Focused);
				Style.Pressed = Style.Hovered;
				Style.Pressed.TintColor = FLinearColor(0.75f, 0.75f, 0.75f);
				Style.PressedPadding = Style.NormalPadding;
				Button->SetStyle(Style);
			}
			if (Widget->GetFName() == TEXT("PausePanelArtwork"))
			{
				if (UImage* Image = Cast<UImage>(Widget))
				{
					FSlateBrush Brush = Image->GetBrush();
					Brush.SetResourceObject(Panel);
					Brush.DrawAs = ESlateBrushDrawType::Image;
					Brush.Margin = FMargin(0);
					Brush.SetUVRegion(FBox2f(ForceInit));
					Image->SetBrush(Brush);
				}
			}
		});
		if (!CompileSave(BP)) return false;
	}
	return true;
}

bool ApplyFactoryLayout(bool bMenusOnly = false)
{
	if (!bMenusOnly && !SaveFactoryLayout()) return false;
	auto SaveLayout = [](UWidgetBlueprint* BP)
	{
		BP->WidgetTree->ForEachWidget([BP](UWidget* Control)
		{
			if (!BP->WidgetVariableNameToGuidMap.Contains(Control->GetFName()))
			{
				BP->WidgetVariableNameToGuidMap.Add(Control->GetFName(), FGuid::NewGuid());
			}
		});
		return CompileSave(BP);
	};
	const TCHAR* MenuPaths[] = {
		TEXT("/Game/Main/UI/Menu/WBP_StartMenu"),
		TEXT("/Game/Main/UI/Menu/WBP_PauseMenu")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(MenuPaths); ++Index)
	{
		Backup(MenuPaths[Index]);
		UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, MenuPaths[Index]);
		if (!BP || !BP->WidgetTree) return false;
		FactoryMenuStyle::ApplyLayout(BP->WidgetTree, Index == 1);
		if (!SaveLayout(BP)) return false;
	}
	if (bMenusOnly) return true;
	const TCHAR* HUDPath = TEXT("/Game/Main/UI/HUD/WBP_PlayerUI");
	Backup(HUDPath);
	UWidgetBlueprint* HUD = LoadObject<UWidgetBlueprint>(nullptr, HUDPath);
	if (!HUD || !HUD->WidgetTree || !BuildStatusPlate(HUD) || !SaveLayout(HUD)) return false;

	const TCHAR* ResourcePath = TEXT("/Game/Main/UI/HUD/Components/WBP_ResourceBar");
	Backup(ResourcePath);
	UWidgetBlueprint* Resource = LoadObject<UWidgetBlueprint>(nullptr, ResourcePath);
	UWidget* Frame = Resource && Resource->WidgetTree ? Resource->WidgetTree->FindWidget(TEXT("ResourceFrame")) : nullptr;
	if (!Frame) return false;
	Frame->SetVisibility(ESlateVisibility::Hidden);
	if (!CompileSave(Resource)) return false;

	const TCHAR* CoresPath = TEXT("/Game/Test_Jihun/Core/WBP/WBP_OwnedCore");
	Backup(CoresPath);
	UWidgetBlueprint* Cores = LoadObject<UWidgetBlueprint>(nullptr, CoresPath);
	if (!Cores || !Cores->WidgetTree) return false;
	for (const TCHAR* Name : {TEXT("AbilityCoresBox"), TEXT("StatCoresBox")})
	{
		UWrapBox* Box = Cast<UWrapBox>(Cores->WidgetTree->FindWidget(Name));
		if (!Box) return false;
		Box->SetInnerSlotPadding(FVector2D(6, 6));
	}
	return CompileSave(Cores);
}

UTexture2D* MouseTexture(bool bLeft)
{
	const FString Path = bLeft ? TEXT("/Game/Main/UI/Icons/T_UI_MouseLeft") : TEXT("/Game/Main/UI/Icons/T_UI_MouseRight");
	if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *Path)) return Existing;
	UTexture2D* Texture = NewObject<UTexture2D>(CreatePackage(*Path), FName(*FPackageName::GetLongPackageAssetName(Path)), RF_Public | RF_Standalone);
	TArray<FColor> Pixels;
	Pixels.SetNumZeroed(32 * 40);
	for (int32 Y = 0; Y < 40; ++Y)
	{
		for (int32 X = 0; X < 32; ++X)
		{
			const float DY = Y < 13 ? Y - 13.0f : Y > 26 ? Y - 26.0f : 0.0f;
			const float DX = X - 15.5f;
			const bool bInside = DX * DX + DY * DY < 13.0f * 13.0f;
			const bool bInner = DX * DX + DY * DY < 10.8f * 10.8f;
			if (!bInside) continue;
			FColor Color = bInner ? FColor(245, 245, 245) : FColor(15, 18, 24);
			if (bInner && Y < 17 && ((bLeft && X < 15) || (!bLeft && X > 16))) Color = FColor(0, 174, 230);
			if ((Y == 17 || Y == 18 || ((X == 15 || X == 16) && Y < 18)) && bInner) Color = FColor(15, 18, 24);
			Pixels[Y * 32 + X] = Color;
		}
	}
	Texture->Source.Init(32, 40, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
	Texture->CompressionSettings = TC_EditorIcon;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->NeverStream = true;
	Texture->SRGB = true;
	Texture->UpdateResource();
	FAssetRegistryModule::AssetCreated(Texture);
	return Save(Texture) ? Texture : nullptr;
}

bool Build()
{
	UTexture2D* Left = MouseTexture(true);
	UTexture2D* Right = MouseTexture(false);
	if (!Left || !Right) return false;
	UWidgetBlueprint* BarBP = Blueprint(BarPath, UPlayerResourceBarWidget::StaticClass());
	UWidgetTree* Tree = BarBP->WidgetTree;
	USizeBox* BarSize = Widget<USizeBox>(Tree, TEXT("ResourceSize"));
	BarSize->SetWidthOverride(420); BarSize->SetHeightOverride(24);
	Tree->RootWidget = BarSize;
	UOverlay* BarOverlay = Widget<UOverlay>(Tree, TEXT("ResourceOverlay"));
	BarSize->AddChild(BarOverlay);
	UProgressBar* Bar = Widget<UProgressBar>(Tree, TEXT("ValueBar"));
	FProgressBarStyle Style;
	Style.BackgroundImage.DrawAs = ESlateBrushDrawType::Image;
	Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.04f, 0.05f, 0.065f, 0.7f));
	Style.FillImage.DrawAs = ESlateBrushDrawType::Image;
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	Bar->SetBorderPadding(FVector2D::ZeroVector);
	Bar->SetPercent(1);
	BarOverlay->AddChildToOverlay(Bar)->SetHorizontalAlignment(HAlign_Fill);
	UTextBlock* Value = Text(Tree, TEXT("ValueText"), 14);
	Value->SetText(FText::FromString(TEXT("100 / 100")));
	UOverlaySlot* ValueSlot = BarOverlay->AddChildToOverlay(Value);
	ValueSlot->SetHorizontalAlignment(HAlign_Left); ValueSlot->SetVerticalAlignment(VAlign_Center); ValueSlot->SetPadding(FMargin(4, 0));
	if (!CompileSave(BarBP)) return false;

	UWidgetBlueprint* CardBP = Blueprint(CardPath, UPlayerWeaponSlotWidget::StaticClass());
	Tree = CardBP->WidgetTree;
	USizeBox* CardSize = Widget<USizeBox>(Tree, TEXT("CardSize"));
	CardSize->SetWidthOverride(87); CardSize->SetHeightOverride(124);
	Tree->RootWidget = CardSize;
	UVerticalBox* Column = Widget<UVerticalBox>(Tree, TEXT("CardColumn"));
	CardSize->AddChild(Column);
	UTextBlock* Key = Text(Tree, TEXT("KeyLabel"), 15);
	Key->SetText(FText::FromString(TEXT("1"))); Key->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Column->AddChildToVerticalBox(Key)->SetHorizontalAlignment(HAlign_Center);
	UBorder* Frame = Widget<UBorder>(Tree, TEXT("SelectionFrame"));
	Frame->SetPadding(FMargin(2)); Frame->SetBrushColor(FLinearColor(0.035f, 0.04f, 0.05f));
	UVerticalBoxSlot* FrameSlot = Column->AddChildToVerticalBox(Frame);
	FrameSlot->SetPadding(FMargin(0, 1, 0, 0));
	USizeBox* IconSize = Widget<USizeBox>(Tree, TEXT("IconSize"));
	IconSize->SetWidthOverride(81); IconSize->SetHeightOverride(66);
	Frame->AddChild(IconSize);
	UBorder* Background = Widget<UBorder>(Tree, TEXT("IconBackground"));
	Background->SetBrushColor(FLinearColor::White); Background->SetPadding(FMargin(3));
	IconSize->AddChild(Background);
	Background->AddChild(Widget<UImage>(Tree, TEXT("WeaponIcon")));
	USizeBox* MouseSize = Widget<USizeBox>(Tree, TEXT("MouseSize"));
	MouseSize->SetWidthOverride(24); MouseSize->SetHeightOverride(30);
	Column->AddChildToVerticalBox(MouseSize)->SetHorizontalAlignment(HAlign_Center);
	UOverlay* Mouse = Widget<UOverlay>(Tree, TEXT("MouseOverlay"));
	MouseSize->AddChild(Mouse);
	UImage* LeftImage = Widget<UImage>(Tree, TEXT("LeftMouseIcon"));
	LeftImage->SetBrushFromTexture(Left); LeftImage->SetVisibility(ESlateVisibility::Hidden); Mouse->AddChildToOverlay(LeftImage);
	UImage* RightImage = Widget<UImage>(Tree, TEXT("RightMouseIcon"));
	RightImage->SetBrushFromTexture(Right); RightImage->SetVisibility(ESlateVisibility::Hidden); Mouse->AddChildToOverlay(RightImage);
	if (!CompileSave(CardBP)) return false;

	UWidgetBlueprint* PanelBP = Blueprint(PanelPath, UPlayerStatusWidget::StaticClass());
	Tree = PanelBP->WidgetTree;
	UCanvasPanel* Canvas = Widget<UCanvasPanel>(Tree, TEXT("PlayerUIRoot"));
	Tree->RootWidget = Canvas;
	UVerticalBox* Panel = Widget<UVerticalBox>(Tree, TEXT("BottomLeftPanel"));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0, 1)); PanelSlot->SetAlignment(FVector2D(0, 1));
	PanelSlot->SetAutoSize(true); PanelSlot->SetPosition(FVector2D(20, -20));
	UHorizontalBox* Row = Widget<UHorizontalBox>(Tree, TEXT("WeaponRow"));
	Panel->AddChildToVerticalBox(Row);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UPlayerWeaponSlotWidget* Card = NewObject<UPlayerWeaponSlotWidget>(Tree, CardBP->GeneratedClass, FName(*FString::Printf(TEXT("WeaponSlot%d"), Index + 1)), RF_Transactional);
		Card->bIsVariable = true;
		UHorizontalBoxSlot* CardSlot = Row->AddChildToHorizontalBox(Card);
		CardSlot->SetPadding(FMargin(0, 0, Index == 3 ? 0 : 24, 0));
	}
	UPlayerResourceBarWidget* Energy = NewObject<UPlayerResourceBarWidget>(Tree, BarBP->GeneratedClass, TEXT("EnergyResource"), RF_Transactional);
	Energy->bIsVariable = true; Energy->FillColor = FLinearColor(0, 0.35f, 0.75f); Energy->CurrentValue = Energy->MaximumValue = 100;
	Panel->AddChildToVerticalBox(Energy)->SetPadding(FMargin(0, 4, 0, 0));
	UPlayerResourceBarWidget* Health = NewObject<UPlayerResourceBarWidget>(Tree, BarBP->GeneratedClass, TEXT("HealthResource"), RF_Transactional);
	Health->bIsVariable = true; Health->FillColor = FLinearColor(0.9f, 0, 0); Health->CurrentValue = Health->MaximumValue = 100;
	Panel->AddChildToVerticalBox(Health)->SetPadding(FMargin(0, 4, 0, 0));
	if (!CompileSave(PanelBP)) return false;

	const TCHAR* HUDPath = TEXT("/Game/Test_Jihong/UI/BP_PlayerHUD");
	Backup(HUDPath);
	UBlueprint* HUDBP = LoadObject<UBlueprint>(nullptr, HUDPath);
	if (!HUDBP || !HUDBP->GeneratedClass) return false;
	UObject* Defaults = HUDBP->GeneratedClass->GetDefaultObject();
	FindFProperty<FClassProperty>(APlayerHUD::StaticClass(), TEXT("StatusWidgetClass"))->SetObjectPropertyValue_InContainer(Defaults, PanelBP->GeneratedClass);
	FKismetEditorUtilities::CompileBlueprint(HUDBP);
	return HUDBP->Status != BS_Error && Save(HUDBP);
}

bool ResizeExisting()
{
	UWidgetBlueprint* BarBP = LoadObject<UWidgetBlueprint>(nullptr, BarPath);
	UWidgetBlueprint* CardBP = LoadObject<UWidgetBlueprint>(nullptr, CardPath);
	UWidgetBlueprint* PanelBP = LoadObject<UWidgetBlueprint>(nullptr, PanelPath);
	if (!BarBP || !CardBP || !PanelBP) return false;
	USizeBox* BarSize = BarBP->WidgetTree->FindWidget<USizeBox>(TEXT("ResourceSize"));
	UTextBlock* Value = BarBP->WidgetTree->FindWidget<UTextBlock>(TEXT("ValueText"));
	USizeBox* CardSize = CardBP->WidgetTree->FindWidget<USizeBox>(TEXT("CardSize"));
	USizeBox* IconSize = CardBP->WidgetTree->FindWidget<USizeBox>(TEXT("IconSize"));
	USizeBox* MouseSize = CardBP->WidgetTree->FindWidget<USizeBox>(TEXT("MouseSize"));
	UTextBlock* Key = CardBP->WidgetTree->FindWidget<UTextBlock>(TEXT("KeyLabel"));
	UWidget* Panel = PanelBP->WidgetTree->FindWidget(TEXT("BottomLeftPanel"));
	if (!BarSize || !Value || !CardSize || !IconSize || !MouseSize || !Key || !Panel)
	{
		UE_LOG(LogTemp, Error, TEXT("Resize stopped: expected UI controls are missing; existing layout was not rebuilt."));
		return false;
	}
	const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("PlayerUIResizeBackup") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	for (const TCHAR* Path : { BarPath, CardPath, PanelPath, TEXT("/Game/Test_Jihong/UI/BP_PlayerHUD") })
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
		const FString Destination = BackupRoot / FString(Path).RightChop(6).Append(TEXT(".uasset"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true);
		if (IFileManager::Get().Copy(*Destination, *Filename) != COPY_OK) return false;
	}
	BarSize->SetWidthOverride(420); BarSize->SetHeightOverride(24); BarSize->SetRenderScale(FVector2D(1, 1));
	FSlateFontInfo ValueFont = Value->GetFont(); ValueFont.Size = 14; Value->SetFont(ValueFont);
	CardSize->SetWidthOverride(87); CardSize->SetHeightOverride(124); CardSize->SetRenderScale(FVector2D(1, 1));
	IconSize->SetWidthOverride(81); IconSize->SetHeightOverride(66);
	MouseSize->SetWidthOverride(24); MouseSize->SetHeightOverride(30);
	FSlateFontInfo KeyFont = Key->GetFont(); KeyFont.Size = 15; Key->SetFont(KeyFont);
	if (!CompileSave(BarBP) || !CompileSave(CardBP)) return false;
	Panel->SetRenderScale(FVector2D(1, 1));
	PanelBP->WidgetTree->RootWidget->SetRenderScale(FVector2D(1, 1));
	PanelBP->GeneratedClass->GetDefaultObject<UUserWidget>()->SetRenderScale(FVector2D(1, 1));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (UWidget* Card = PanelBP->WidgetTree->FindWidget(FName(*FString::Printf(TEXT("WeaponSlot%d"), Index + 1))))
		{
			Card->SetRenderScale(FVector2D(1, 1));
			if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Card->Slot))
			{
				FMargin Padding = Slot->GetPadding(); Padding.Right = Index == 3 ? 0 : 24; Slot->SetPadding(Padding);
			}
		}
	}
	for (const TCHAR* Name : { TEXT("EnergyResource"), TEXT("HealthResource") })
	{
		if (UWidget* Resource = PanelBP->WidgetTree->FindWidget(Name)) Resource->SetRenderScale(FVector2D(1, 1));
	}
	if (!CompileSave(PanelBP)) return false;
	if (UBlueprint* HUD = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Test_Jihong/UI/BP_PlayerHUD")))
	{
		FKismetEditorUtilities::CompileBlueprint(HUD);
		if (HUD->Status == BS_Error || !Save(HUD)) return false;
	}
	UE_LOG(LogTemp, Display, TEXT("PLAYER_UI_RESIZED: actual dimensions and font sizes enlarged; render scale reset; existing layout preserved."));
	return true;
}

bool BuildSaveNotification()
{
	const TCHAR* Path = TEXT("/Game/Main/UI/Save/WBP_SaveNotification");
	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, Path);
	if (!BP)
	{
		BP = Blueprint(Path, USaveNotificationWidget::StaticClass());
		USaveNotificationWidget::BuildDefaultLayout(BP->WidgetTree);
		BP->WidgetTree->ForEachWidget([](UWidget* Control) { Control->bIsVariable = true; });
		if (!CompileSave(BP)) return false;
	}
	UBlueprint* HUD = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Test_Jihong/UI/BP_PlayerHUD"));
	if (!HUD || !HUD->GeneratedClass) return false;
	FindFProperty<FClassProperty>(APlayerHUD::StaticClass(), TEXT("SaveNotificationClass"))->SetObjectPropertyValue_InContainer(
		HUD->GeneratedClass->GetDefaultObject(), BP->GeneratedClass);
	FKismetEditorUtilities::CompileBlueprint(HUD);
	return HUD->Status != BS_Error && Save(HUD);
}

bool BuildPauseMenu()
{
	const TCHAR* Path = TEXT("/Game/Main/UI/Menu/WBP_PauseMenu");
	if (LoadObject<UWidgetBlueprint>(nullptr, Path)) return true;
	UWidgetBlueprint* BP = Blueprint(Path, UPauseMenuWidget::StaticClass());
	UPauseMenuWidget::BuildDefaultLayout(BP->WidgetTree);
	BP->WidgetTree->ForEachWidget([](UWidget* Control) { Control->bIsVariable = true; });
	return CompileSave(BP);
}

bool BuildHUDResourceBars()
{
	UWidgetBlueprint* Existing = LoadObject<UWidgetBlueprint>(nullptr, BarPath);
	float Width = 600.0f;
	if (Existing)
		if (const USizeBox* Size = Existing->WidgetTree->FindWidget<USizeBox>(TEXT("ResourceSize"))) Width = Size->GetWidthOverride();
	UTexture2D* FrameTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/HUD/Textures/T_HUD_ResourceFrame.T_HUD_ResourceFrame"));
	UTexture2D* FillTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/HUD/Textures/T_HUD_ResourceFill.T_HUD_ResourceFill"));
	if (!FrameTexture || !FillTexture) return false;
	UWidgetBlueprint* BP = Blueprint(BarPath, UPlayerResourceBarWidget::StaticClass());
	UWidgetTree* Tree = BP->WidgetTree;
	USizeBox* Size = Widget<USizeBox>(Tree, TEXT("ResourceSize"));
	Size->SetWidthOverride(Width); Size->SetHeightOverride(46);
	Tree->RootWidget = Size;
	UOverlay* Overlay = Widget<UOverlay>(Tree, TEXT("ResourceOverlay")); Size->AddChild(Overlay);
	UImage* Frame = Widget<UImage>(Tree, TEXT("ResourceFrame")); Frame->SetBrushFromTexture(FrameTexture);
	FSlateBrush FrameBrush = Frame->GetBrush();
	FrameBrush.SetUVRegion(FBox2f(FVector2f(0, 0), FVector2f(1, 1)));
	Frame->SetBrush(FrameBrush);
	UOverlaySlot* FrameSlot = Overlay->AddChildToOverlay(Frame);
	FrameSlot->SetHorizontalAlignment(HAlign_Fill); FrameSlot->SetVerticalAlignment(VAlign_Fill);
	UProgressBar* Bar = Widget<UProgressBar>(Tree, TEXT("ValueBar"));
	FProgressBarStyle Style;
	Style.BackgroundImage.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.FillImage.DrawAs = ESlateBrushDrawType::Image;
	Style.FillImage.SetResourceObject(FillTexture);
	Style.FillImage.SetUVRegion(FBox2f(FVector2f(0.026f, 0.472f), FVector2f(0.974f, 0.529f)));
	Style.FillImage.TintColor = FLinearColor::White;
	Bar->SetWidgetStyle(Style);
	Bar->SetBarFillType(EProgressBarFillType::LeftToRight);
	Bar->SetBarFillStyle(EProgressBarFillStyle::Mask);
	Bar->SetBorderPadding(FVector2D::ZeroVector); Bar->SetPercent(1);
	UOverlaySlot* FillSlot = Overlay->AddChildToOverlay(Bar);
	FillSlot->SetHorizontalAlignment(HAlign_Fill); FillSlot->SetVerticalAlignment(VAlign_Fill);
	FillSlot->SetPadding(FMargin(26, 13));
	UTextBlock* Label = Text(Tree, TEXT("LabelText"), 17); Label->SetText(FText::FromString(TEXT("HP")));
	Label->SetColorAndOpacity(FLinearColor(0.95f, 0.94f, 0.87f));
	Label->SetShadowOffset(FVector2D(1, 1)); Label->SetShadowColorAndOpacity(FLinearColor::Black);
	UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HAlign_Left); LabelSlot->SetVerticalAlignment(VAlign_Center); LabelSlot->SetPadding(FMargin(32, 0));
	UTextBlock* Value = Text(Tree, TEXT("ValueText"), 18); Value->SetText(FText::FromString(TEXT("100 / 100")));
	Value->SetColorAndOpacity(FLinearColor(0.95f, 0.94f, 0.87f));
	Value->SetShadowOffset(FVector2D(1, 1)); Value->SetShadowColorAndOpacity(FLinearColor::Black);
	UOverlaySlot* ValueSlot = Overlay->AddChildToOverlay(Value);
	ValueSlot->SetHorizontalAlignment(HAlign_Right); ValueSlot->SetVerticalAlignment(VAlign_Center); ValueSlot->SetPadding(FMargin(32, 0));
	BP->WidgetVariableNameToGuidMap.Reset();
	Tree->ForEachWidget([BP](UWidget* Control) { Control->bIsVariable = true; BP->WidgetVariableNameToGuidMap.Add(Control->GetFName(), FGuid::NewGuid()); });
	if (!CompileSave(BP)) return false;
	UWidgetBlueprint* Panel = LoadObject<UWidgetBlueprint>(nullptr, PanelPath);
	if (!Panel) return false;
	Backup(PanelPath);
	UPlayerResourceBarWidget* HP = Cast<UPlayerResourceBarWidget>(Panel->WidgetTree->FindWidget(TEXT("HealthResource")));
	UPlayerResourceBarWidget* Energy = Cast<UPlayerResourceBarWidget>(Panel->WidgetTree->FindWidget(TEXT("EnergyResource")));
	if (!HP || !Energy) return false;
	HP->FillColor = FLinearColor(0.85f, 0.055f, 0.025f); HP->ResourceLabel = FText::FromString(TEXT("HP"));
	Energy->FillColor = FLinearColor(0.025f, 0.65f, 0.95f); Energy->ResourceLabel = FText::FromString(TEXT("ENERGY"));
	Energy->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CompileSave(Panel);
}

bool BuildMenuScaffold(bool bPreview)
{
	const TCHAR* Path = TEXT("/Game/Main/UI/Menu/WBP_StartMenu");
	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, Path);
	if (!BP)
	{
		BP = Blueprint(Path, UStartMenuWidget::StaticClass());
		UStartMenuWidget::BuildDefaultLayout(BP->WidgetTree);
		BP->WidgetTree->ForEachWidget([](UWidget* Control) { Control->bIsVariable = true; });
		if (!CompileSave(BP)) return false;
	}
	else
	{
		bool bChanged = UStartMenuWidget::AddConfirmationLayout(BP->WidgetTree);
		bChanged |= UStartMenuWidget::AddCreditsLayout(BP->WidgetTree);
		BP->WidgetTree->ForEachWidget([&](UWidget* Control)
		{
			Control->bIsVariable = true;
			if (!BP->WidgetVariableNameToGuidMap.Contains(Control->GetFName()))
			{
				BP->WidgetVariableNameToGuidMap.Add(Control->GetFName(), FGuid::NewGuid());
				bChanged = true;
			}
		});
		if (bChanged && !CompileSave(BP)) return false;
	}
	const TCHAR* MapPath = TEXT("/Game/Main/Maps/Lv_MainMenu");
	if (!FPackageName::DoesPackageExist(MapPath))
	{
		UPackage* Package = CreatePackage(MapPath);
		UWorld* MenuWorld = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("Lv_MainMenu"), Package);
		MenuWorld->SetFlags(RF_Public | RF_Standalone);
		Package->SetPackageFlags(PKG_ContainsMap);
		MenuWorld->GetWorldSettings()->DefaultGameMode = AStartMenuGameMode::StaticClass();
		FAssetRegistryModule::AssetCreated(MenuWorld);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, MenuWorld,
			*FPackageName::LongPackageNameToFilename(MapPath, FPackageName::GetMapPackageExtension()), Args);
		MenuWorld->DestroyWorld(false);
		if (!bSaved) return false;
	}
	if (!bPreview) return true;
	if (!FApp::CanEverRender()) return false;
	if (!FSlateApplication::IsInitialized())
		FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UStartMenuWidget* Panel = CreateWidget<UStartMenuWidget>(World, BP->GeneratedClass.Get());
	if (!Panel) return false;
	TSharedRef<SWidget> SlateWidget = Panel->TakeWidget();
	UWidgetSwitcher* Pages = Cast<UWidgetSwitcher>(Panel->WidgetTree->FindWidget(TEXT("Pages")));
	UButton* Slot1 = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("Slot1Button")));
	if (!Pages || !Slot1 || Pages->GetActiveWidgetIndex() != 0) return false;
	FTextureCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	auto Render = [&](const TCHAR* Name)
	{
		SlateWidget->SlatePrepass(1.0f);
		FWidgetRenderer Renderer(true);
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(SlateWidget, FVector2D(1280, 720));
		for (int32 Frame = 0; Frame < 3 && Target; ++Frame)
		{
			FlushRenderingCommands();
			Renderer.DrawWidget(Target, SlateWidget, FVector2D(1280, 720), 0.016f, false);
		}
		FlushRenderingCommands();
		FBufferArchive PNG;
		const FString Filename = FPaths::ProjectSavedDir() / TEXT("StartMenuPreview") / Name;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		return Target && FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG) && FFileHelper::SaveArrayToFile(PNG, *Filename);
	};
	bool bOK = Render(TEXT("Main.png"));
	CastChecked<UButton>(Panel->WidgetTree->FindWidget(TEXT("CreditsButton")))->OnClicked.Broadcast();
	bOK = Pages->GetActiveWidget() == Panel->WidgetTree->FindWidget(TEXT("CreditsPage")) && Render(TEXT("Credits.png")) && bOK;
	CastChecked<UButton>(Panel->WidgetTree->FindWidget(TEXT("CreditsBackButton")))->OnClicked.Broadcast();
	bOK = Pages->GetActiveWidgetIndex() == 0 && bOK;
	CastChecked<UButton>(Panel->WidgetTree->FindWidget(TEXT("NewGameButton")))->OnClicked.Broadcast();
	bOK = Pages->GetActiveWidgetIndex() == 1 && Slot1->GetIsEnabled() && Render(TEXT("NewGameSlots.png")) && bOK;
	Slot1->OnClicked.Broadcast();
	bOK = CastChecked<UTextBlock>(Panel->WidgetTree->FindWidget(TEXT("SelectionText")))->GetText().ToString().Contains(TEXT("1")) && bOK;
	CastChecked<UButton>(Panel->WidgetTree->FindWidget(TEXT("BackButton")))->OnClicked.Broadcast();
	bOK = Pages->GetActiveWidgetIndex() == 0 && bOK;
	CastChecked<UButton>(Panel->WidgetTree->FindWidget(TEXT("LoadGameButton")))->OnClicked.Broadcast();
	bOK = Pages->GetActiveWidgetIndex() == 1 && !Slot1->GetIsEnabled() && Render(TEXT("LoadSlots.png")) && bOK;
	Panel->SetSlotPresentation(1, true, FText::FromString(TEXT("2026. 11. 4. 오후 1:52:28")));
	Panel->SetSlotPresentation(2, true, FText::FromString(TEXT("2026. 11. 4. 오전 9:25:41")));
	Panel->SetSlotPresentation(3, true, FText::FromString(TEXT("2026. 10. 30. 오전 11:23:29")));
	bOK = Slot1->GetIsEnabled() && Render(TEXT("LoadSlotsLong.png")) && bOK;
	Panel->ShowNewGameSlots();
	Slot1->OnClicked.Broadcast();
	bOK = Pages->GetActiveWidgetIndex() == 2 && Render(TEXT("OverwriteConfirm.png")) && bOK;
	CastChecked<UButton>(Panel->WidgetTree->FindWidget(TEXT("CancelButton")))->OnClicked.Broadcast();
	bOK = Pages->GetActiveWidgetIndex() == 1 && bOK;
	Slot1->OnClicked.Broadcast();
	CastChecked<UButton>(Panel->WidgetTree->FindWidget(TEXT("ConfirmButton")))->OnClicked.Broadcast();
	bOK = Pages->GetActiveWidgetIndex() == 1 && bOK;
	Panel->SetBusy(true);
	Slot1->OnClicked.Broadcast();
	bOK = Pages->GetActiveWidgetIndex() == 1 && !Slot1->GetIsEnabled() && bOK;
	Panel->SetBusy(false);
	Panel->RemoveFromParent();
	UPauseMenuWidget* PausePanel = CreateWidget<UPauseMenuWidget>(World,
		LoadClass<UPauseMenuWidget>(nullptr, TEXT("/Game/Main/UI/Menu/WBP_PauseMenu.WBP_PauseMenu_C")));
	if (!PausePanel) return false;
	SlateWidget = PausePanel->TakeWidget();
	FTextureCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	PausePanel->SetCheckpointAvailable(false);
	UButton* Reload = CastChecked<UButton>(PausePanel->WidgetTree->FindWidget(TEXT("ReloadButton")));
	bOK = !Reload->GetIsEnabled() && Render(TEXT("Pause.png")) && bOK;
	PausePanel->SetCheckpointAvailable(true);
	Reload->OnClicked.Broadcast();
	UWidgetSwitcher* PausePages = CastChecked<UWidgetSwitcher>(PausePanel->WidgetTree->FindWidget(TEXT("PausePages")));
	bOK = PausePages->GetActiveWidgetIndex() == 1 && Render(TEXT("PauseConfirm.png")) && bOK;
	CastChecked<UButton>(PausePanel->WidgetTree->FindWidget(TEXT("CancelButton")))->OnClicked.Broadcast();
	bOK = PausePages->GetActiveWidgetIndex() == 0 && bOK;
	CastChecked<UButton>(PausePanel->WidgetTree->FindWidget(TEXT("SettingsButton")))->OnClicked.Broadcast();
	bOK = PausePages->GetActiveWidgetIndex() == 2 && Render(TEXT("PauseSettings.png")) && bOK;
	CastChecked<UButton>(PausePanel->WidgetTree->FindWidget(TEXT("SettingsBackButton")))->OnClicked.Broadcast();
	PausePanel->RemoveFromParent();
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	UE_LOG(LogTemp, Display, TEXT("START_MENU_SCAFFOLD: editable widget, menu map, navigation/selection/empty-slot checks: %s"), bOK ? TEXT("PASS") : TEXT("FAIL"));
	return bOK;
}

bool Preview()
{
	if (!FApp::CanEverRender())
	{
		UE_LOG(LogTemp, Error, TEXT("Preview requires -AllowCommandletRendering without -NullRHI."));
		return false;
	}
	if (!FSlateApplication::IsInitialized())
	{
		FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
	}
	UClass* PanelClass = LoadClass<UPlayerStatusWidget>(nullptr, TEXT("/Game/Test_Jihong/UI/WBP_PlayerUI.WBP_PlayerUI_C"));
	UClass* PlayerClass = LoadClass<APlayerBase>(nullptr, TEXT("/Game/Test_Jihong/Blueprints/Player/BP_TestPlayer.BP_TestPlayer_C"));
	if (!PanelClass || !PlayerClass) return false;
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player = World->SpawnActor<APlayerBase>();
	Player->WeaponChoices = PlayerClass->GetDefaultObject<APlayerBase>()->WeaponChoices;
	for (int32 Index = 0; Index < 4; ++Index) Player->AcquireWeaponChoice(Index);
	Player->EquipWeapon(EWeaponSlot::Primary, Player->GetWeaponChoice(0));
	Player->EquipWeapon(EWeaponSlot::Secondary, Player->GetWeaponChoice(2));
	Player->GetCombatAttributes()->SetMaxHealth(100); Player->GetCombatAttributes()->SetHealth(100);
	Player->GetWeaponSlotAttributes()->SetSharedEnergy(100);
	UPlayerStatusWidget* Panel = CreateWidget<UPlayerStatusWidget>(World, PanelClass);
	if (!Panel) return false;
	UE_LOG(LogTemp, Display, TEXT("UI_PREVIEW: widget created"));
	TSharedRef<SWidget> SlateWidget = Panel->TakeWidget();
	UE_LOG(LogTemp, Display, TEXT("UI_PREVIEW: widget rebuilt"));
	Panel->SetObservedPlayer(Player);
	TArray<UTexture2D*> Textures;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (const UWeaponDataAsset* Data = Player->GetWeaponChoice(Index)) Textures.Add(Data->WeaponIcon);
	}
	Textures.Add(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/Icons/T_UI_MouseLeft.T_UI_MouseLeft")));
	Textures.Add(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/Icons/T_UI_MouseRight.T_UI_MouseRight")));
	Textures.Add(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/HUD/Textures/T_HUD_ResourceFrame.T_HUD_ResourceFrame")));
	Textures.Add(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Main/UI/HUD/Textures/T_HUD_ResourceFill.T_HUD_ResourceFill")));
	TArray<UTexture*> CompileTextures;
	for (UTexture2D* Texture : Textures) if (Texture) CompileTextures.Add(Texture);
	FTextureCompilingManager::Get().FinishCompilation(CompileTextures);
	UE_LOG(LogTemp, Display, TEXT("UI_PREVIEW: texture compilation finished"));
	for (UTexture2D* Texture : Textures)
	{
		if (!Texture) continue;
		// The offscreen preview has no gameplay streaming ticks. Load its small UI textures fully.
		Texture->NeverStream = true;
		Texture->UpdateResource();
	}
	FlushRenderingCommands();
	UE_LOG(LogTemp, Display, TEXT("UI_PREVIEW: resources ready"));
	auto Render = [&](const TCHAR* Name)
	{
		Panel->RefreshStatus();
		SlateWidget->SlatePrepass(1.0f);
		FWidgetRenderer Renderer(true);
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(SlateWidget, FVector2D(1280, 720));
		for (int32 Frame = 0; Frame < 3 && Target; ++Frame)
		{
			FlushRenderingCommands();
			Renderer.DrawWidget(Target, SlateWidget, FVector2D(1280, 720), 0.016f, false);
		}
		FlushRenderingCommands();
		FBufferArchive PNG;
		const FString Filename = FPaths::ProjectSavedDir() / TEXT("PlayerUIPreview") / Name;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		return Target && FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG) && FFileHelper::SaveArrayToFile(PNG, *Filename);
	};
	bool bResult = Render(TEXT("AllWeapons.png"));
	Player->OwnedWeaponChoices[1] = false;
	Player->GetWeaponSlotAttributes()->SetSharedEnergy(37);
	Player->GetCombatAttributes()->SetHealth(63);
	bResult = Render(TEXT("MissingWeapon.png")) && bResult;
	Player->ShowWeaponInSlot(EWeaponSlot::Secondary);
	bResult = Render(TEXT("RangedResources.png")) && bResult;
	Player->ShowWeaponInSlot(EWeaponSlot::Primary);
	bResult = Render(TEXT("MeleeResources.png")) && bResult;
	Panel->SetObservedPlayer(nullptr);
	World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return bResult;
}
}
#endif

UIntegratePlayerUICommandlet::UIntegratePlayerUICommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UIntegratePlayerUICommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	if (FParse::Param(*Params, TEXT("AudioSettings")))
	{
		const TCHAR* Path = TEXT("/Game/Main/UI/Menu/WBP_AudioSettings");
		if (UWidgetBlueprint* Existing = LoadObject<UWidgetBlueprint>(nullptr, Path))
		{
			if (!FParse::Param(*Params, TEXT("RestorePanel"))) return 0;
			FactoryMenuStyle::ApplyLayout(Existing->WidgetTree, true);
			return PlayerUIBuild::CompileSave(Existing) ? 0 : 1;
		}
		UWidgetBlueprint* Blueprint = PlayerUIBuild::Blueprint(Path, UAudioSettingsWidget::StaticClass());
		UAudioSettingsWidget::BuildDefaultLayout(Blueprint->WidgetTree);
		FactoryMenuStyle::ApplyLayout(Blueprint->WidgetTree, true);
		return PlayerUIBuild::CompileSave(Blueprint) ? 0 : 1;
	}
#endif
#if WITH_EDITOR
	if (FParse::Param(*Params, TEXT("ApplyFactoryLayout"))) return PlayerUIBuild::ApplyFactoryLayout() ? 0 : 1;
	if (FParse::Param(*Params, TEXT("ApplyFactoryMenus"))) return PlayerUIBuild::ApplyFactoryLayout(true) ? 0 : 1;
	if (FParse::Param(*Params, TEXT("ApplyMenuArtwork"))) return PlayerUIBuild::ApplyMenuArtwork() ? 0 : 1;
	if (FParse::Param(*Params, TEXT("ApplyProjectFonts"))) return PlayerUIBuild::ApplyProjectFonts() ? 0 : 1;
#endif
#if WITH_EDITOR
	if (FParse::Param(*Params, TEXT("MeleeReachHammerMaterial")))
	{
		UMaterial* Source = LoadObject<UMaterial>(nullptr, TEXT("/Game/Test_Jihong/Blueprints/Weapon/Effects/M_HammerTrail_ScifiStable.M_HammerTrail_ScifiStable"));
		if (!Source) return 1;
		UMaterial* Material = DuplicateObject<UMaterial>(Source, CreatePackage(TEXT("/Game/Main/Effects/M_MeleeReach_Hammer")), TEXT("M_MeleeReach_Hammer"));
		Material->SetFlags(RF_Public | RF_Standalone);
		Material->TwoSided = true;
		auto* Tint = NewObject<UMaterialExpressionVectorParameter>(Material);
		Tint->ParameterName = TEXT("ReachColor");
		Tint->DefaultValue = FLinearColor::White;
		for (UMaterialExpression* Expression : Material->GetExpressions())
			for (int32 Index = 0; FExpressionInput* Input = Expression->GetInput(Index); ++Index)
				if (Cast<UMaterialExpressionParticleColor>(Input->Expression)) Input->Expression = Tint;
		Material->GetExpressionCollection().AddExpression(Tint);
		auto* Vertex = NewObject<UMaterialExpressionVertexColor>(Material);
		Material->GetExpressionCollection().AddExpression(Vertex);
		auto* Fade = NewObject<UMaterialExpressionMultiply>(Material);
		Fade->A = Material->GetEditorOnlyData()->Opacity;
		Fade->B.Connect(4, Vertex);
		Material->GetExpressionCollection().AddExpression(Fade);
		Material->GetEditorOnlyData()->Opacity.Connect(0, Fade);
		Material->PostEditChange();
		FAssetRegistryModule::AssetCreated(Material);
		return PlayerUIBuild::Save(Material) ? 0 : 1;
	}
	if (FParse::Param(*Params, TEXT("MeleeReachMaterial")))
	{
		const TCHAR* Path = TEXT("/Game/Main/Effects/M_MeleeReach");
		UMaterial* Material = LoadObject<UMaterial>(nullptr, Path);
		if (!Material)
		{
			Material = NewObject<UMaterial>(CreatePackage(Path), TEXT("M_MeleeReach"), RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Material);
		}
		Material->BlendMode = BLEND_Additive;
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = true;
		Material->GetExpressionCollection().Empty();
		auto* Color = NewObject<UMaterialExpressionVertexColor>(Material);
		Material->GetExpressionCollection().AddExpression(Color);
		auto* Tint = NewObject<UMaterialExpressionVectorParameter>(Material);
		Tint->ParameterName = TEXT("ReachColor");
		Tint->DefaultValue = FLinearColor::White;
		Material->GetExpressionCollection().AddExpression(Tint);
		auto* Multiply = NewObject<UMaterialExpressionMultiply>(Material);
		Multiply->A.Connect(0, Color);
		Multiply->B.Connect(0, Tint);
		Material->GetExpressionCollection().AddExpression(Multiply);
		Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Multiply);
		Material->GetEditorOnlyData()->Opacity.Connect(4, Color);
		Material->PostEditChange();
		return PlayerUIBuild::Save(Material) ? 0 : 1;
	}
	if (FParse::Param(*Params, TEXT("FixResourceFrameUV")))
	{
		UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, PlayerUIBuild::BarPath);
		UImage* Frame = BP && BP->WidgetTree ? Cast<UImage>(BP->WidgetTree->FindWidget(TEXT("ResourceFrame"))) : nullptr;
		if (!Frame) return 1;
		FSlateBrush Brush = Frame->GetBrush();
		Brush.SetUVRegion(FBox2f(FVector2f(0, 0), FVector2f(1, 1)));
		Frame->SetBrush(Brush);
		UWidget* Fill = BP->WidgetTree->FindWidget(TEXT("ValueBar"));
		UWidget* Label = BP->WidgetTree->FindWidget(TEXT("LabelText"));
		UWidget* Value = BP->WidgetTree->FindWidget(TEXT("ValueText"));
		if (!Fill || !Label || !Value) return 1;
		UOverlaySlot* FillSlot = Cast<UOverlaySlot>(Fill->Slot);
		UOverlaySlot* LabelSlot = Cast<UOverlaySlot>(Label->Slot);
		UOverlaySlot* ValueSlot = Cast<UOverlaySlot>(Value->Slot);
		if (!FillSlot || !LabelSlot || !ValueSlot) return 1;
		FillSlot->SetPadding(FMargin(26, 13));
		LabelSlot->SetPadding(FMargin(32, 0));
		ValueSlot->SetPadding(FMargin(32, 0));
		return PlayerUIBuild::CompileSave(BP) ? 0 : 1;
	}
	if (FParse::Param(*Params, TEXT("HUDResources")))
	{
		if (!PlayerUIBuild::BuildHUDResourceBars()) return 1;
		return FParse::Param(*Params, TEXT("Preview")) ? (PlayerUIBuild::Preview() ? 0 : 1) : 0;
	}
	if (FParse::Param(*Params, TEXT("FactoryMenus")))
	{
		// Rebuild only the two menu Widget Blueprints; gameplay HUDs and maps are untouched.
		UWidgetBlueprint* Main = PlayerUIBuild::Blueprint(TEXT("/Game/Main/UI/Menu/WBP_StartMenu"), UStartMenuWidget::StaticClass());
		UStartMenuWidget::BuildDefaultLayout(Main->WidgetTree);
		Main->WidgetVariableNameToGuidMap.Reset();
		Main->WidgetTree->ForEachWidget([Main](UWidget* Control) { Control->bIsVariable = true; Main->WidgetVariableNameToGuidMap.Add(Control->GetFName(), FGuid::NewGuid()); });
		if (!PlayerUIBuild::CompileSave(Main)) return 1;
		UWidgetBlueprint* Pause = PlayerUIBuild::Blueprint(TEXT("/Game/Main/UI/Menu/WBP_PauseMenu"), UPauseMenuWidget::StaticClass());
		UPauseMenuWidget::BuildDefaultLayout(Pause->WidgetTree);
		Pause->WidgetVariableNameToGuidMap.Reset();
		Pause->WidgetTree->ForEachWidget([Pause](UWidget* Control) { Control->bIsVariable = true; Pause->WidgetVariableNameToGuidMap.Add(Control->GetFName(), FGuid::NewGuid()); });
		if (!PlayerUIBuild::CompileSave(Pause)) return 1;
		return FParse::Param(*Params, TEXT("Preview")) ? (PlayerUIBuild::BuildMenuScaffold(true) ? 0 : 1) : 0;
	}
#endif
#if WITH_EDITOR
	if (FParse::Param(*Params, TEXT("PauseMenu"))) return PlayerUIBuild::BuildPauseMenu() ? 0 : 1;
	if (FParse::Param(*Params, TEXT("CheckpointPopup"))) return PlayerUIBuild::BuildSaveNotification() ? 0 : 1;
	if (FParse::Param(*Params, TEXT("MenuScaffold")))
		return PlayerUIBuild::BuildMenuScaffold(FParse::Param(*Params, TEXT("Preview"))) ? 0 : 1;
	if (FParse::Param(*Params, TEXT("Resize")))
	{
		if (!PlayerUIBuild::ResizeExisting()) return 1;
		return FParse::Param(*Params, TEXT("Preview")) ? (PlayerUIBuild::Preview() ? 0 : 1) : 0;
	}
	if (FParse::Param(*Params, TEXT("PreviewOnly"))) return PlayerUIBuild::Preview() ? 0 : 1;
	if (!FParse::Param(*Params, TEXT("Apply")))
	{
		UE_LOG(LogTemp, Error, TEXT("Explicit -Apply is required. This tool rebuilds the editable UI layout; existing assets are backed up first."));
		return 1;
	}
	if (!PlayerUIBuild::Build()) return 1;
	if (FParse::Param(*Params, TEXT("Preview")) && !PlayerUIBuild::Preview()) return 1;
	UE_LOG(LogTemp, Display, TEXT("PLAYER_UI_INTEGRATED: WBP_PlayerUI with editable weapon cards, shared energy and HP bars."));
	return 0;
#else
	return 1;
#endif
}
