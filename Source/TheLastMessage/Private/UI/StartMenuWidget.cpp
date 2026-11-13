#include "UI/StartMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"

#include "UI/FactoryMenuStyle.h"
#include "UI/FactoryUIStyle.h"
#include "UI/FactoryUIAssetSettings.h"
#include "UI/AudioSettingsWidget.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
namespace {
UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Box, FName Name, const FText& Label, int32 FontSize) { return FactoryMenuStyle::AddText(Tree, Box, Name, Label, FontSize); }
void AddButton(UWidgetTree* Tree, UVerticalBox* Box, FName Name, FName TextName, const FText& Label) {
 const bool bCompact = Name != TEXT("NewGameButton") && Name != TEXT("LoadGameButton") && Name != TEXT("ExitButton");
 FactoryMenuStyle::AddButton(Tree, Box, Name, TextName, Label, bCompact);
}

void AddSettingsButton(UWidgetTree* Tree)
{
	auto* Main = Cast<UVerticalBox>(Tree->FindWidget(TEXT("MainPage")));
	auto* Load = Cast<UButton>(Tree->FindWidget(TEXT("LoadGameButton")));
	auto* Exit = Cast<UButton>(Tree->FindWidget(TEXT("ExitButton")));
	if (!Main || !Load || !Exit || Tree->FindWidget(TEXT("StartSettingsButton"))) return;

	auto* LoadSize = Cast<USizeBox>(Load->GetParent());
	UWidget* ExitRow = Exit->GetParent() == Main ? Exit : Exit->GetParent();
	auto* Size = Tree->ConstructWidget<USizeBox>();
	Size->SetHeightOverride(LoadSize ? LoadSize->GetHeightOverride() : 94);
	auto* Slot = Main->AddChildToVerticalBox(Size);
	if (const auto* LoadSlot = LoadSize ? Cast<UVerticalBoxSlot>(LoadSize->Slot) : nullptr)
		Slot->SetPadding(LoadSlot->GetPadding());
	Main->ShiftChild(Main->GetChildIndex(ExitRow), Size);

	auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartSettingsButton"));
	Button->SetStyle(Load->GetStyle());
	Button->SetBackgroundColor(Load->GetBackgroundColor());
	Size->AddChild(Button);
	auto* Row = Tree->ConstructWidget<UHorizontalBox>();
	auto* Content = CastChecked<UButtonSlot>(Button->AddChild(Row));
	Content->SetHorizontalAlignment(HAlign_Fill);
	Content->SetVerticalAlignment(VAlign_Center);
	Content->SetPadding(FMargin(11, 0));
	auto* Icon = Tree->ConstructWidget<UImage>();
	const auto* Assets = GetDefault<UFactoryUIAssetSettings>();
	const auto* Texture = Assets->MenuIcons.Find(TEXT("Settings"));
	Icon->SetBrushFromTexture(Texture ? Texture->LoadSynchronous() : nullptr);
	Icon->SetDesiredSizeOverride(FVector2D(29));
	Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* IconSlot = Row->AddChildToHorizontalBox(Icon);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(FMargin(0, 0, 10, 0));
	auto* Label = Tree->ConstructWidget<UTextBlock>();
	Label->SetText(NSLOCTEXT("StartMenu", "Settings", "설정"));
	auto Font = Label->GetFont();
	if (const auto* LoadLabel = Cast<UTextBlock>(Tree->FindWidget(TEXT("LoadGameText"))))
		Font = LoadLabel->GetFont();
	Font.Size = Assets->SettingsButtonFontSize;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FactoryUI::Text);
	Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	Row->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
}
}
void UStartMenuWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
	UWidgetSwitcher* Switcher = FactoryMenuStyle::BuildShell(Tree, false);
	UVerticalBox* Main = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPage"));
	Switcher->AddChild(Main);
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
	AddCreditsLayout(Tree);
}

