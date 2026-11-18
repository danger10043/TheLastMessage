#include "UI/Guide/ControlGuideActor.h"
#include "UI/Guide/ControlGuideWidget.h"

#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "UObject/ConstructorHelpers.h"

AControlGuideActor::AControlGuideActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 안내판의 위치와 회전 기준을 만든다.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// UI를 표시할 컴포넌트를 만든다.
	GuideWidgetComponent =
		CreateDefaultSubobject<UWidgetComponent>(TEXT("GuideWidgetComponent"));

	GuideWidgetComponent->SetupAttachment(SceneRoot);
	GuideWidgetComponent->SetWidgetClass(UControlGuideWidget::StaticClass());

	// UI가 게임 월드 안의 특정 위치에 표시되도록 한다.
	GuideWidgetComponent->SetWidgetSpace(EWidgetSpace::World);

	// UI를 그릴 기본 영역 크기. 월드에서의 크기는 스케일로 조절할 수 있다.
	GuideWidgetComponent->SetDrawSize(FVector2D(600.0f, 600.0f));

	// 앞뒤 양쪽에서 보이도록 한다.
	GuideWidgetComponent->SetTwoSided(true);

	// 안내판 UI가 플레이어나 다른 물체를 막지 않도록 한다.
	GuideWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GuideWidgetComponent->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UMediaTexture> PipeTexture(
		TEXT("/Game/Main/UI/Guide/MT_ControlGuide_Pipe.MT_ControlGuide_Pipe"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PipeMaterial(
		TEXT("/Game/Main/UI/Guide/M_UI_ControlGuide_Pipe.M_UI_ControlGuide_Pipe"));
	if (PipeTexture.Succeeded())
	{
		GuideTextures.Add(EControlGuideType::WeaponEquip, PipeTexture.Object);
	}
	// ImgMedia 모듈보다 먼저 실행될 수 있는 생성자에서는 소스를 로드하지 않는다.
	WeaponEquipMediaSource = TSoftObjectPtr<UMediaSource>(FSoftObjectPath(
		TEXT("/Game/Main/UI/Guide/IMS_ControlGuide_Pipe_Playback.IMS_ControlGuide_Pipe_Playback")));
	GuideMediaSources.Add(EControlGuideType::WeaponEquip, WeaponEquipMediaSource);
	GuideMediaSources.Add(EControlGuideType::PushPull, TSoftObjectPtr<UMediaSource>(FSoftObjectPath(
		TEXT("/Game/Main/UI/Guide/IMS_ControlGuide_PushPull.IMS_ControlGuide_PushPull"))));
	GuideMediaSources.Add(EControlGuideType::ChargeAttack, TSoftObjectPtr<UMediaSource>(FSoftObjectPath(
		TEXT("/Game/Main/UI/Guide/IMS_ControlGuide_ChargeHammer.IMS_ControlGuide_ChargeHammer"))));
	GuideMediaSources.Add(EControlGuideType::RifleChargeAttack, TSoftObjectPtr<UMediaSource>(FSoftObjectPath(
		TEXT("/Game/Main/UI/Guide/IMS_ControlGuide_ChargeRifle.IMS_ControlGuide_ChargeRifle"))));
	GuideVideoMaterial = PipeMaterial.Object;
}

void AControlGuideActor::BeginPlay()
{
	Super::BeginPlay();

	// 기존 BP의 Widget Class 설정과 관계없이 C++에서 UI를 생성한다.
	GuideWidgetComponent->SetWidgetClass(UControlGuideWidget::StaticClass());
	GuideWidgetComponent->InitWidget();

	// 생성된 위젯을 조작법 안내 위젯으로 가져온다.
	UControlGuideWidget* GuideWidget =
		Cast<UControlGuideWidget>(
			GuideWidgetComponent->GetUserWidgetObject());

	if (GuideWidget)
	{
		// 이 안내판에서 선택한 안내 종류를 전달한다.
		GuideWidget->SetGuideType(GuideType);

		const TObjectPtr<UTexture>* FoundTexture = GuideTextures.Find(GuideType);
		UTexture* Texture = FoundTexture ? FoundTexture->Get() : nullptr;
		if (UMediaTexture* MediaTexture = Cast<UMediaTexture>(Texture))
		{
			PlayGuideVideo(GuideWidget, MediaTexture);
		}
		else
		{
			GuideWidget->SetGuideTexture(Cast<UTexture2D>(Texture));
		}
	}
}

void AControlGuideActor::PlayGuideVideo(UControlGuideWidget* GuideWidget, UMediaTexture* TextureTemplate)
{
	const TSoftObjectPtr<UMediaSource>* ConfiguredSource = GuideMediaSources.Find(GuideType);
	UMediaSource* MediaSource = ConfiguredSource ? ConfiguredSource->LoadSynchronous() : nullptr;
	if (!MediaSource && GuideType == EControlGuideType::WeaponEquip)
	{
		MediaSource = WeaponEquipMediaSource.LoadSynchronous();
	}
	if (!MediaSource || !GuideVideoMaterial)
	{
		GuideWidget->SetGuideTexture(nullptr);
		UE_LOG(LogTemp, Warning, TEXT("%s: Guide video source or material is missing."), *GetName());
		return;
	}

	// 안내판마다 독립적인 재생기를 사용해서 다른 안내판의 재생에 영향을 주지 않는다.
	GuideMediaPlayer = NewObject<UMediaPlayer>(this);
	GuideMediaPlayer->PlayOnOpen = true;
	GuideMediaPlayer->OnMediaOpened.AddDynamic(this, &AControlGuideActor::HandleMediaOpened);
	GuideMediaPlayer->OnMediaOpenFailed.AddDynamic(this, &AControlGuideActor::HandleMediaOpenFailed);

	GuideMediaTexture = DuplicateObject<UMediaTexture>(TextureTemplate, this);
	GuideMediaTexture->NewStyleOutput = true;
	GuideMediaTexture->AutoClear = true;
	GuideMediaTexture->ClearColor = FLinearColor::Transparent;
	GuideMediaTexture->SetMediaPlayer(GuideMediaPlayer);
	GuideMediaTexture->UpdateResource();

	GuideMediaMaterial = UMaterialInstanceDynamic::Create(GuideVideoMaterial, this);
	GuideMediaMaterial->SetTextureParameterValue(TEXT("GuideVideoTexture"), GuideMediaTexture);
	GuideWidget->SetGuideMaterial(GuideMediaMaterial);

	if (!GuideMediaPlayer->OpenSource(MediaSource))
	{
		HandleMediaOpenFailed(MediaSource->GetUrl());
	}
}

void AControlGuideActor::HandleMediaOpened(FString OpenedUrl)
{
	if (GuideMediaPlayer)
	{
		GuideMediaPlayer->SetLooping(true);
		GuideMediaPlayer->Play();
	}
}

void AControlGuideActor::HandleMediaOpenFailed(FString FailedUrl)
{
	UE_LOG(LogTemp, Warning, TEXT("%s: Could not open guide video: %s"), *GetName(), *FailedUrl);
	if (UControlGuideWidget* GuideWidget = Cast<UControlGuideWidget>(GuideWidgetComponent->GetUserWidgetObject()))
	{
		GuideWidget->SetGuideTexture(nullptr);
	}
}

void AControlGuideActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GuideMediaPlayer)
	{
		GuideMediaPlayer->OnMediaOpened.RemoveAll(this);
		GuideMediaPlayer->OnMediaOpenFailed.RemoveAll(this);
		GuideMediaPlayer->Close();
	}
	if (GuideMediaTexture)
	{
		GuideMediaTexture->SetMediaPlayer(nullptr);
	}
	Super::EndPlay(EndPlayReason);
}
