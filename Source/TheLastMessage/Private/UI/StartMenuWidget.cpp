#include "UI/StartMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"

namespace
{
UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Box, FName Name, const FText& Label, int32 FontSize)
{
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Text->SetText(Label);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	Text->SetJustification(ETextJustify::Center);
	Text->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(Text)->SetPadding(FMargin(0, 12));
	return Text;
}

void AddButton(UWidgetTree* Tree, UVerticalBox* Box, FName Name, FName TextName, const FText& Label)
{
	USizeBox* Size = Tree->ConstructWidget<USizeBox>();
	Size->SetHeightOverride(72);
	Box->AddChildToVerticalBox(Size)->SetPadding(FMargin(0, 8));
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Size->AddChild(Button);
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
	Text->SetText(Label);
	FSlateFontInfo Font = Text->GetFont(); Font.Size = 22; Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Text->SetJustification(ETextJustify::Center);
	Button->AddChild(Text);
}
}

void UStartMenuWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
	UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	Tree->RootWidget = Canvas;
	UBorder* Background = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.025f, 0.025f, 0.025f, 1));
	UCanvasPanelSlot* BGSlot = Canvas->AddChildToCanvas(Background);
	BGSlot->SetAnchors(FAnchors(0, 0, 1, 1)); BGSlot->SetOffsets(FMargin(0));
	USizeBox* Content = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MenuSize"));
	Content->SetWidthOverride(520); Content->SetHeightOverride(600);
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Content);
	Slot->SetAnchors(FAnchors(0.5f)); Slot->SetAlignment(FVector2D(0.5f)); Slot->SetAutoSize(true);
	UWidgetSwitcher* Switcher = Tree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Pages"));
	Content->AddChild(Switcher);
	UVerticalBox* Main = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPage"));
	Switcher->AddChild(Main);
	AddText(Tree, Main, TEXT("TitleText"), FText::FromString(TEXT("The Last Message")), 36);
	AddButton(Tree, Main, TEXT("NewGameButton"), TEXT("NewGameText"), NSLOCTEXT("StartMenu", "New", "새 게임"));
	AddButton(Tree, Main, TEXT("LoadGameButton"), TEXT("LoadGameText"), NSLOCTEXT("StartMenu", "Load", "불러오기"));
	AddButton(Tree, Main, TEXT("ExitButton"), TEXT("ExitText"), NSLOCTEXT("StartMenu", "Exit", "나가기"));
	UVerticalBox* Slots = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotsPage"));
	Switcher->AddChild(Slots);
	AddText(Tree, Slots, TEXT("SlotHeading"), NSLOCTEXT("StartMenu", "Choose", "슬롯 선택"), 30);
	for (int32 Index = 1; Index <= 3; ++Index)
		AddButton(Tree, Slots, FName(*FString::Printf(TEXT("Slot%dButton"), Index)), FName(*FString::Printf(TEXT("Slot%dText"), Index)),
			FText::Format(NSLOCTEXT("StartMenu", "EmptySlot", "슬롯 {0} · 빈 슬롯"), FText::AsNumber(Index)));
	AddText(Tree, Slots, TEXT("SelectionText"), FText::GetEmpty(), 18);
	AddButton(Tree, Slots, TEXT("BackButton"), TEXT("BackText"), NSLOCTEXT("StartMenu", "Back", "뒤로"));
	AddConfirmationLayout(Tree);
}

bool UStartMenuWidget::AddConfirmationLayout(UWidgetTree* Tree)
{
	UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Tree->FindWidget(TEXT("Pages")));
	if (!Switcher || Tree->FindWidget(TEXT("ConfirmPage"))) return false;
	UVerticalBox* Confirm = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmPage"));
	Switcher->AddChild(Confirm);
	AddText(Tree, Confirm, TEXT("ConfirmHeading"), NSLOCTEXT("StartMenu", "OverwriteTitle", "새 게임 시작"), 30);
	AddText(Tree, Confirm, TEXT("ConfirmText"), NSLOCTEXT("StartMenu", "Overwrite", "기존 진행을 덮어쓰고 새 게임을 시작할까요?"), 22);
	AddButton(Tree, Confirm, TEXT("ConfirmButton"), TEXT("ConfirmButtonText"), NSLOCTEXT("StartMenu", "Confirm", "새 게임 시작"));
	AddButton(Tree, Confirm, TEXT("CancelButton"), TEXT("CancelButtonText"), NSLOCTEXT("StartMenu", "Cancel", "취소"));
	return true;
}

