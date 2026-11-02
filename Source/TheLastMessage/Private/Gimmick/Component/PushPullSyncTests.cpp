#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Gimmick/Component/PushPullComponent.h"
#include "Gimmick/PushPull/PushPullGimmick.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/Player/PlayerLocomotionAnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPushPullSyncTest, "TheLastMessage.Gimmick.PushPullDistanceSync",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPushPullSyncTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto MakeBlock = [&](FVector Location, FVector Scale)
    {
        auto* A = World->SpawnActor<AActor>();
        auto* M = NewObject<UStaticMeshComponent>(A); A->SetRootComponent(M); A->AddInstanceComponent(M);
        M->SetStaticMesh(Cube); M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        M->SetCollisionResponseToAllChannels(ECR_Block); M->RegisterComponent();
        A->SetActorScale3D(Scale); A->SetActorLocation(Location); return A;
    };
    auto* Floor = MakeBlock(FVector(0,0,-10), FVector(100,100,0.2));
    auto* Table = NewObject<UDataTable>(); Table->RowStruct = FPushPullConfigRow::StaticStruct();
    TStrongObjectPtr<UDataTable> KeepTestTableAlive(Table);
    FPushPullConfigRow Config; Config.MoveSpeed = 140; Config.HorizontalAcceleration = 1200;
    Table->AddRow(TEXT("Test"), Config);
    auto Advance = [&](float Seconds, float DT, APlayerBase* Player, float Input)
    {
        for (int32 I=0; I<FMath::RoundToInt(Seconds/DT); ++I)
        {
            if (Player) Player->AddMovementInput(FVector::XAxisVector, Input);
            ++GFrameCounter; World->Tick(LEVELTICK_All, DT);
        }
    };
    auto* PlayerClass = LoadClass<APlayerBase>(nullptr,TEXT("/Game/Test_Jihong/Blueprints/Player/BP_TestPlayer.BP_TestPlayer_C"));
    auto* Defaults = PlayerClass->GetDefaultObject<APlayerBase>();
    auto* BombClass = LoadClass<APushPullGimmick>(nullptr,TEXT("/Game/Test_Jiho/Gimmick/PushPull/BP_ExplosiveBomb.BP_ExplosiveBomb_C"));
    auto* BombDefaults = BombClass->GetDefaultObject<APushPullGimmick>();
    TArray<double> PullSpeeds;
    for (float BoxScale : {1.0f,2.0f}) for (float DT : {1.0f/30.0f, 1.0f/120.0f}) for (float Side : {-1.0f, 1.0f})
    {
        auto* Player = World->SpawnActor<APlayerBase>(APlayerBase::StaticClass(), FVector(Side*220,0,100), FRotator::ZeroRotator);
        Player->GetMesh()->SetSkeletalMeshAsset(Defaults->GetMesh()->GetSkeletalMeshAsset());
        Player->GetMesh()->SetRelativeTransform(Defaults->GetMesh()->GetRelativeTransform());
        Player->GetMesh()->SetAnimInstanceClass(Defaults->GetMesh()->GetAnimClass());
        Player->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        FindFProperty<FObjectPropertyBase>(APlayerBase::StaticClass(),TEXT("PushMontage"))->SetObjectPropertyValue_InContainer(Player,FindFProperty<FObjectPropertyBase>(APlayerBase::StaticClass(),TEXT("PushMontage"))->GetObjectPropertyValue_InContainer(Defaults));
        FindFProperty<FObjectPropertyBase>(APlayerBase::StaticClass(),TEXT("PullMontage"))->SetObjectPropertyValue_InContainer(Player,FindFProperty<FObjectPropertyBase>(APlayerBase::StaticClass(),TEXT("PullMontage"))->GetObjectPropertyValue_InContainer(Defaults));
        auto* Controller = World->SpawnActor<APlayerController>(); Controller->Possess(Player);
        Player->GetCombatAttributes()->SetMaxHealth(100); Player->GetCombatAttributes()->SetHealth(100);
        const FTransform Transform(FVector(0,0,40));
        auto* Box = World->SpawnActorDeferred<APushPullGimmick>(APushPullGimmick::StaticClass(), Transform);
        auto* Mesh = Box->GetPhysicsMesh(); Mesh->SetStaticMesh(BombDefaults->GetPhysicsMesh()->GetStaticMesh());
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Mesh->SetCollisionResponseToAllChannels(ECR_Block);
        Mesh->SetSimulatePhysics(true);
        auto* Row = FindFProperty<FStructProperty>(APushPullGimmick::StaticClass(), TEXT("PushPullConfigRow"))->ContainerPtrToValuePtr<FDataTableRowHandle>(Box);
        Row->DataTable=Table; Row->RowName=TEXT("Test"); Box->FinishSpawning(Transform);
        Box->SetActorScale3D(BombDefaults->GetPhysicsMesh()->GetRelativeScale3D()*BoxScale);
        Advance(0.4f,DT,nullptr,0);
        auto* Movement = Player->GetCharacterMovement();
        auto* PushPull = Player->FindComponentByClass<UPushPullComponent>();
        const float WalkSpeed = Movement->MaxWalkSpeed;
        if (TestTrue(TEXT("Grab starts while grounded"), PushPull->StartPushPull(Box)))
        {
            const double BeforeApproach = Player->GetActorLocation().X;
            TestTrue(TEXT("Far interaction begins an approach"),PushPull->IsApproaching());
            Advance(DT,DT,nullptr,0);
            TestTrue(TEXT("Approach does not snap in one frame"),PushPull->IsApproaching() && FMath::Abs(Player->GetActorLocation().X-BeforeApproach)<15.0);
            FVector EarlyL,EarlyR;
            TestFalse(TEXT("Hands are not forced onto the box during approach"),PushPull->GetHandContacts(EarlyL,EarlyR));
            Advance(0.6f,DT,nullptr,0);
            TestFalse(TEXT("Approach completes"),PushPull->IsApproaching());
            const double Gap = Player->GetActorLocation().X - Mesh->GetComponentLocation().X;
            const double SurfaceDistance = FMath::Abs(Player->GetActorLocation().X-Mesh->Bounds.Origin.X)-Mesh->Bounds.BoxExtent.X;
            TestTrue(TEXT("Max-range grab approaches the box"), FMath::IsNearlyEqual(SurfaceDistance,Player->GetCapsuleComponent()->GetScaledCapsuleRadius()+3.0,1.0));
            double MaxError=0;
            double MaxHandError=0;
            const double StartX = Mesh->GetComponentLocation().X;
            // Push, release input to brake, then pull. Repeated reversals reveal drift.
            for (float Input : {-Side,0.0f,Side,0.0f,-Side})
            {
                UAnimMontage* RestMontage = nullptr;
                FVector LastRestHand = FVector::ZeroVector;
                double RestMotion = 0.0;
                for (int32 I=0; I<FMath::RoundToInt(0.7f/DT); ++I)
                {
                    Advance(DT,DT,Player,Input);
                    MaxError=FMath::Max(MaxError,FMath::Abs(Player->GetActorLocation().X-Mesh->GetComponentLocation().X-Gap));
                    if (I>10)
                    {
                        FVector L,R; PushPull->GetHandContacts(L,R);
                        const FVector Shoulder = (Player->GetMesh()->GetSocketLocation(TEXT("upperarm_l"))+Player->GetMesh()->GetSocketLocation(TEXT("upperarm_r")))*0.5;
                        TestTrue(TEXT("Grip stays below shoulder height"),L.Z<Shoulder.Z+3.0 && R.Z<Shoulder.Z+3.0);
                        MaxHandError=FMath::Max(MaxHandError,FVector::Dist(L,Player->GetMesh()->GetSocketLocation(TEXT("hand_l"))));
                        MaxHandError=FMath::Max(MaxHandError,FVector::Dist(R,Player->GetMesh()->GetSocketLocation(TEXT("hand_r"))));
                    }
                    if (Input == 0.0f && I*DT > 0.4f)
                    {
                        auto* Current = Player->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage();
                        const FVector Hand = Player->GetMesh()->GetSocketLocation(TEXT("hand_l"));
                        if (RestMontage)
                        {
                            TestEqual(TEXT("Standing grip does not alternate push/pull montages"),Current,RestMontage);
                            RestMotion=FMath::Max(RestMotion,FVector::Dist(Hand,LastRestHand));
                        }
                        RestMontage=Current; LastRestHand=Hand;
                    }
                }
                if (Input == 0.0f) TestTrue(FString::Printf(TEXT("Standing hand is stable (%.4f cm/frame)"),RestMotion),RestMotion<0.5);
                if (Input == Side)
                {
                    const double Speed=FMath::Abs(PushPull->GetFollowSpeedX());
                    PullSpeeds.Add(Speed);
                    AddInfo(FString::Printf(TEXT("Pull direction %.0f at %.0ffps: speed %.3f cm/s"),Side,1/DT,Speed));
                    TestTrue(TEXT("Pull reaches usable speed in either direction"),Speed>50);
                    TestEqual(TEXT("Pull uses the pull montage"), Player->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage(), Cast<UAnimMontage>(FindFProperty<FObjectPropertyBase>(APlayerBase::StaticClass(),TEXT("PullMontage"))->GetObjectPropertyValue_InContainer(Defaults)));
                }
            }
            TestTrue(TEXT("Push/pull stays engaged"), PushPull->IsPushPulling());
            AddInfo(FString::Printf(TEXT("%.0ffps side %.0f: box displacement %.2f cm; maximum gap error %.4f cm"),1/DT,Side,Mesh->GetComponentLocation().X-StartX,MaxError));
            TestTrue(TEXT("Box actually moved"), FMath::Abs(Mesh->GetComponentLocation().X-StartX)>20);
            TestTrue(FString::Printf(TEXT("Gap remains within 2cm at %.0ffps side %.0f (error %.3f)"),1/DT,Side,MaxError),MaxError<2);
            TestTrue(FString::Printf(TEXT("Both hands remain on the box (error %.3f cm)"),MaxHandError),MaxHandError<2);
            AddInfo(FString::Printf(TEXT("Actual TNT mesh: maximum hand contact error %.4f cm"),MaxHandError));
            // A wall blocks the box while the holder continues trying to push.
            const double WallX = Mesh->GetComponentLocation().X - Side*160;
            auto* Wall = MakeBlock(FVector(WallX,0,120),FVector(0.2,4,2.4));
            Advance(2.5f,DT,Player,-Side);
            TestTrue(TEXT("Wall cannot make the holder sink into the box"), FMath::Abs(Player->GetActorLocation().X-Mesh->GetComponentLocation().X-Gap)<2);
            Wall->Destroy();
            PushPull->StopPushPull();
            TestEqual(TEXT("Walking speed restored"),Movement->MaxWalkSpeed,WalkSpeed);
            TestTrue(TEXT("Character physics interaction restored"),Movement->bEnablePhysicsInteraction);
            TestFalse(TEXT("Temporary box sweep ignore removed"),Player->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Box));
        }
        Box->Destroy(); Player->Destroy(); Controller->Destroy(); Advance(0.1f,DT,nullptr,0);
    }
    for (int32 I=0; I+1<PullSpeeds.Num(); I+=2)
        TestTrue(TEXT("Left and right pull speeds match"),FMath::Abs(PullSpeeds[I]-PullSpeeds[I+1])<10);
    Floor->Destroy(); World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
