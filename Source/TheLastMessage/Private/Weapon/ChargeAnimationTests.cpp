#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Player/PlayerLocomotionAnimInstance.h"
#include "DataAsset/WeaponDataAsset.h"
#include "GAS/Abilities/GA_AttackPrimary.h"
#include "GAS/CombatAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChargeAnimationTest, "TheLastMessage.Weapon.ChargeAnimationLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChargeAnimationTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance = [World](float Seconds)
    {
        for (int32 I = 0; I < FMath::RoundToInt(Seconds / 0.01f); ++I)
        { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.01f); }
    };
    UClass* PlayerClass = LoadClass<APlayerBase>(nullptr, TEXT("/Game/Test_Jihong/Blueprints/Player/BP_TestPlayer.BP_TestPlayer_C"));
    APlayerBase* Player = World->SpawnActor<APlayerBase>(PlayerClass, FVector(0, 0, 200), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Player can spawn"), Player)) return false;
    auto* Controller = World->SpawnActor<APlayerController>(); Controller->Possess(Player);
    Player->GetCharacterMovement()->DisableMovement();
    Player->GetCombatAttributes()->SetMaxHealth(100); Player->GetCombatAttributes()->SetHealth(100);
    Player->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Advance(0.05f);
    auto* ASC = Player->GetAbilitySystemComponent();
    auto* Spec = ASC->FindAbilitySpecFromClass(UGA_AttackPrimary::StaticClass());
    auto* Anim = Player->GetMesh()->GetAnimInstance();
    TestNotNull(TEXT("Air-aware locomotion class is installed"), Cast<UPlayerLocomotionAnimInstance>(Anim));
    if (!TestNotNull(TEXT("Primary ability exists"), Spec) || !TestNotNull(TEXT("Animation instance exists"), Anim)) return false;
    for (const TCHAR* Kind : {TEXT("Hammer"), TEXT("Railgun")})
    {
        auto* Source = LoadObject<UWeaponDataAsset>(nullptr, *FString::Printf(TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_%s.DA_Weapon_%s"), Kind, Kind));
        if (!TestNotNull(TEXT("Weapon exists"), Source)) continue;
        auto* Data = DuplicateObject<UWeaponDataAsset>(Source, Player);
        Data->EnergyPerShot = 0.0f;
        Data->ChargeStartDelay = 0.1f; Data->MaxChargeTime = 0.3f;
        TestNotNull(TEXT("Charge montage assigned"), Data->ChargeMontage.Get());
        TestTrue(TEXT("Equip charging weapon"), Player->EquipWeapon(EWeaponSlot::Primary, Data));
        if (!Data->ChargeMontage) continue;
        Spec->InputPressed = true;
        TestTrue(TEXT("Hold activates"), ASC->TryActivateAbility(Spec->Handle));
        Advance(0.04f);
        TestFalse(TEXT("Animation waits for hold threshold"), Anim->Montage_IsPlaying(Data->ChargeMontage));
        Advance(0.12f);
        TestTrue(TEXT("Hold starts charge montage"), Anim->Montage_IsPlaying(Data->ChargeMontage));
        Advance(2.0f);
        TestEqual(TEXT("Long hold remains in Loop"), Anim->Montage_GetCurrentSection(Data->ChargeMontage), FName(TEXT("Loop")));
        const auto* AirProperty = FindFProperty<FBoolProperty>(UPlayerLocomotionAnimInstance::StaticClass(), TEXT("bUseAirAttackPose"));
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        Anim->NativeUpdateAnimation(0.01f);
        TestTrue(TEXT("Air charge enables upper-body masking"), AirProperty->GetPropertyValue_InContainer(Anim));
        Player->GetCharacterMovement()->DisableMovement();
        Anim->NativeUpdateAnimation(0.01f);
        TestTrue(TEXT("Landing preserves charge pose"), AirProperty->GetPropertyValue_InContainer(Anim));
        ASC->AbilitySpecInputReleased(*Spec);
        TestTrue(TEXT("Release starts attack without charge interruption cancelling it"), Anim->Montage_IsPlaying(Data->AttackMontage));
        TestTrue(TEXT("Attack ability remains active after releasing"), Spec->IsActive());
        if (!Data->ChargeReleaseSection.IsNone())
        {
            TestEqual(TEXT("Hammer starts at Release"), Anim->Montage_GetCurrentSection(Data->AttackMontage), Data->ChargeReleaseSection);
            const int32 Index = Data->AttackMontage->GetSectionIndex(Data->ChargeReleaseSection);
            TestTrue(TEXT("Release has correct initial position"), FMath::IsNearlyEqual(Anim->Montage_GetPosition(Data->AttackMontage), Data->AttackMontage->CompositeSections[Index].GetTime(), 0.02f));
        }
        Advance(1.5f);
        TestFalse(TEXT("Released attack completes"), Spec->IsActive());
        Spec->InputPressed = true;
        TestTrue(TEXT("Cancelable charge activates"), ASC->TryActivateAbility(Spec->Handle));
        Advance(0.2f);
        ASC->CancelAbilityHandle(Spec->Handle);
        Advance(0.3f);
        TestFalse(TEXT("Cancel stops charge montage"), Anim->Montage_IsPlaying(Data->ChargeMontage));
        TestFalse(TEXT("Cancel clears charge state"), Player->IsChargingWeapon());
        // Ordinary hammer swings must not stop at the new marker or select it randomly.
        if (!Data->ChargeReleaseSection.IsNone()) for (int32 Trial = 0; Trial < 12; ++Trial)
        {
            Spec->InputPressed = true;
            TestTrue(TEXT("Tap activates"), ASC->TryActivateAbility(Spec->Handle));
            ASC->AbilitySpecInputReleased(*Spec);
            const FName Section = Anim->Montage_GetCurrentSection(Data->AttackMontage);
            TestTrue(TEXT("Tap selects an ordinary attack"), Section == TEXT("Attack_00") || Section == TEXT("Attack_01"));
            Advance(0.3f);
            if (Section == TEXT("Attack_01")) TestTrue(TEXT("Second ordinary swing continues through Release marker"), Spec->IsActive());
            Advance(1.0f);
            TestFalse(TEXT("Tap attack completes"), Spec->IsActive());
        }
    }
    World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
