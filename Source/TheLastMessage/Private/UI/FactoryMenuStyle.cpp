#include "UI/FactoryMenuStyle.h"
#include "UI/FactoryUIAssetSettings.h"
#include "UI/FactoryUIStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
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
class SMenuFocusScope : public SBox
{
public:
	virtual TOptional<bool> OnQueryShowFocus(EFocusCause FocusCause) const override
	{
		return false;
	}
};

const FLinearColor Ivory(0.94f, 0.91f, 0.82f);

UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, FVector2D Position, FVector2D Size)
{
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
	Slot->SetPosition(Position); Slot->SetSize(Size);
	return Slot;
}

UImage* Image(UWidgetTree* Tree, FName Name, UTexture2D* Texture)
{
	UImage* Item = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	Item->SetBrushFromTexture(Texture);
	if (Name == TEXT("PausePanelArtwork"))
	{
		FSlateBrush Brush = Item->GetBrush();
		Brush.SetUVRegion(FBox2f(FVector2f(0.046f, 0.048f), FVector2f(0.954f, 0.954f)));
		Item->SetBrush(Brush);
	}
	Item->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Item;
}

}

TSharedRef<SWidget> WithMenuFocusStyle(const TSharedRef<SWidget>& Content)
{
	return SNew(SMenuFocusScope)[Content];
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
	Button->SetStyle(FactoryUI::Button());
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
	const auto* Assets = GetDefault<UFactoryUIAssetSettings>();
	if (!bPause) Place(Canvas, Image(Tree, TEXT("MenuArtwork"), Assets->MenuBackground.LoadSynchronous()), FVector2D::ZeroVector, FVector2D(1920, 1080));
	else Place(Canvas, Image(Tree, TEXT("PausePanelArtwork"), Assets->PausePanel.LoadSynchronous()), FVector2D(616, 80), FVector2D(688, 920));
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
			const bool bFocused = Button->HasAnyUserFocus();
			FButtonStyle Style = Button->GetStyle();
			FSlateBrush Target = bFocused ? Style.Hovered : Style.Disabled;
			// 이미지 버튼은 Disabled 텍스처를 흰색 틴트로 평소 모습에 쓴다. RoundedBox는 틴트가 곧 채우기 색이라 그대로 둔다.
			if (!bFocused && Target.DrawAs != ESlateBrushDrawType::RoundedBox) Target.TintColor = FLinearColor::White;
			if (Style.Normal.GetResourceObject() != Target.GetResourceObject() || Style.Normal.TintColor != Target.TintColor)
			{
				Style.Normal = Target;
				Button->SetStyle(Style);
			}
		}
	});
}

