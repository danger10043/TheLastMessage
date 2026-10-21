#include "UI/AreaMapWidget.h"
#include "Map/AreaMapComponent.h"
#include "Character/Player/PlayerBase.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

bool UAreaMapWidget::OpenMap(UAreaMapComponent* Source, const UInputAction* Action)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->GetPawn() || !Source || UGameplayStatics::IsGamePaused(this)) return false;
	MapSource = Source;
	Source->LoadExploration(); Source->RefreshLayout(); Source->ExploreAt(PC->GetPawn()->GetActorLocation());
	InputOwner = PC; ObservedPawn = PC->GetPawn();
	bPreviousCursor = PC->bShowMouseCursor;
	ToggleKeys.Reset();
	if (const ULocalPlayer* Local = PC->GetLocalPlayer())
		if (const auto* Inputs = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			ToggleKeys = Inputs->QueryKeysMappedToAction(Action);
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	AddToPlayerScreen(100);
	bCapturedInput = true;
	if (APlayerBase* MapPlayer=Cast<APlayerBase>(PC->GetPawn())) MapPlayer->CancelWeaponInputsForUI();
	// 싱글 플레이에서는 지도를 읽는 동안 정지하고 다른 화면이 건 일시정지는 해제하지 않음
	if (GetWorld()->GetNetMode() == NM_Standalone) bPausedByMap = PC->SetPause(true);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode); PC->SetShowMouseCursor(true); PC->FlushPressedKeys();
	SetKeyboardFocus();
	return true;
}

void UAreaMapWidget::RestoreInput()
{
	if (!bCapturedInput) return;
	bCapturedInput = false;
	if (APlayerController* PC = InputOwner.Get(); PC && PC->GetWorld() && !PC->GetWorld()->bIsTearingDown)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode); PC->SetShowMouseCursor(bPreviousCursor); PC->FlushPressedKeys();
		if (bPausedByMap) PC->SetPause(false);
	}
	bPausedByMap = false; InputOwner.Reset(); ObservedPawn.Reset();
}

void UAreaMapWidget::CloseMap()
{
	RestoreInput(); RemoveFromParent();
}

void UAreaMapWidget::NativeDestruct()
{
	RestoreInput();
	Super::NativeDestruct();
}

void UAreaMapWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!bCapturedInput) return;
	const APlayerBase* Player = Cast<APlayerBase>(ObservedPawn.Get());
	if (!Player || Player->IsDead() || !InputOwner.IsValid() || InputOwner->GetPawn() != Player) CloseMap();
}

float UAreaMapWidget::GetMapScale(FVector2D Size) const
{
	if (!MapSource || !MapSource->GetMapBounds().bIsValid) return 1;
	const FVector2D Extent = MapSource->GetMapBounds().GetSize();
	return FMath::Min(FMath::Max(1.0, Size.X-160) / FMath::Max(1.0,Extent.X),
		FMath::Max(1.0, Size.Y-230) / FMath::Max(1.0,Extent.Y)) * Zoom;
}

FVector2D UAreaMapWidget::ToMap(FVector2D World, FVector2D Size) const
{
	const FVector2D Relative = World - MapSource->GetMapBounds().GetCenter();
	return FVector2D(Size.X*0.5,Size.Y*0.5+8) + Pan + FVector2D(Relative.X,-Relative.Y) * GetMapScale(Size);
}

