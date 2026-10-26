#include "UI/PauseMenuWidget.h"
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

void UPauseMenuWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	Tree->RootWidget = Root;
	UBorder* Shade = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Shade->SetBrushColor(FLinearColor(0, 0, 0, 0.85f));
	UCanvasPanelSlot* ShadePosition = Root->AddChildToCanvas(Shade);
	ShadePosition->SetAnchors(FAnchors(0, 0, 1, 1)); ShadePosition->SetOffsets(FMargin(0));
	USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MenuSize"));
	Size->SetWidthOverride(540); Size->SetHeightOverride(660);
	UCanvasPanelSlot* Position = Root->AddChildToCanvas(Size);
	Position->SetAnchors(FAnchors(0.5f)); Position->SetAlignment(FVector2D(0.5f)); Position->SetAutoSize(true);
	UWidgetSwitcher* Switcher = Tree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("PausePages"));
	Size->AddChild(Switcher);
	auto Text = [&](UVerticalBox* Box, FName Name, const FText& Label, int32 FontSize)
	{
		UTextBlock* Item = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Item->SetText(Label); Item->SetAutoWrapText(true); Item->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Item->GetFont(); Font.Size = FontSize; Item->SetFont(Font);
		Box->AddChildToVerticalBox(Item)->SetPadding(FMargin(0, 10));
		return Item;
	};
	auto Button = [&](UVerticalBox* Box, FName Name, const FText& Label)
	{
		USizeBox* ButtonSize = Tree->ConstructWidget<USizeBox>(); ButtonSize->SetHeightOverride(64);
		Box->AddChildToVerticalBox(ButtonSize)->SetPadding(FMargin(0, 6));
		UButton* Item = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name); ButtonSize->AddChild(Item);
		UTextBlock* Caption = Tree->ConstructWidget<UTextBlock>(); Caption->SetText(Label);
		FSlateFontInfo Font = Caption->GetFont(); Font.Size = 22; Caption->SetFont(Font);
		Caption->SetColorAndOpacity(FSlateColor(FLinearColor::Black)); Item->AddChild(Caption);
	};
	UVerticalBox* Main = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PausePage")); Switcher->AddChild(Main);
	Text(Main, TEXT("Heading"), NSLOCTEXT("Pause", "Title", "일시정지"), 32);
	Button(Main, TEXT("ResumeButton"), NSLOCTEXT("Pause", "Resume", "재개"));
	Button(Main, TEXT("ReloadButton"), NSLOCTEXT("Pause", "Reload", "마지막 체크포인트 불러오기"));
	Button(Main, TEXT("SettingsButton"), NSLOCTEXT("Pause", "Settings", "설정"));
	Button(Main, TEXT("MenuButton"), NSLOCTEXT("Pause", "Menu", "메뉴로 나가기"));
	Button(Main, TEXT("QuitButton"), NSLOCTEXT("Pause", "Quit", "바탕화면으로 나가기"));
	Text(Main, TEXT("StatusText"), FText::GetEmpty(), 18);
	UVerticalBox* Confirmation = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmationPage")); Switcher->AddChild(Confirmation);
	Text(Confirmation, TEXT("ConfirmHeading"), NSLOCTEXT("Pause", "ConfirmTitle", "확인"), 32);
	Text(Confirmation, TEXT("ConfirmText"), FText::GetEmpty(), 22);
	Button(Confirmation, TEXT("ConfirmButton"), NSLOCTEXT("Pause", "Confirm", "확인"));
	Button(Confirmation, TEXT("CancelButton"), NSLOCTEXT("Pause", "Cancel", "취소"));
	UVerticalBox* Settings = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsPage")); Switcher->AddChild(Settings);
	Text(Settings, TEXT("SettingsHeading"), NSLOCTEXT("Pause", "Settings", "설정"), 32);
	Button(Settings, TEXT("SettingsBackButton"), NSLOCTEXT("Pause", "Back", "뒤로"));
}

TSharedRef<SWidget> UPauseMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout(WidgetTree);
	return Super::RebuildWidget();
}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct(); SetIsFocusable(true);
	Pages = Cast<UWidgetSwitcher>(WidgetTree->FindWidget(TEXT("PausePages")));