TSharedRef<SWidget> UStartMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout(WidgetTree);
	return Super::RebuildWidget();
}

void UStartMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Resolve the same named controls for native preview and editable Blueprint layouts.
#define MENU_CONTROL(Name, Type) Name = Cast<Type>(WidgetTree->FindWidget(TEXT(#Name)))
	MENU_CONTROL(Pages, UWidgetSwitcher); MENU_CONTROL(NewGameButton, UButton); MENU_CONTROL(LoadGameButton, UButton);
	MENU_CONTROL(ExitButton, UButton); MENU_CONTROL(Slot1Button, UButton); MENU_CONTROL(Slot2Button, UButton);
	MENU_CONTROL(Slot3Button, UButton); MENU_CONTROL(BackButton, UButton); MENU_CONTROL(SlotHeading, UTextBlock);
	MENU_CONTROL(Slot1Text, UTextBlock); MENU_CONTROL(Slot2Text, UTextBlock); MENU_CONTROL(Slot3Text, UTextBlock);
	MENU_CONTROL(SelectionText, UTextBlock);
	MENU_CONTROL(ConfirmText, UTextBlock); MENU_CONTROL(ConfirmButton, UButton); MENU_CONTROL(CancelButton, UButton);
#undef MENU_CONTROL
	if (NewGameButton) NewGameButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ShowNewGameSlots);
	if (LoadGameButton) LoadGameButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ShowLoadGameSlots);
	if (ExitButton) ExitButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::RequestExit);
	if (Slot1Button) Slot1Button->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::SelectSlot1);
	if (Slot2Button) Slot2Button->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::SelectSlot2);
	if (Slot3Button) Slot3Button->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::SelectSlot3);
	if (BackButton) BackButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ShowMainMenu);
	if (ConfirmButton) ConfirmButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ConfirmOverwrite);
	if (CancelButton) CancelButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::CancelOverwrite);
	SetIsFocusable(true);
	ShowMainMenu();
}

void UStartMenuWidget::ShowMainMenu()
{
	if (bWorking) return;
	PendingSlot = 0;
	if (Pages) Pages->SetActiveWidgetIndex(0);
	if (NewGameButton) NewGameButton->SetKeyboardFocus();
}

void UStartMenuWidget::ShowNewGameSlots()
{
	if (bWorking) return;
	bNewGame = true; RefreshSlots();
	if (Pages) Pages->SetActiveWidgetIndex(1);
	if (Slot1Button) Slot1Button->SetKeyboardFocus();
}

void UStartMenuWidget::ShowLoadGameSlots()
{
	if (bWorking) return;
	bNewGame = false; RefreshSlots();
	if (Pages) Pages->SetActiveWidgetIndex(1);
	if (BackButton) BackButton->SetKeyboardFocus();
}

void UStartMenuWidget::SetSlotPresentation(int32 SlotId, bool bOccupied, FText Description, bool bCanLoad)
{
	if (SlotId < 1 || SlotId > 3) return;
	Occupied[SlotId - 1] = bOccupied; Descriptions[SlotId - 1] = Description;
	Loadable[SlotId - 1] = bOccupied && bCanLoad;
	RefreshSlots();
}

