#include "UI/Guide/ControlGuideWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"

TSharedRef<SWidget> UControlGuideWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}

	if (!WidgetTree->RootWidget)
	{
		BuildGuideLayout();
	}
	else
	{
		GuideImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("ImageGuide")));
	}

	RefreshGuideImage();
	return Super::RebuildWidget();
}

void UControlGuideWidget::BuildGuideLayout()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("GuideRoot"));
	WidgetTree->RootWidget = Root;

	// 안내 그림은 남은 공간을 채우되 원래 비율을 유지한다.
	UScaleBox* GuideScaleBox = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(), TEXT("GuideScaleBox"));
	GuideScaleBox->SetStretch(EStretch::ScaleToFit);
	Root->AddChildToVerticalBox(GuideScaleBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	GuideImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ImageGuide"));
	GuideScaleBox->AddChild(GuideImage);

}

void UControlGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshGuideImage();
	OnGuideTypeChanged(GuideType);
}

void UControlGuideWidget::SetGuideType(EControlGuideType InGuideType)
{
	GuideType = InGuideType;

	// 위젯 블루프린트에 화면 갱신을 요청한다.
	OnGuideTypeChanged(GuideType);
}

void UControlGuideWidget::SetGuideTexture(UTexture2D* InTexture)
{
	GuideTexture = InTexture;
	GuideMaterial = nullptr;
	RefreshGuideImage();
}

void UControlGuideWidget::SetGuideMaterial(UMaterialInterface* InMaterial)
{
	GuideMaterial = InMaterial;
	GuideTexture = nullptr;
	RefreshGuideImage();
}

void UControlGuideWidget::RefreshGuideImage()
{
	if (!GuideImage)
	{
		return;
	}

	if (GuideMaterial)
	{
		GuideImage->SetBrushFromMaterial(GuideMaterial);
		GuideImage->SetDesiredSizeOverride(FVector2D(600.0f, 600.0f));
	}
	else
	{
		GuideImage->SetBrushFromTexture(GuideTexture.Get(), true);
	}
	GuideImage->SetVisibility((GuideTexture || GuideMaterial)
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Hidden);
}
