#pragma once
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/StrongObjectPtr.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"

namespace FactoryCursor
{
// 인게임은 조준 커서 하나만 쓰고, 훅 가능 여부는 안쪽 링 색으로만 구분한다.
// Crosshairs = 훅 가능(청록), EyeDropper = 훅 불가(주황)
enum class EKind : uint8 { Menu, Aim };
inline const FLinearColor AimReadyColor(FColor(0, 242, 242));
inline const FLinearColor AimBlockedColor(FColor(255, 128, 24));

class SFactoryCursor : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SFactoryCursor) : _Kind(EKind::Menu), _Ready(true) {}
  SLATE_ARGUMENT(EKind,Kind)
  SLATE_ARGUMENT(bool,Ready)
 SLATE_END_ARGS()
 void Construct(const FArguments& Args)
 {
  Kind=Args._Kind;bReady=Args._Ready;SetVisibility(EVisibility::HitTestInvisible);ForceVolatile(true);
  if(Kind==EKind::Menu)
  {
   LoadBrush(TEXT("Menu"),Normal,Brush);
   LoadBrush(TEXT("MenuHover"),Hover,HoverBrush);
  }
  else
  {
   LoadBrush(TEXT("Aim_Base"),Normal,Brush);
   LoadBrush(TEXT("Aim_Fill"),Hover,HoverBrush);
  }
 }
 virtual FVector2D ComputeDesiredSize(float) const override
 {return Kind==EKind::Menu ? FVector2D(64,80) : FVector2D(44,44);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
 {
  if(Kind==EKind::Aim)
  {
   // 외곽선·흰 링은 원본 그대로, 안쪽 링(글로우 포함)만 상태 색으로 덧칠한다.
   const FVector2f Size(40,40);
   const FVector2f Position=(FVector2f(44,44)-Size)*.5f;
   const FPaintGeometry Geometry=G.ToPaintGeometry(Size,FSlateLayoutTransform(Position));
   FSlateDrawElement::MakeBox(Out,Layer,Geometry,&Brush,ESlateDrawEffect::None,FLinearColor::White);
   FSlateDrawElement::MakeBox(Out,Layer+1,Geometry,&HoverBrush,ESlateDrawEffect::None,bReady ? AimReadyColor : AimBlockedColor);
   return Layer+1;
  }
  bool bButton=false;
  const auto Path=FSlateApplication::Get().LocateWindowUnderMouse(FSlateApplication::Get().GetCursorPos(),FSlateApplication::Get().GetInteractiveTopLevelWindows());
  for(int32 Index=0;Index<Path.Widgets.Num();++Index)
  {
   const auto& Widget=Path.Widgets[Index].Widget;const FName Type=Widget->GetType();
   if(Widget->IsEnabled() && (Type==TEXT("SButton") || Type==TEXT("SCheckBox") || Type==TEXT("SComboButton"))) bButton=true;
  }
  const FSlateBrush& Image=bButton && Hover.IsValid() ? HoverBrush : Brush;
  const FVector2f Size(20,27);
  // The menu arrow tip, unlike a centered aiming mark, is the click hotspot.
  const FVector2f Position(32,40);
  const FVector2f Offsets[]={{-1.2f,0},{1.2f,0},{0,-1.2f},{0,1.2f}};
  for(auto Offset:Offsets) FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(Size,FSlateLayoutTransform(Position+Offset)),&Image,ESlateDrawEffect::None,FLinearColor(0,0,0,.9f));
  FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(Size,FSlateLayoutTransform(Position)),&Image,ESlateDrawEffect::None,FLinearColor::White);
  return Layer+1;
 }
private:
 static void LoadBrush(const TCHAR* Name,TStrongObjectPtr<UTexture2D>& Hold,FSlateBrush& Image)
 {
  Hold.Reset(LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/Main/UI/Cursors/T_Cursor_%s.T_Cursor_%s"),Name,Name)));
  Image.SetResourceObject(Hold.Get());Image.DrawAs=ESlateBrushDrawType::Image;Image.ImageSize=FVector2D(44,44);
  if(FCString::Strcmp(Name,TEXT("Menu"))==0) Image.SetUVRegion(FBox2f(FVector2f(0.31036882f,0.19269406f),FVector2f(0.64300626f,0.80273973f)));
  if(FCString::Strcmp(Name,TEXT("MenuHover"))==0) Image.SetUVRegion(FBox2f(FVector2f(0.20188679f,0.26886792f),FVector2f(0.71792453f,0.74663073f)));
 }
 EKind Kind=EKind::Menu;
 bool bReady=true;
 TStrongObjectPtr<UTexture2D> Normal{nullptr},Hover{nullptr};
 FSlateBrush Brush,HoverBrush;
};

// 메뉴/지도 등 UI 위젯이 닫히면서 포커스가 사라지면 GameViewportClient::GetCursor가
// 다음 클릭 전까지 기본(메뉴) 커서를 돌려준다. 포커스를 즉시 게임 뷰포트로 되돌린다.
inline void FocusGameViewport(UGameViewportClient* V)
{
 if(!V || !FSlateApplication::IsInitialized()) return;
 const TSharedPtr<SViewport> ViewportWidget=V->GetGameViewportWidget();
 if(!ViewportWidget.IsValid()) return;
 FSlateApplication::Get().SetAllUserFocus(ViewportWidget,EFocusCause::SetDirectly);
 FSlateApplication::Get().QueryCursor();
}

inline FTSTicker::FDelegateHandle CursorRefreshTicker;
inline void Install(UGameViewportClient* V)
{
 if(!V)return;V->SetUseSoftwareCursorWidgets(true);
 if(CursorRefreshTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(CursorRefreshTicker);
 CursorRefreshTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak=TWeakObjectPtr<UGameViewportClient>(V)](float)
 {
  if(!Weak.IsValid()) return false;
  if(Weak->GetWorld() && Weak->GetWorld()->IsGameWorld() && FSlateApplication::IsInitialized())
  {
   // 포커스를 가진 위젯이 제거되어 아무도 포커스가 없으면 뷰포트가 다시 받도록 한다.
   FSlateApplication& Slate=FSlateApplication::Get();
   if(Slate.IsActive() && !Slate.GetUserFocusedWidget(0).IsValid()) FocusGameViewport(Weak.Get());
   Slate.QueryCursor();
  }
  return true;
 }));
 V->SetSoftwareCursorWidget(EMouseCursor::Default,SNew(SFactoryCursor));
 V->SetSoftwareCursorWidget(EMouseCursor::Hand,SNew(SFactoryCursor));
 V->SetSoftwareCursorWidget(EMouseCursor::GrabHand,SNew(SFactoryCursor));
 V->SetSoftwareCursorWidget(EMouseCursor::GrabHandClosed,SNew(SFactoryCursor));
 V->SetSoftwareCursorWidget(EMouseCursor::Crosshairs,SNew(SFactoryCursor).Kind(EKind::Aim).Ready(true));
 V->SetSoftwareCursorWidget(EMouseCursor::EyeDropper,SNew(SFactoryCursor).Kind(EKind::Aim).Ready(false));
}
}
