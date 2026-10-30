#include "UI/FactoryMenuStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"

namespace FactoryMenuStyle
{
namespace
{
const FLinearColor Ivory(0.94f, 0.91f, 0.82f);
const FLinearColor Cyan(0.65f, 0.90f, 1.0f);
const FLinearColor Amber(1.0f, 0.53f, 0.12f);

UTexture2D* Texture(const TCHAR* Name)
{
	return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Main/UI/Menu/Textures/%s.%s"), Name, Name));
}

UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, FVector2D Position, FVector2D Size)
{
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
	Slot->SetPosition(Position); Slot->SetSize(Size);
	return Slot;
}

UImage* Image(UWidgetTree* Tree, FName Name, const TCHAR* Asset)
{
	UImage* Item = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	Item->SetBrushFromTexture(Texture(Asset));
	if (Name == TEXT("PausePanelArtwork"))
	{
		FSlateBrush Brush = Item->GetBrush();
		Brush.SetUVRegion(FBox2f(FVector2f(0.046f, 0.048f), FVector2f(0.954f, 0.954f)));
		Item->SetBrush(Brush);
	}
	Item->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Item;
}

FSlateBrush ButtonBrush()
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture(TEXT("T_Menu_Button")));
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = FVector2D(680, 124);
	// Sample the visible plate only; keep the source PNG and alpha untouched.
	Brush.SetUVRegion(FBox2f(FVector2f(0.019f, 0.210f), FVector2f(0.980f, 0.756f)));
	return Brush;
}
}

UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Box, FName Name, const FText& Label, int32 Size)
{
	UTextBlock* Item = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Item->SetText(Label);
	FSlateFontInfo Font = Item->GetFont(); Font.Size = Size; Item->SetFont(Font);
	Item->SetColorAndOpacity(Ivory);
	Item->SetJustification(ETextJustify::Center);
	Item->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(Item)->SetPadding(FMargin(12, 10));
	return Item;
}

void AddButton(UWidgetTree* Tree, UVerticalBox* Box, FName Name, FName TextName, const FText& Label, bool bCompact)
{
	USizeBox* Size = Tree->ConstructWidget<USizeBox>();
	Size->SetHeightOverride(bCompact ? 88 : 124);
	Box->AddChildToVerticalBox(Size)->SetPadding(FMargin(0, bCompact ? 5 : 9));
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Size->AddChild(Button);
	FButtonStyle Style;
	Style.Normal = ButtonBrush(); Style.Normal.TintColor = Cyan;
	Style.Hovered = Style.Normal; Style.Hovered.TintColor = FLinearColor(0.8f, 1.0f, 1.0f);
	Style.Pressed = Style.Normal; Style.Pressed.TintColor = Amber * 0.75f;
	Style.Disabled = Style.Normal; Style.Disabled.TintColor = FLinearColor(0.24f, 0.27f, 0.29f);
	Style.NormalPadding = FMargin(32, 5); Style.PressedPadding = FMargin(32, 7, 32, 3);
	Button->SetStyle(Style);
	Button->SetBackgroundColor(FLinearColor::White);
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
	Text->SetText(Label);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = (Name.ToString().StartsWith(TEXT("Slot")) || Name == TEXT("ReloadButton")) ? 22 : (bCompact ? 28 : 48);
	Text->SetFont(Font);
	Text->SetColorAndOpacity(Ivory);
	Text->SetJustification(bCompact ? ETextJustify::Center : ETextJustify::Left);
	Text->SetAutoWrapText(true);
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	UButtonSlot* ContentSlot = CastChecked<UButtonSlot>(Button->AddChild(Text));
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetPadding(bCompact ? FMargin(30, 0) : FMargin(94, 0, 70, 0));
}

UWidgetSwitcher* BuildShell(UWidgetTree* Tree, bool bPause)
{
	UOverlay* Root = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	Tree->RootWidget = Root;
	if (bPause)
	{
		UBackgroundBlur* Blur = Tree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("WorldBlur"));
		Blur->SetBlurStrength(3.0f);
		Blur->SetVisibility(ESlateVisibility::HitTestInvisible);
		Root->AddChildToOverlay(Blur)->SetHorizontalAlignment(HAlign_Fill);
	}
	UBorder* Shade = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Shade->SetBrushColor(bPause ? FLinearColor(0.005f, 0.009f, 0.012f, 0.62f) : FLinearColor::Black);
	UOverlaySlot* ShadeSlot = Root->AddChildToOverlay(Shade);
	ShadeSlot->SetHorizontalAlignment(HAlign_Fill); ShadeSlot->SetVerticalAlignment(VAlign_Fill);
	UScaleBox* Scale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ResponsiveScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	UOverlaySlot* ScaleSlot = Root->AddChildToOverlay(Scale);
	ScaleSlot->SetHorizontalAlignment(HAlign_Fill); ScaleSlot->SetVerticalAlignment(VAlign_Fill);
	USizeBox* Design = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DesignResolution"));
	Design->SetWidthOverride(1920); Design->SetHeightOverride(1080); Scale->AddChild(Design);
	UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(); Design->AddChild(Canvas);
	if (!bPause) Place(Canvas, Image(Tree, TEXT("MenuArtwork"), TEXT("T_Menu_Background")), FVector2D::ZeroVector, FVector2D(1920, 1080));
	else Place(Canvas, Image(Tree, TEXT("PausePanelArtwork"), TEXT("T_Menu_PausePanel")), FVector2D(616, 80), FVector2D(688, 920));
	UWidgetSwitcher* Pages = Tree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), bPause ? TEXT("PausePages") : TEXT("Pages"));
	Place(Canvas, Pages, bPause ? FVector2D(680, 157) : FVector2D(122, 420), bPause ? FVector2D(560, 755) : FVector2D(690, 560));
	UTextBlock* Hint = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NavigationHint"));
	Hint->SetText(bPause ? NSLOCTEXT("MenuStyle", "PauseHint", "↑↓ 선택    Enter 확인") : NSLOCTEXT("MenuStyle", "MainHint", "↑↓ 선택    Enter 확인    Esc 뒤로"));
	FSlateFontInfo HintFont = Hint->GetFont(); HintFont.Size = 19; Hint->SetFont(HintFont);
	Hint->SetColorAndOpacity(FLinearColor(0.59f, 0.70f, 0.73f));
	Hint->SetJustification(bPause ? ETextJustify::Center : ETextJustify::Left);
	Place(Canvas, Hint, bPause ? FVector2D(680, 910) : FVector2D(128, 952), FVector2D(bPause ? 560 : 650, 40));
	return Pages;
}

void UpdateFocus(UWidgetTree* Tree)
{
	if (!Tree) return;
	Tree->ForEachWidget([](UWidget* Widget)
	{
		if (UButton* Button = Cast<UButton>(Widget))
		{
			const FLinearColor Tint = Button->HasAnyUserFocus() ? Amber : Cyan;
			if (Button->GetStyle().Normal.TintColor.GetSpecifiedColor() != Tint)
			{
				FButtonStyle Style = Button->GetStyle(); Style.Normal.TintColor = Tint;
				Style.Hovered.TintColor = Button->HasAnyUserFocus() ? Amber : FLinearColor(0.8f, 1.0f, 1.0f);
				Button->SetStyle(Style);
			}
		}
	});
}
}