#define BIND_PAUSE(Name, Method) if (UButton* Button = Cast<UButton>(WidgetTree->FindWidget(TEXT(Name)))) Button->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::Method)
	BIND_PAUSE("ResumeButton", Resume); BIND_PAUSE("ReloadButton", Reload); BIND_PAUSE("SettingsButton", OpenSettings);
	BIND_PAUSE("MenuButton", MainMenu); BIND_PAUSE("QuitButton", Quit); BIND_PAUSE("ConfirmButton", Confirm);
	BIND_PAUSE("CancelButton", Back); BIND_PAUSE("SettingsBackButton", Back);
#undef BIND_PAUSE
	if (Pages) Pages->SetActiveWidgetIndex(0);
	SetBusy(false);
}

void UPauseMenuWidget::SetCheckpointAvailable(bool bAvailable)
{
	bCheckpointAvailable = bAvailable;
	if (UWidget* Button = WidgetTree->FindWidget(TEXT("ReloadButton"))) Button->SetIsEnabled(bAvailable && !bWorking);
}

void UPauseMenuWidget::NativeDestruct()
{
	OnMenuRemoved.Broadcast();
	Super::NativeDestruct();
}

void UPauseMenuWidget::SetBusy(bool bBusy)
{
	bWorking = bBusy;
	for (const TCHAR* Name : {TEXT("ResumeButton"), TEXT("SettingsButton"), TEXT("MenuButton"), TEXT("QuitButton"), TEXT("ConfirmButton"), TEXT("CancelButton")})
		if (UWidget* Button = WidgetTree->FindWidget(Name)) Button->SetIsEnabled(!bBusy);
	SetCheckpointAvailable(bCheckpointAvailable);
	if (Pages && bBusy) Pages->SetActiveWidgetIndex(0);
	if (bBusy) SetKeyboardFocus();
	if (UTextBlock* Status = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("StatusText"))))
		Status->SetText(bBusy ? NSLOCTEXT("Pause", "Waiting", "저장 중… 완료 후 이동합니다.") : FText::GetEmpty());
}

void UPauseMenuWidget::ShowError(const FText& Message)
{
	SetBusy(false); Back();
	if (UTextBlock* Status = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("StatusText")))) Status->SetText(Message);
}

void UPauseMenuWidget::Resume() { if (!bWorking) OnActionRequested.Broadcast(EPauseMenuAction::Resume); }
void UPauseMenuWidget::Reload()
{
	if (bCheckpointAvailable) Ask(EPauseMenuAction::ReloadCheckpoint, NSLOCTEXT("Pause", "ReloadConfirm", "마지막 체크포인트 이후 진행이 사라집니다. 불러올까요?"));
}
void UPauseMenuWidget::MainMenu() { Ask(EPauseMenuAction::MainMenu, NSLOCTEXT("Pause", "MenuConfirm", "마지막 저장 이후 진행이 사라집니다. 시작 메뉴로 나갈까요?")); }
void UPauseMenuWidget::Quit() { Ask(EPauseMenuAction::QuitGame, NSLOCTEXT("Pause", "QuitConfirm", "마지막 저장 이후 진행이 사라집니다. 게임을 종료할까요?")); }
void UPauseMenuWidget::OpenSettings()
{
	if (bWorking || !Pages) return;
	Pages->SetActiveWidgetIndex(2);
	if (UWidget* Button = WidgetTree->FindWidget(TEXT("SettingsBackButton"))) Button->SetKeyboardFocus();
}
void UPauseMenuWidget::Ask(EPauseMenuAction Action, const FText& Message)
{
	if (bWorking) return;
	PendingAction = Action;
	if (UTextBlock* Text = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ConfirmText")))) Text->SetText(Message);
	if (Pages) Pages->SetActiveWidgetIndex(1);
	if (UWidget* Cancel = WidgetTree->FindWidget(TEXT("CancelButton"))) Cancel->SetKeyboardFocus();
}
void UPauseMenuWidget::Confirm() { if (!bWorking && Pages && Pages->GetActiveWidgetIndex() == 1) OnActionRequested.Broadcast(PendingAction); }
void UPauseMenuWidget::Back()
{
	if (bWorking) return;
	PendingAction = EPauseMenuAction::Resume;
	if (Pages) Pages->SetActiveWidgetIndex(0);
	if (UWidget* Button = WidgetTree->FindWidget(TEXT("ResumeButton"))) Button->SetKeyboardFocus();
}
FReply UPauseMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		if (!Event.IsRepeat() && !bWorking) { if (Pages && Pages->GetActiveWidgetIndex() != 0) Back(); else Resume(); }
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