void UStartMenuWidget::RefreshSlots()
{
	if (SlotHeading) SlotHeading->SetText(bNewGame ? NSLOCTEXT("StartMenu", "NewSlots", "새 게임 · 슬롯 선택") : NSLOCTEXT("StartMenu", "LoadSlots", "불러오기 · 슬롯 선택"));
	UButton* Buttons[] = { Slot1Button, Slot2Button, Slot3Button };
	UTextBlock* Labels[] = { Slot1Text, Slot2Text, Slot3Text };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Buttons[Index]) Buttons[Index]->SetIsEnabled(!bWorking && (bNewGame || Loadable[Index]));
		const FText Description = Occupied[Index]
			? (Descriptions[Index].IsEmpty() ? NSLOCTEXT("StartMenu", "Saved", "저장된 게임") : Descriptions[Index])
			: NSLOCTEXT("StartMenu", "Empty", "빈 슬롯");
		if (Labels[Index]) Labels[Index]->SetText(FText::Format(NSLOCTEXT("StartMenu", "SlotLabel", "슬롯 {0} · {1}"), FText::AsNumber(Index + 1), Description));
	}
	if (SelectionText) SelectionText->SetText(FText::GetEmpty());
}

void UStartMenuWidget::SelectSlot(int32 SlotId)
{
	if (bWorking || SlotId < 1 || SlotId > 3 || (!bNewGame && !Loadable[SlotId - 1])) return;
	if (bNewGame && Occupied[SlotId - 1])
	{
		PendingSlot = SlotId;
		if (ConfirmText) ConfirmText->SetText(FText::Format(NSLOCTEXT("StartMenu", "OverwriteSlot", "슬롯 {0}의 기존 진행을 덮어쓰고 새 게임을 시작할까요?"), FText::AsNumber(SlotId)));
		if (Pages) Pages->SetActiveWidgetIndex(2);
		if (CancelButton) CancelButton->SetKeyboardFocus();
		return;
	}
	if (SelectionText) SelectionText->SetText(FText::Format(NSLOCTEXT("StartMenu", "Selected", "선택한 슬롯: {0}"), FText::AsNumber(SlotId)));
	OnSlotSelected.Broadcast(SlotId, bNewGame);
}

void UStartMenuWidget::SelectSlot1() { SelectSlot(1); }
void UStartMenuWidget::SelectSlot2() { SelectSlot(2); }
void UStartMenuWidget::SelectSlot3() { SelectSlot(3); }
void UStartMenuWidget::RequestExit() { if (!bWorking) OnExitRequested.Broadcast(); }

void UStartMenuWidget::ConfirmOverwrite()
{
	if (bWorking || PendingSlot < 1 || PendingSlot > 3) return;
	const int32 SelectedSlot = PendingSlot;
	PendingSlot = 0;
	if (Pages) Pages->SetActiveWidgetIndex(1);
	OnSlotSelected.Broadcast(SelectedSlot, true);
}

void UStartMenuWidget::CancelOverwrite()
{
	if (bWorking) return;
	PendingSlot = 0;
	if (Pages) Pages->SetActiveWidgetIndex(1);
	if (Slot1Button) Slot1Button->SetKeyboardFocus();
}

void UStartMenuWidget::SetBusy(bool bBusy)
{
	bWorking = bBusy;
	UButton* Buttons[] = {NewGameButton, LoadGameButton, ExitButton, BackButton, ConfirmButton, CancelButton};
	for (UButton* Button : Buttons) if (Button) Button->SetIsEnabled(!bBusy);
	RefreshSlots();
	if (bBusy && SelectionText) SelectionText->SetText(NSLOCTEXT("StartMenu", "Starting", "준비 중…"));
}

void UStartMenuWidget::ShowError(const FText& Message)
{
	SetBusy(false);
	if (Pages) Pages->SetActiveWidgetIndex(1);
	if (SelectionText) SelectionText->SetText(Message);
}

FReply UStartMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape && Pages)
	{
		if (bWorking) return FReply::Handled();
		if (Pages->GetActiveWidgetIndex() == 2) { CancelOverwrite(); return FReply::Handled(); }
		if (Pages->GetActiveWidgetIndex() == 1) { ShowMainMenu(); return FReply::Handled(); }
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}
