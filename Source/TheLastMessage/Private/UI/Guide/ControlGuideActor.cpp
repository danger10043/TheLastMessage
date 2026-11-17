#include "UI/Guide/ControlGuideActor.h"
#include "UI/Guide/ControlGuideWidget.h"

#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"

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

	// UI가 게임 월드 안의 특정 위치에 표시되도록 한다.
	GuideWidgetComponent->SetWidgetSpace(EWidgetSpace::World);

	// UI를 그릴 기본 영역 크기. 월드에서의 크기는 스케일로 조절할 수 있다.
	GuideWidgetComponent->SetDrawSize(FVector2D(600.0f, 300.0f));

	// 앞뒤 양쪽에서 보이도록 한다.
	GuideWidgetComponent->SetTwoSided(true);

	// 안내판 UI가 플레이어나 다른 물체를 막지 않도록 한다.
	GuideWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GuideWidgetComponent->SetGenerateOverlapEvents(false);
}

void AControlGuideActor::BeginPlay()
{
	Super::BeginPlay();

	// 컴포넌트에 지정된 위젯 클래스의 인스턴스를 준비한다.
	GuideWidgetComponent->InitWidget();

	// 생성된 위젯을 조작법 안내 위젯으로 가져온다.
	UControlGuideWidget* GuideWidget =
		Cast<UControlGuideWidget>(
			GuideWidgetComponent->GetUserWidgetObject());

	if (GuideWidget)
	{
		// 이 안내판에서 선택한 안내 종류를 전달한다.
		GuideWidget->SetGuideType(GuideType);
	}
}