int32 UAreaMapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer = Super::NativePaint(Args,Geometry,Culling,Out,Layer,Style,bEnabled);
	const FVector2D Size = Geometry.GetLocalSize();
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	auto Box = [&](FVector2D Position, FVector2D Dimensions, FLinearColor Color, int32 Depth)
	{
		FSlateDrawElement::MakeBox(Out,Layer+Depth,Geometry.ToPaintGeometry(FVector2f(Dimensions),FSlateLayoutTransform(FVector2f(Position))),White,ESlateDrawEffect::None,Color);
	};
	auto Text = [&](const FText& Value, FVector2D Position, int32 FontSize, FLinearColor Color)
	{
		FSlateDrawElement::MakeText(Out,Layer+8,Geometry.ToPaintGeometry(FVector2f(Size),FSlateLayoutTransform(FVector2f(Position))),Value,
			FCoreStyle::GetDefaultFontStyle("Regular",FontSize),ESlateDrawEffect::None,Color);
	};
	const FLinearColor Cyan(0.22f,0.85f,1.0f);
	Box(FVector2D::ZeroVector,Size,FLinearColor(0.003f,0.009f,0.02f,0.97f),1);
	Box(FVector2D(36,90),FVector2D(FMath::Max(1.0,Size.X-72),1),FLinearColor(0.12f,0.25f,0.32f),2);
	Text(NSLOCTEXT("AreaMap","Title","구역 지도"),FVector2D(40,28),28,Cyan);
	Text(NSLOCTEXT("AreaMap","Controls","휠: 확대  |  드래그: 이동  |  F: 현재 위치  |  지도 키 / ESC: 닫기"),FVector2D(40,Size.Y-42),13,FLinearColor(0.65f,0.76f,0.82f));
	if (!MapSource || !MapSource->GetMapBounds().bIsValid)
	{
		Text(NSLOCTEXT("AreaMap","Empty","표시할 지형이 없습니다"),Size*0.5-FVector2D(90,0),18,FLinearColor::White);
		return Layer+9;
	}
	Text(MapSource->GetCurrentRoomName(),FVector2D(40,66),14,FLinearColor(0.7f,0.8f,0.87f));
	Text(FText::Format(NSLOCTEXT("AreaMap","Progress","탐사 구역  {0} / {1}"),MapSource->GetExploredRoomCount(),MapSource->GetRooms().Num()),
		FVector2D(FMath::Max(40.0,Size.X-240),40),16,FLinearColor::White);
	Out.PushClip(FSlateClippingZone(Geometry.ToPaintGeometry(FVector2f(FMath::Max(1.0,Size.X-72),FMath::Max(1.0,Size.Y-170)),FSlateLayoutTransform(FVector2f(36,104)))));
	for (const FAreaMapRoom& Room : MapSource->GetRooms())
	{
		if (!MapSource->IsExplored(Room.Id)) continue;
		const FVector2D Min = ToMap(FVector2D(Room.Bounds.Min.X,Room.Bounds.Max.Y),Size);
		const FVector2D Max = ToMap(FVector2D(Room.Bounds.Max.X,Room.Bounds.Min.Y),Size);
		Box(Min,Max-Min,FLinearColor(0.025f,0.09f,0.13f),3);
		Out.PushClip(FSlateClippingZone(Geometry.ToPaintGeometry(FVector2f(Max-Min),FSlateLayoutTransform(FVector2f(Min)))));
		for (const FBox2D& Surface : MapSource->GetSurfaces())
		{
			if (!Surface.Intersect(Room.Bounds)) continue;
			const FVector2D A = ToMap(FVector2D(Surface.Min.X,Surface.Max.Y),Size);
			const FVector2D B = ToMap(FVector2D(Surface.Max.X,Surface.Min.Y),Size);
			Box(A,FVector2D(FMath::Max(1.0,B.X-A.X),FMath::Max(2.0,B.Y-A.Y)),FLinearColor(0.25f,0.46f,0.56f),4);
		}
		for (FVector2D Checkpoint : MapSource->GetCheckpoints())
			if (Room.Bounds.IsInside(Checkpoint)) Box(ToMap(Checkpoint,Size)-FVector2D(3,3),FVector2D(6,6),FLinearColor(0.95f,0.7f,0.25f),5);
		Out.PopClip();
	}
	if (const AActor* Player = MapSource->GetOwner())
	{
		const FVector2D Position = ToMap(FVector2D(Player->GetActorLocation().X,Player->GetActorLocation().Z),Size);
		Box(Position-FVector2D(7,7),FVector2D(14,14),FLinearColor(0.015f,0.025f,0.035f),6);
		Box(Position-FVector2D(4,4),FVector2D(8,8),Cyan,7);
		const float Direction = Player->GetActorForwardVector().X >= 0 ? 1 : -1;
		TArray<FVector2D> Arrow = {Position+FVector2D(Direction*8,-4), Position+FVector2D(Direction*12,0), Position+FVector2D(Direction*8,4)};
		FSlateDrawElement::MakeLines(Out,Layer+7,Geometry.ToPaintGeometry(),Arrow,ESlateDrawEffect::None,Cyan,true,2);
	}
	Out.PopClip();
	Text(NSLOCTEXT("AreaMap","Legend","● 현재 위치     ■ 체크포인트     어두운 영역: 미탐사"),FVector2D(40,Size.Y-67),13,FLinearColor(0.65f,0.76f,0.82f));
	return Layer+9;
}

FReply UAreaMapWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey()==EKeys::Escape || ToggleKeys.Contains(Event.GetKey()))
	{
		if (!Event.IsRepeat()) CloseMap();
		return FReply::Handled();
	}
	if (Event.GetKey()==EKeys::F && MapSource && MapSource->GetOwner())
	{
		const FVector Location = MapSource->GetOwner()->GetActorLocation();
		Pan -= ToMap(FVector2D(Location.X,Location.Z),Geometry.GetLocalSize()) - (Geometry.GetLocalSize()*0.5+FVector2D(0,8));
	}
	return FReply::Handled();
}

FReply UAreaMapWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton()==EKeys::LeftMouseButton || Event.GetEffectingButton()==EKeys::RightMouseButton)
	{
		bDragging=true; return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply UAreaMapWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	bDragging=false; return FReply::Handled().ReleaseMouseCapture();
}

FReply UAreaMapWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bDragging) Pan += Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Geometry.AbsoluteToLocal(Event.GetLastScreenSpacePosition());
	return FReply::Handled();
}

FReply UAreaMapWidget::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const float Previous=Zoom;
	Zoom=FMath::Clamp(Zoom*FMath::Pow(1.2f,Event.GetWheelDelta()),1.0f,6.0f);
	const FVector2D PointerPosition=Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const FVector2D Center=Geometry.GetLocalSize()*0.5+FVector2D(0,8);
	Pan=PointerPosition-Center-(PointerPosition-Center-Pan)*(Zoom/Previous);
	return FReply::Handled();
}