bool UStartMenuWidget::AddCreditsLayout(UWidgetTree* Tree)
{
	UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Tree->FindWidget(TEXT("Pages")));
	UVerticalBox* Main = Cast<UVerticalBox>(Tree->FindWidget(TEXT("MainPage")));
	if (!Switcher || !Main || Tree->FindWidget(TEXT("CreditsPage"))) return false;

	UVerticalBox* Credits = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CreditsPage"));
	Switcher->AddChild(Credits);
	UTextBlock* Heading = AddText(Tree, Credits, TEXT("CreditsHeading"), NSLOCTEXT("StartMenu", "Credits", "크레딧"), 32);
	UTextBlock* Notice = AddText(Tree, Credits, TEXT("FontCreditsText"),
		NSLOCTEXT("StartMenu", "FontCredits",
			"사용 글꼴 · SIL Open Font License 1.1\n\n"
			"Orbit · © 2022 The Orbit Project Authors\n"
			"Black Han Sans · © 2015 The Black Han Sans Project Authors\n"
			"Pretendard · © 2021 Kil Hyung-jin\n"
			"Reserved Font Name: Pretendard"), 20);
	AddButton(Tree, Main, TEXT("CreditsButton"), TEXT("CreditsText"), NSLOCTEXT("StartMenu", "Credits", "크레딧"));
	AddButton(Tree, Credits, TEXT("CreditsBackButton"), TEXT("CreditsBackText"), NSLOCTEXT("StartMenu", "Back", "뒤로"));
	const UButton* Back = Cast<UButton>(Tree->FindWidget(TEXT("BackButton")));
	const UTextBlock* Body = Cast<UTextBlock>(Tree->FindWidget(TEXT("ConfirmText")));
	const UTextBlock* Title = Cast<UTextBlock>(Tree->FindWidget(TEXT("SlotHeading")));
	for (const TCHAR* Name : {TEXT("CreditsButton"), TEXT("CreditsBackButton")})
	{
		UButton* Button = CastChecked<UButton>(Tree->FindWidget(Name));
		if (Back) Button->SetStyle(Back->GetStyle());
		if (USizeBox* Size = Cast<USizeBox>(Button->GetParent())) Size->SetHeightOverride(56);
	}
	for (UTextBlock* TextBlock : {Heading, Notice, CastChecked<UTextBlock>(Tree->FindWidget(TEXT("CreditsText"))),
		CastChecked<UTextBlock>(Tree->FindWidget(TEXT("CreditsBackText")))})
	{
		const UTextBlock* Template = TextBlock == Heading ? Title : Body;
		if (!Template) continue;
		FSlateFontInfo Font = Template->GetFont();
		Font.Size = TextBlock == Heading ? 32 : TextBlock == Notice ? 20 : 24;
		TextBlock->SetFont(Font);
	}
	return true;
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
	if (WidgetTree) AddSettingsButton(WidgetTree);
	return FactoryMenuStyle::WithMenuFocusStyle(Super::RebuildWidget());
}

void UStartMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Resolve the same named controls for native preview and editable Blueprint layouts.
#define MENU_CONTROL(Name, Type) Name = Cast<Type>(WidgetTree->FindWidget(TEXT(#Name)))
	MENU_CONTROL(Pages, UWidgetSwitcher); MENU_CONTROL(NewGameButton, UButton); MENU_CONTROL(LoadGameButton, UButton);
	MENU_CONTROL(ExitButton, UButton); MENU_CONTROL(Slot1Button, UButton); MENU_CONTROL(Slot2Button, UButton);
	MENU_CONTROL(CreditsButton, UButton); MENU_CONTROL(CreditsBackButton, UButton);
	MENU_CONTROL(Slot3Button, UButton); MENU_CONTROL(BackButton, UButton); MENU_CONTROL(SlotHeading, UTextBlock);
	MENU_CONTROL(Slot1Text, UTextBlock); MENU_CONTROL(Slot2Text, UTextBlock); MENU_CONTROL(Slot3Text, UTextBlock);
	MENU_CONTROL(SelectionText, UTextBlock);
	MENU_CONTROL(ConfirmText, UTextBlock); MENU_CONTROL(ConfirmButton, UButton); MENU_CONTROL(CancelButton, UButton);
#undef MENU_CONTROL
	SettingsButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("StartSettingsButton")));
	if (SettingsButton) SettingsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenSettings);
	if (NewGameButton) NewGameButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ShowNewGameSlots);
	if (LoadGameButton) LoadGameButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ShowLoadGameSlots);
	if (ExitButton) ExitButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::RequestExit);
	if (CreditsButton) CreditsButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ShowCredits);
	if (CreditsBackButton) CreditsBackButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ShowMainMenu);
	if (Slot1Button) Slot1Button->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::SelectSlot1);
	if (Slot2Button) Slot2Button->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::SelectSlot2);
	if (Slot3Button) Slot3Button->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::SelectSlot3);
	if (BackButton) BackButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ShowMainMenu);
	if (ConfirmButton) ConfirmButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::ConfirmOverwrite);
	if (CancelButton) CancelButton->OnClicked.AddUniqueDynamic(this, &UStartMenuWidget::CancelOverwrite);
	using FactoryUI::ESound;
	UButton* ClickButtons[] = {NewGameButton, LoadGameButton, SettingsButton, ExitButton, CreditsButton};
	for (UButton* Button : ClickButtons) FactoryUI::ApplyButtonSounds(Button, ESound::Click);
	UButton* CancelButtons[] = {BackButton, CreditsBackButton, CancelButton};
	for (UButton* Button : CancelButtons) FactoryUI::ApplyButtonSounds(Button, ESound::Cancel);
	FactoryUI::ApplyButtonSounds(ConfirmButton, ESound::Confirm);
	// Slot presses pick their sound in SelectSlot: an overwrite prompt is a click, starting the game is a confirm.
	UButton* SlotButtons[] = {Slot1Button, Slot2Button, Slot3Button};
	for (UButton* Button : SlotButtons) FactoryUI::ApplyButtonSounds(Button, NullOpt);
	SetIsFocusable(true);
	ShowMainMenu();
}