#if WITH_EDITOR
void ApplyLayout(UWidgetTree* Tree, bool bPause)
{
	if (!Tree) return;
	const FVector2D Origin = bPause ? FVector2D(680, 214) : FVector2D(122, 420);
	const FVector2D Size = bPause ? FVector2D(560, 604) : FVector2D(650, 480);
	if (UWidget* Pages = Tree->FindWidget(bPause ? TEXT("PausePages") : TEXT("Pages")))
	{
		if (auto* Slot = Cast<UCanvasPanelSlot>(Pages->Slot))
		{
			Slot->SetPosition(Origin);
			Slot->SetSize(Size);
		}
	}
	if (UTextBlock* Heading = Cast<UTextBlock>(Tree->FindWidget(TEXT("SlotHeading"))))
	{
		auto Font = Heading->GetFont();
		Font.Size = 25;
		Heading->SetFont(Font);
		Heading->SetAutoWrapText(false);
		Heading->SetWrapTextAt(0);
	}
	if (bPause)
	{
		if (UBackgroundBlur* Blur = Cast<UBackgroundBlur>(Tree->FindWidget(TEXT("WorldBlur")))) Blur->SetBlurStrength(7.0f);
		if (UImage* Panel = Cast<UImage>(Tree->FindWidget(TEXT("PausePanelArtwork"))))
		{
			Panel->SetBrush(FactoryUI::Chamfer(FactoryUI::EPlate::Screen));
			if (auto* Slot = Cast<UCanvasPanelSlot>(Panel->Slot))
			{
				Slot->SetPosition(Origin - FVector2D(42, 36));
				Slot->SetSize(Size + FVector2D(84, 90));
			}
		}
		if (UWidget* Hint = Tree->FindWidget(TEXT("NavigationHint")))
			if (auto* Slot = Cast<UCanvasPanelSlot>(Hint->Slot)) Slot->SetPosition(FVector2D(680, 842));
		if (UTextBlock* Heading = Cast<UTextBlock>(Tree->FindWidget(TEXT("Heading"))))
		{
			auto Font = Heading->GetFont();
			Font.Size = 47;
			Heading->SetFont(Font);
			Heading->SetJustification(ETextJustify::Center);
			if (auto* Slot = Cast<UVerticalBoxSlot>(Heading->Slot)) Slot->SetPadding(FMargin(12, 0, 12, 17));
		}
		if (UTextBlock* Caption = Cast<UTextBlock>(Tree->FindWidget(TEXT("PausedLabel"))))
		{
			Caption->SetText(FText::FromString(TEXT("P A U S E D")));
			Caption->SetColorAndOpacity(FactoryUI::Cyan);
			Caption->SetJustification(ETextJustify::Center);
			if (UVerticalBox* Column = Cast<UVerticalBox>(Caption->GetParent())) Column->ShiftChild(0, Caption);
			if (auto* Slot = Cast<UVerticalBoxSlot>(Caption->Slot)) Slot->SetPadding(FMargin(12, 5, 12, 9));
		}
	}
	struct FMenuIcon { const TCHAR* Name; const TCHAR* Asset; };
	static const FMenuIcon Icons[] = {
		{TEXT("NewGameButton"), TEXT("Play")}, {TEXT("LoadGameButton"), TEXT("Folder")},
		{TEXT("ExitButton"), TEXT("Exit")}, {TEXT("ResumeButton"), TEXT("Play")},
		{TEXT("ReloadButton"), TEXT("Retry")}, {TEXT("SettingsButton"), TEXT("Settings")},
		{TEXT("MenuButton"), TEXT("Back")}, {TEXT("QuitButton"), TEXT("Power")},
		{TEXT("BackButton"), TEXT("Back")}, {TEXT("SettingsBackButton"), TEXT("Back")},
		{TEXT("ConfirmButton"), TEXT("Play")}, {TEXT("CancelButton"), TEXT("Back")},
		{TEXT("Slot1Button"), TEXT("Save")}, {TEXT("Slot2Button"), TEXT("Save")}, {TEXT("Slot3Button"), TEXT("Save")}
	};
	TArray<UWidget*> Widgets;
	Tree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		UButton* Button = Cast<UButton>(Widget);
		if (!Button) continue;
		const FName Name = Button->GetFName();
		const bool bStartPrimary = !bPause && (Name == TEXT("NewGameButton") || Name == TEXT("LoadGameButton") || Name == TEXT("ExitButton"));
		Button->SetStyle(FactoryUI::Button());
		Button->SetBackgroundColor(FLinearColor::White);
		if (USizeBox* Box = Cast<USizeBox>(Button->GetParent()))
		{
			Box->SetHeightOverride(bPause ? 72 : bStartPrimary ? 94 : 56);
			if (auto* Slot = Cast<UVerticalBoxSlot>(Box->Slot)) Slot->SetPadding(FMargin(0, bPause ? 8 : bStartPrimary ? 10 : 5));
		}
		UHorizontalBox* Row = Cast<UHorizontalBox>(Button->GetChildAt(0));
		UTextBlock* Label = Cast<UTextBlock>(Row ? Row->GetChildAt(1) : Button->GetChildAt(0));
		UScaleBox* Fit = Row ? Cast<UScaleBox>(Row->GetChildAt(1)) : nullptr;
		if (Fit) Label = Cast<UTextBlock>(Fit->GetChildAt(0));
		if (!Label) continue;
		const FMenuIcon* Icon = nullptr;
		for (const FMenuIcon& Candidate : Icons)
		{
			if (Button->GetFName() == FName(Candidate.Name))
			{
				Icon = &Candidate;
				break;
			}
		}
		if (!Icon) continue;
		auto Font = Label->GetFont();
		Font.Size = bPause ? 28 : bStartPrimary ? 38 : 23;
		Label->SetFont(Font);
		Label->SetAutoWrapText(false);
		Label->SetWrapTextAt(0);
		Label->SetJustification(ETextJustify::Left);
		Label->SetColorAndOpacity(FactoryUI::Text);
		if (!Row)
		{
			Button->RemoveChild(Label);
			Row = Tree->ConstructWidget<UHorizontalBox>();
			auto* RowSlot = CastChecked<UButtonSlot>(Button->AddChild(Row));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetVerticalAlignment(VAlign_Fill);
			RowSlot->SetPadding(FMargin(6, 0, 6, 0));
			auto* IconSlot = Row->AddChildToHorizontalBox(Tree->ConstructWidget<UImage>());
			IconSlot->SetPadding(FMargin(5, 0, 10, 0));
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UImage* Glyph = Cast<UImage>(Row->GetChildAt(0)))
		{
			const auto* Texture = GetDefault<UFactoryUIAssetSettings>()->MenuIcons.Find(FName(Icon->Asset));
			Glyph->SetBrushFromTexture(Texture ? Texture->LoadSynchronous() : nullptr);
			const float IconSize = bPause ? 24 : bStartPrimary ? 29 : 21;
			Glyph->SetDesiredSizeOverride(FVector2D(IconSize));
			Glyph->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (!Fit)
		{
			Label->RemoveFromParent();
			Fit = Tree->ConstructWidget<UScaleBox>();
			Fit->SetStretch(EStretch::ScaleToFit);
			Fit->SetStretchDirection(EStretchDirection::DownOnly);
			auto* FitSlot = CastChecked<UScaleBoxSlot>(Fit->AddChild(Label));
			FitSlot->SetHorizontalAlignment(HAlign_Left);
			FitSlot->SetVerticalAlignment(VAlign_Center);
			auto* LabelSlot = Row->AddChildToHorizontalBox(Fit);
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
}
#endif
}
