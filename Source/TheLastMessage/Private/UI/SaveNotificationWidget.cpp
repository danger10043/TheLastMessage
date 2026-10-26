#include "UI/SaveNotificationWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "Engine/World.h"

void USaveNotificationWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	Tree->RootWidget = Root;
	USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NotificationSize"));
	Size->SetWidthOverride(440); Size->SetMinDesiredHeight(76);
	UCanvasPanelSlot* Position = Root->AddChildToCanvas(Size);
	Position->SetAnchors(FAnchors(1, 0)); Position->SetAlignment(FVector2D(1, 0));
	Position->SetPosition(FVector2D(-32, 32)); Position->SetAutoSize(true);
	UBorder* Background = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.04f, 0.95f));
	Background->SetPadding(FMargin(20, 16)); Size->AddChild(Background);
	UTextBlock* Message = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	FSlateFontInfo Font = Message->GetFont(); Font.Size = 22; Message->SetFont(Font);
	Message->SetText(NSLOCTEXT("SaveUI", "Complete", "체크포인트 저장 완료"));
	Message->SetAutoWrapText(true); Message->SetJustification(ETextJustify::Center);
	Background->SetHorizontalAlignment(HAlign_Fill); Background->SetVerticalAlignment(VAlign_Center);
	Background->AddChild(Message);
}

TSharedRef<SWidget> USaveNotificationWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout(WidgetTree);
	return Super::RebuildWidget();
}

void USaveNotificationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	MessageText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MessageText")));
	SetVisibility(ESlateVisibility::Collapsed);
}

void USaveNotificationWidget::ShowResult(bool bSuccess, bool bHasSlot)
{
	if (MessageText)
	{
		MessageText->SetText(bSuccess ? NSLOCTEXT("SaveUI", "Complete", "체크포인트 저장 완료")
			: bHasSlot ? NSLOCTEXT("SaveUI", "Failed", "저장하지 못했습니다. 다시 접촉해 주세요.")
			: NSLOCTEXT("SaveUI", "NoSlot", "시작 메뉴에서 진행 슬롯을 선택해 주세요."));
		MessageText->SetColorAndOpacity(FSlateColor(bSuccess ? FLinearColor::White : FLinearColor(1, 0.45f, 0.3f)));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(HideTimer, this, &USaveNotificationWidget::HideNotification,
		FMath::Max(DisplayDuration, 0.1f), false);
}

void USaveNotificationWidget::HideNotification() { SetVisibility(ESlateVisibility::Collapsed); }

void USaveNotificationWidget::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HideTimer);
	Super::NativeDestruct();
}
