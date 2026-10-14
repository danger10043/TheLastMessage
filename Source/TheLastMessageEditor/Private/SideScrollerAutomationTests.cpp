#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Character/Player/RobotPlayerCharacter.h"
#include "Camera/SideScrollingCameraManager.h"
#include "Framework/SideScrollingPlayerController.h"
#include "Framework/SideScrollingGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "UObject/UnrealType.h"
#include "Interfaces/SideScrollerInterop.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSideScrollerSettingsTest, "TheLastMessage.SideScroller.TemplateSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSideScrollerSettingsTest::RunTest(const FString& Parameters)
{
	const UClass* ReferenceClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/_ExternalAssets/ThirdPerson/Variant_SideScroller/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	const UClass* MigratedClass = LoadClass<ARobotPlayerCharacter>(nullptr, TEXT("/Game/Test_Jihong/SideScroller/Blueprints/BP_RobotPlayerCharacter.BP_RobotPlayerCharacter_C"));
	if (!TestNotNull(TEXT("Reference character"), ReferenceClass) || !TestNotNull(TEXT("Migrated character"), MigratedClass)) return false;
	const ACharacter* Reference = CastChecked<ACharacter>(ReferenceClass->GetDefaultObject());
	const ACharacter* Migrated = CastChecked<ACharacter>(MigratedClass->GetDefaultObject());
	const UCharacterMovementComponent* OriginalMovement = Reference->GetCharacterMovement();
	const UCharacterMovementComponent* NewMovement = Migrated->GetCharacterMovement();
	for (const FName Name : {TEXT("GravityScale"), TEXT("JumpZVelocity"), TEXT("MaxWalkSpeed"), TEXT("MaxAcceleration"),
		TEXT("BrakingFrictionFactor"), TEXT("BrakingDecelerationWalking"), TEXT("AirControl"), TEXT("Mass"),
		TEXT("PerchRadiusThreshold"), TEXT("LedgeCheckThreshold"), TEXT("WalkableFloorAngle"), TEXT("MinAnalogWalkSpeed")})
	{
		const FFloatProperty* Property = FindFProperty<FFloatProperty>(NewMovement->GetClass(), Name);
		TestEqual(*Name.ToString(), Property->GetPropertyValue_InContainer(NewMovement), Property->GetPropertyValue_InContainer(OriginalMovement));
	}
	TestEqual(TEXT("Rotation speed"), NewMovement->RotationRate, OriginalMovement->RotationRate);
	TestEqual(TEXT("Plane constraint"), NewMovement->GetPlaneConstraintNormal(), OriginalMovement->GetPlaneConstraintNormal());
	TestEqual(TEXT("Capsule radius"), Migrated->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), Reference->GetCapsuleComponent()->GetUnscaledCapsuleRadius());
	TestEqual(TEXT("Capsule height"), Migrated->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), Reference->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
	TestEqual(TEXT("Jump hold duration"), Migrated->JumpMaxHoldTime, Reference->JumpMaxHoldTime);
	TestEqual(TEXT("Jump count"), Migrated->JumpMaxCount, Reference->JumpMaxCount);
	return !HasAnyErrors();
}

