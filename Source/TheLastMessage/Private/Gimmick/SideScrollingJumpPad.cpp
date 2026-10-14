// Copyright Epic Games, Inc. All Rights Reserved.


#include "Gimmick/SideScrollingJumpPad.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"

ASideScrollingJumpPad::ASideScrollingJumpPad()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root comp
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the bounding box
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->SetupAttachment(RootComponent);

	// configure the bounding box
	Sphere->SetRelativeLocation(FVector(0.0f, 0.0f, 16.0f));

	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// add the overlap handler
	OnActorBeginOverlap.AddDynamic(this, &ASideScrollingJumpPad::BeginOverlap);
}

void ASideScrollingJumpPad::BeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	// were we overlapped by a character?
	if (ACharacter* OverlappingCharacter = Cast<ACharacter>(OtherActor))
	{
		// force the character to jump
		OverlappingCharacter->Jump();

		// launch the character to override its vertical velocity
		FVector LaunchVelocity = FVector::UpVector * ZStrength;
		OverlappingCharacter->LaunchCharacter(LaunchVelocity, false, true);
	}
}
