#if WITH_EDITOR
#include "Map/AreaMapComponent.h"
#include "UI/AreaMapWidget.h"
#include "Character/Player/PlayerBase.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

namespace
{
void PreviewAreaMap()
{
	UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !FApp::CanEverRender()) return;
	FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
	APlayerBase* Player=World->SpawnActor<APlayerBase>(APlayerBase::StaticClass(),FTransform::Identity,Params);
	for (TActorIterator<APlayerStart> It(World); It; ++It) { Player->SetActorLocation(It->GetActorLocation()); break; }
	UAreaMapComponent* Map=Player->FindComponentByClass<UAreaMapComponent>();
	Map->bPersistExploration=false;Map->RefreshLayout();
	if (Map->GetRooms().IsEmpty()) {Player->Destroy();return;}
	UAreaMapWidget* Widget=NewObject<UAreaMapWidget>(World);
	Widget->Initialize();Widget->SetMapSource(Map);Widget->SetVisibility(ESlateVisibility::Visible);
	FWidgetRenderer Renderer(false,false);
	for (int32 Pass=0;Pass<2;++Pass)
	{
		const int32 Count=Pass==0 ? FMath::Min(3,Map->GetRooms().Num()) : Map->GetRooms().Num();
		for (int32 Index=0;Index<Count;++Index)
		{
			const FVector2D Center=Map->GetRooms()[Index].Bounds.GetCenter();
			Map->ExploreAt(FVector(Center.X,0,Center.Y));
		}
		const FVector2D Center=Map->GetRooms()[0].Bounds.GetCenter();
		Player->SetActorLocation(FVector(Center.X,0,Center.Y));Map->ExploreAt(Player->GetActorLocation());
		UTextureRenderTarget2D* Target=Renderer.DrawWidget(Widget->TakeWidget(),FVector2D(1400,900));
		FlushRenderingCommands();
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags;ReadFlags.SetLinearToGamma(false);
		if (Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,ReadFlags))
		{
			TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(1400,900,Pixels,Png);
			const FString Stem = World->GetName()==TEXT("Lv_Test") ? TEXT("AreaMapTestPreview") : TEXT("AreaMapPreview");
			FFileHelper::SaveArrayToFile(Png,*(FPaths::ProjectSavedDir()+Stem+(Pass==0 ? TEXT(".png") : TEXT("Explored.png"))));
		}
	}
	Widget->ReleaseSlateResources(true);Player->Destroy();
	UE_LOG(LogTemp,Display,TEXT("AREA_MAP_PREVIEW_SUCCESS"));
}
FAutoConsoleCommand PreviewMapCommand(TEXT("tlm.PreviewAreaMap"),TEXT("Render area map previews"),FConsoleCommandDelegate::CreateStatic(&PreviewAreaMap));
}
#endif
