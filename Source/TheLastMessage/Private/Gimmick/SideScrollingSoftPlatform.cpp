// Copyright Epic Games, Inc. All Rights Reserved.


#include "Gimmick/SideScrollingSoftPlatform.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Character/Player/SideScrollingCharacter.h"

ASideScrollingSoftPlatform::ASideScrollingSoftPlatform()
{
 	PrimaryActorTick.bCanEverTick = true;

	// create the root component
	RootComponent = Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the mesh
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);

	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_GameTraceChannel1);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);

	// create the collision check box
	CollisionCheckSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Collision Check Sphere"));
	CollisionCheckSphere->SetupAttachment(Mesh);

	CollisionCheckSphere->SetRelativeLocation(FVector(0.0f, 0.0f, -40.0f));
	CollisionCheckSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionCheckSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionCheckSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionCheckSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// subscribe to the overlap events
	CollisionCheckSphere->OnComponentBeginOverlap.AddDynamic(this, &ASideScrollingSoftPlatform::OnSoftCollisionOverlap);
}

void ASideScrollingSoftPlatform::BeginPlay()
{
	Super::BeginPlay();
	// Keep existing child BP component defaults from reverting this to WorldStatic.
	Mesh->SetCollisionObjectType(ECC_GameTraceChannel1);
}

void ASideScrollingSoftPlatform::OnSoftCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// have we overlapped a character?
	if (ASideScrollingCharacter* Char = Cast<ASideScrollingCharacter>(OtherActor))
	{
		// disable the soft collision channel
		Char->RefreshSoftPlatformCollision();
	}
}

void ASideScrollingSoftPlatform::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	// have we overlapped a character?
	if (ASideScrollingCharacter* Char = Cast<ASideScrollingCharacter>(OtherActor))
	{
		// enable the soft collision channel
		Char->RefreshSoftPlatformCollision();
	}
}