void UStartMenuWidget::OpenSettings()
{
	if (bWorking || SettingsScreen || !GetOwningPlayer()) return;
	SettingsScreen = UAudioSettingsWidget::CreateFor(GetOwningPlayer());
	if (!SettingsScreen) return;
	SettingsScreen->OnClosed.AddUniqueDynamic(this, &ThisClass::OnSettingsClosed);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SettingsScreen->AddToViewport(100);
	SettingsScreen->FocusFirstControl();
}

void UStartMenuWidget::OnSettingsClosed()
{
	if (SettingsScreen) SettingsScreen->RemoveFromParent();
	SettingsScreen = nullptr;
	SetVisibility(ESlateVisibility::Visible);
	if (SettingsButton) SettingsButton->SetKeyboardFocus();
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
	ForceLayoutPrepass();
	if (Slot1Button) Slot1Button->SetKeyboardFocus();
}

void UStartMenuWidget::ShowCredits()
{
	if (bWorking || !Pages || !CreditsBackButton) return;
	if (UWidget* Credits = WidgetTree->FindWidget(TEXT("CreditsPage")))
	{
		Pages->SetActiveWidget(Credits);
		CreditsBackButton->SetKeyboardFocus();
	}
}

void UStartMenuWidget::ShowLoadGameSlots()
{
	if (bWorking) return;
	bNewGame = false; RefreshSlots();
	if (Pages) Pages->SetActiveWidgetIndex(1);
	ForceLayoutPrepass();
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
		FactoryUI::PlayUISound(FactoryUI::ESound::Click);
		if (ConfirmText) ConfirmText->SetText(FText::Format(NSLOCTEXT("StartMenu", "OverwriteSlot", "슬롯 {0}의 기존 진행을 덮어쓰고 새 게임을 시작할까요?"), FText::AsNumber(SlotId)));
		if (Pages) Pages->SetActiveWidgetIndex(2);
		ForceLayoutPrepass();
		if (CancelButton) CancelButton->SetKeyboardFocus();
		return;
	}
	if (SelectionText) SelectionText->SetText(FText::Format(NSLOCTEXT("StartMenu", "Selected", "선택한 슬롯: {0}"), FText::AsNumber(SlotId)));
	FactoryUI::PlayUISound(FactoryUI::ESound::Confirm);
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
	UButton* Buttons[] = {NewGameButton, LoadGameButton, SettingsButton, ExitButton, BackButton, ConfirmButton, CancelButton, CreditsButton, CreditsBackButton};
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

FReply UStartMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Pages && (Key == EKeys::Up || Key == EKeys::Down))
	{
		if (bWorking) return FReply::Handled();
		TArray<UButton*> Buttons;
		switch (Pages->GetActiveWidgetIndex())
		{
		case 0: Buttons = { NewGameButton, LoadGameButton, SettingsButton, ExitButton, CreditsButton }; break;
		case 1: Buttons = { Slot1Button, Slot2Button, Slot3Button, BackButton }; break;
		case 2: Buttons = { ConfirmButton, CancelButton }; break;
		case 3: Buttons = { CreditsBackButton }; break;
		default: return Super::NativeOnPreviewKeyDown(Geometry, Event);
		}
		int32 Current = INDEX_NONE;
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
			if (Buttons[Index] && Buttons[Index]->HasAnyUserFocus()) { Current = Index; break; }
		const int32 Direction = Key == EKeys::Down ? 1 : -1;
		if (Current == INDEX_NONE) Current = Direction > 0 ? Buttons.Num() - 1 : 0;
		for (int32 Step = 1; Step <= Buttons.Num(); ++Step)
		{
			UButton* Button = Buttons[(Current + Direction * Step + Buttons.Num()) % Buttons.Num()];
			if (Button && Button->GetIsEnabled() && Button->IsVisible())
			{
				if (!Button->HasAnyUserFocus()) FactoryUI::PlayUISound(FactoryUI::ESound::Hover);
				Button->SetKeyboardFocus();
				break;
			}
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UStartMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape && Pages)
	{
		if (bWorking) return FReply::Handled();
		if (Pages->GetActiveWidgetIndex() == 2) { FactoryUI::PlayUISound(FactoryUI::ESound::Cancel); CancelOverwrite(); return FReply::Handled(); }
		if (Pages->GetActiveWidgetIndex() == 1) { FactoryUI::PlayUISound(FactoryUI::ESound::Cancel); ShowMainMenu(); return FReply::Handled(); }
		if (Pages->GetActiveWidget() == WidgetTree->FindWidget(TEXT("CreditsPage"))) { FactoryUI::PlayUISound(FactoryUI::ESound::Cancel); ShowMainMenu(); return FReply::Handled(); }
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

void UStartMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaTime) { Super::NativeTick(Geometry, DeltaTime); FactoryMenuStyle::UpdateFocus(WidgetTree); }
