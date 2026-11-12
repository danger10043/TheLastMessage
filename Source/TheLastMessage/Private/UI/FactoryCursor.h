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
#include "GameFramework/PlayerController.h"
#include "UI/PlayerHUD.h"
#include "UI/FactoryUIAssetSettings.h"

namespace FactoryCursor
{
// 인게임은 조준 커서 하나만 쓰고, 훅 가능 여부는 안쪽 링 색으로만 구분한다.
// Crosshairs = 훅 가능(청록), EyeDropper = 훅 불가(주황)
enum class EKind : uint8 { Menu, Aim };
inline const FLinearColor AimReadyColor(FColor(0, 242, 242));
inline const FLinearColor AimBlockedColor(FColor(255, 128, 24));
// 0: 메뉴 커서, 1: 훅 가능, 2: 훅 불가. 매 프레임 티커가 갱신한다.
// Slate는 게임 뷰포트가 포커스를 받기 전(클릭 전)까지 Default 커서를 요청하므로,
// Default 커서 위젯도 이 값을 보고 인게임이면 조준 커서를 그린다.
inline uint8 GameplayCursorState = 0;

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
  const auto* Assets = GetDefault<UFactoryUIAssetSettings>();
  if(Kind==EKind::Menu)
  {
   LoadBrush(Assets->MenuCursor,Normal,Brush);
   LoadBrush(Assets->HoverCursor,Hover,HoverBrush);
   Brush.SetUVRegion(FBox2f(FVector2f(0.31036882f,0.19269406f),FVector2f(0.64300626f,0.80273973f)));
   HoverBrush.SetUVRegion(FBox2f(FVector2f(0.20188679f,0.26886792f),FVector2f(0.71792453f,0.74663073f)));
  }
  LoadBrush(Assets->AimBase,AimBase,AimBaseBrush);
  LoadBrush(Assets->AimFill,AimFill,AimFillBrush);
 }
 virtual FVector2D ComputeDesiredSize(float) const override
 {return Kind==EKind::Menu ? FVector2D(64,80) : FVector2D(44,44);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
 {
  const bool bGameplay=GameplayCursorState!=0;
  if(Kind==EKind::Aim || bGameplay)
  {
   // 메뉴 커서 위젯은 64x80 상자에 화살표 끝(32,40)이 핫스팟이므로 조준 중심도 그 점에 맞춘다.
   const bool bAimReady=Kind==EKind::Aim && !bGameplay ? bReady : GameplayCursorState==1;
   const FVector2f Center=Kind==EKind::Menu ? FVector2f(32,40) : FVector2f(22,22);
   // 외곽선·흰 링은 원본 그대로, 안쪽 링(글로우 포함)만 상태 색으로 덧칠한다.
   const FVector2f Size(40,40);
   const FPaintGeometry Geometry=G.ToPaintGeometry(Size,FSlateLayoutTransform(Center-Size*.5f));
   FSlateDrawElement::MakeBox(Out,Layer,Geometry,&AimBaseBrush,ESlateDrawEffect::None,FLinearColor::White);
   FSlateDrawElement::MakeBox(Out,Layer+1,Geometry,&AimFillBrush,ESlateDrawEffect::None,bAimReady ? AimReadyColor : AimBlockedColor);
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
 static void LoadBrush(const TSoftObjectPtr<UTexture2D>& Texture,TStrongObjectPtr<UTexture2D>& Hold,FSlateBrush& Image)
 {
  Hold.Reset(Texture.LoadSynchronous());
  Image.SetResourceObject(Hold.Get());Image.DrawAs=ESlateBrushDrawType::Image;Image.ImageSize=FVector2D(44,44);
 }
 EKind Kind=EKind::Menu;
 bool bReady=true;
 TStrongObjectPtr<UTexture2D> Normal{nullptr},Hover{nullptr},AimBase{nullptr},AimFill{nullptr};
 FSlateBrush Brush,HoverBrush,AimBaseBrush,AimFillBrush;
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

inline bool IsOnScreen(TSharedPtr<SWidget> Widget)
{
 // 부모를 따라 올라가 창에 닿으면 화면에 붙어 있는 위젯이다. 패널에서 제거되면 부모 연결이 끊긴다.
 for(int32 Depth=0;Widget.IsValid() && Depth<256;++Depth,Widget=Widget->GetParentWidget())
 {
  if(Widget->Advanced_IsWindow()) return true;
 }
 return false;
}

inline void UpdateGameplayCursorState(UWorld* World)
{
 const APlayerController* PC=World ? World->GetFirstPlayerController() : nullptr;
 const APlayerHUD* HUD=PC ? Cast<APlayerHUD>(PC->GetHUD()) : nullptr;
 GameplayCursorState=HUD && HUD->ShouldUseGameplayCursor() ? (HUD->ShouldUseGrappleCursor() ? 1 : 2) : 0;
}

inline FTSTicker::FDelegateHandle CursorRefreshTicker;
inline void Install(UGameViewportClient* V)
{
 if(!V)return;V->SetUseSoftwareCursorWidgets(true);
 if(CursorRefreshTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(CursorRefreshTicker);
 CursorRefreshTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak=TWeakObjectPtr<UGameViewportClient>(V)](float)
 {
  if(!Weak.IsValid()) { GameplayCursorState=0; return false; }
  if(!Weak->GetWorld() || !Weak->GetWorld()->IsGameWorld()) GameplayCursorState=0;
  if(Weak->GetWorld() && Weak->GetWorld()->IsGameWorld() && FSlateApplication::IsInitialized())
  {
   // 닫힌 메뉴 위젯은 화면에서 빠져도 살아 있어 포커스를 계속 쥐고 있다(GetUserFocusedWidget이 유효).
   // 포커스 위젯이 없거나 더 이상 어떤 창에도 붙어 있지 않으면 뷰포트가 다시 포커스를 받는다.
   FSlateApplication& Slate=FSlateApplication::Get();
   if(Slate.IsActive() && !IsOnScreen(Slate.GetUserFocusedWidget(0))) FocusGameViewport(Weak.Get());
   UpdateGameplayCursorState(Weak->GetWorld());
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