class FSideScrollerPlayChecks : public IAutomationLatentCommand
{
public:
	explicit FSideScrollerPlayChecks(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UWorld* World = GEditor->PlayWorld;
		if (!World) { Test->AddError(TEXT("PIE world did not start.")); return true; }
		APlayerController* Player = World->GetFirstPlayerController();
		ARobotPlayerCharacter* Character = Player ? Cast<ARobotPlayerCharacter>(Player->GetPawn()) : nullptr;
		if (!Character) { Test->AddError(TEXT("Native robot character was not possessed.")); return true; }
		UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player->GetLocalPlayer());
		if (!Input) { Test->AddError(TEXT("Enhanced Input subsystem missing.")); return true; }
		const double Now = World->GetTimeSeconds();
		if (Phase == 0)
		{
			Test->TestTrue(TEXT("Native controller active"), Player->IsA<ASideScrollingPlayerController>());
			Test->TestTrue(TEXT("Native camera active"), Player->PlayerCameraManager->IsA<ASideScrollingCameraManager>());
			Test->TestTrue(TEXT("Native game mode active"), World->GetAuthGameMode()->IsA<ASideScrollingGameMode>());
			MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_ExternalAssets/ThirdPerson/Input/Actions/IA_Move.IA_Move"));
			JumpAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/_ExternalAssets/ThirdPerson/Input/Actions/IA_Jump.IA_Jump"));
			Start = Character->GetActorLocation();
			PhaseTime = Now;
			Phase = 1;
		}
		if (Phase == 1)
		{
			Input->InjectInputForAction(MoveAction, FInputActionValue(FVector2D(0, 1)), {}, {});
			if (Now - PhaseTime < 0.35) return false;
			Test->TestTrue(TEXT("Enhanced Input moves along X"), Character->GetActorLocation().X > Start.X + 10);
			Test->TestTrue(TEXT("Depth remains locked"), FMath::IsNearlyEqual(Character->GetActorLocation().Y, Start.Y, 0.1));
			Input->InjectInputForAction(MoveAction, FInputActionValue(FVector2D::ZeroVector), {}, {});
			Start = Character->GetActorLocation();
			Input->InjectInputForAction(JumpAction, FInputActionValue(true), {}, {});
			PhaseTime = Now;
			Phase = 2;
			return false;
		}
		if (Phase == 2)
		{
			if (Now - PhaseTime < 0.2) return false;
			Test->TestTrue(TEXT("Enhanced Input jump rises"), Character->GetActorLocation().Z > Start.Z + 10);
			Input->InjectInputForAction(JumpAction, FInputActionValue(false), {}, {});
			Phase = 3;
			return false;
		}
		if (Phase == 3)
		{
			Input->InjectInputForAction(JumpAction, FInputActionValue(true), {}, {});
			Phase = 4;
			return false;
		}
		if (Phase == 4)
		{
			Test->TestTrue(TEXT("Double jump flag drives animation"), Character->HasDoubleJumped());
			// Exercise Blueprint interface forwarding used by the template platforms.
			Character->ApplySoftCollision(true);
			Test->TestEqual(TEXT("Drop disables platform blocking"), Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Ignore);
			UFunction* Reset = Character->FindFunction(TEXT("SoftCollisionSwitch"));
			if (Test->TestNotNull(TEXT("Platform interface bridge"), Reset)) Character->ProcessEvent(Reset, nullptr);
			Test->TestEqual(TEXT("Platform bridge restores blocking"), Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Block);

			// The reference BP and native manager must produce the same camera trajectory.
			UClass* CameraClass = LoadClass<APlayerCameraManager>(nullptr, TEXT("/Game/_ExternalAssets/ThirdPerson/Variant_SideScroller/Blueprints/BP_SideScrollingCameraManager.BP_SideScrollingCameraManager_C"));
			APlayerCameraManager* ReferenceCamera = World->SpawnActor<APlayerCameraManager>(CameraClass);
			ASideScrollingCameraManager* NativeCamera = World->SpawnActor<ASideScrollingCameraManager>();
			ReferenceCamera->InitializeFor(Player);
			NativeCamera->InitializeFor(Player);
			ReferenceCamera->SetViewTarget(Character);
			NativeCamera->SetViewTarget(Character);
			for (int32 Frame = 0; Frame < 40; ++Frame)
			{
				const FVector Position(100.0 + Frame * 30.0, Start.Y, Start.Z + Frame * 15.0);
				Character->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
				Character->GetCharacterMovement()->Velocity = FVector(0, 0, Frame < 20 ? 50 : 0);
				// The BP reads world delta time; use that same delta for the native update.
				ReferenceCamera->UpdateCamera(World->GetDeltaSeconds());
				NativeCamera->UpdateCamera(World->GetDeltaSeconds());
				if (!ReferenceCamera->GetCameraLocation().Equals(NativeCamera->GetCameraLocation(), 0.1))
				{
					Test->AddError(FString::Printf(TEXT("Camera parity mismatch at frame %d: reference=%s native=%s"), Frame, *ReferenceCamera->GetCameraLocation().ToString(), *NativeCamera->GetCameraLocation().ToString()));
					break;
				}
			}
			ReferenceCamera->Destroy();
			NativeCamera->Destroy();

			// Build a wall within the template's 50 cm trace and test launch direction.
			Character->SetActorLocation(FVector(3000, Start.Y, 3000), false, nullptr, ETeleportType::TeleportPhysics);
			Character->SetActorRotation(FRotator::ZeroRotator);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			AActor* Wall = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
			Wall->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(10, 100, 200));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent();
			Wall->SetActorLocation(FVector(3045, Start.Y, 3000));
			Character->DoMove(1.0f);
			Character->DoJumpStart();
			Test->TestTrue(TEXT("Wall jump activates lockout"), Character->HasWallJumped());
			PhaseTime = Now;
			Phase = 5;
			return false;
		}
		if (Phase == 5)
		{
			Test->TestTrue(TEXT("Wall jump moves away from wall"), Character->GetVelocity().X < 0);
			Phase = 6;
			return false;
		}
		if (Phase == 6)
		{
			if (Now - PhaseTime < 0.35) return false;
			Test->TestFalse(TEXT("Wall jump lockout expires"), Character->HasWallJumped());
			const TSubclassOf<APawn> PreviousClass = Character->GetClass();
			Character->Destroy();
			Test->TestNotNull(TEXT("Destroyed character respawns"), Player->GetPawn().Get());
			Test->TestTrue(TEXT("Respawn preserves character class"), Player->GetPawn() && Player->GetPawn()->IsA(PreviousClass));
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	int32 Phase = 0;
	double PhaseTime = 0;
	FVector Start = FVector::ZeroVector;
	UInputAction* MoveAction = nullptr;
	UInputAction* JumpAction = nullptr;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSideScrollerPlayTest, "TheLastMessage.SideScroller.PlayIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSideScrollerPlayTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Test_Jihong/SideScroller/Lvl_SideScrolling")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSideScrollerPlayChecks>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
