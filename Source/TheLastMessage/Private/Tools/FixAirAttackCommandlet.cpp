#include "Tools/FixAirAttackCommandlet.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_Inertialization.h"
#include "AnimStateTransitionNode.h"
#include "K2Node_VariableGet.h"
#include "Character/Player/PlayerLocomotionAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Animation/AnimSequence.h"
#include "DataAsset/WeaponDataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"

template<class T> static T* AddAirNode(UEdGraph* G, const TCHAR* Name, int X, int Y)
{
    T* N = NewObject<T>(G, FName(Name), RF_Transactional);
    G->AddNode(N, false, false);
    N->CreateNewGuid(); N->PostPlacedNewNode();
    N->NodePosX = X; N->NodePosY = Y;
    return N;
}
static void AirLink(UEdGraphPin* A, UEdGraphPin* B)
{
    check(A && B);
    A->MakeLinkTo(B);
}
static bool SaveAirAsset(UObject* Object)
{
    UPackage* P = Object->GetOutermost();
    P->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(P, Object, *FPackageName::LongPackageNameToFilename(P->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}
#endif

int32 UFixAirAttackCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    if (Params.Contains(TEXT("-ChargeRepairSlots")))
    {
        for (const TCHAR* Kind : {TEXT("Hammer"), TEXT("Railgun")})
        {
            auto* M = LoadObject<UAnimMontage>(nullptr, *FString::Printf(TEXT("/Game/Test_Jihong/Animations/AM_PlayerCharge_%s.AM_PlayerCharge_%s"), Kind, Kind));
            if (!M) return 1;
            M->SlotAnimTracks.RemoveAll([](const FSlotAnimationTrack& Track) { return Track.AnimTrack.AnimSegments.IsEmpty(); });
            if (M->SlotAnimTracks.Num() != 1 || !M->IsValidSlot(TEXT("DefaultSlot"))) return 2;
            M->PostEditChange();
            if (!SaveAirAsset(M)) return 3;
        }
        return 0;
    }
    if (Params.Contains(TEXT("-ChargeApply")))
    {
        for (const TCHAR* Kind : {TEXT("Hammer"), TEXT("Railgun")})
        {
            const FString AssetName = FString::Printf(TEXT("AM_PlayerCharge_%s"), Kind);
            const FString PackageName = TEXT("/Game/Test_Jihong/Animations/") + AssetName;
            if (FPackageName::DoesPackageExist(PackageName))
            { UE_LOG(LogTemp, Error, TEXT("Refusing to overwrite existing %s"), *PackageName); return 1; }
            auto* Start = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Test_Jihong/Animations/A_PlayerCharge_%s_Start.A_PlayerCharge_%s_Start"), Kind, Kind));
            auto* Loop = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Test_Jihong/Animations/A_PlayerCharge_%s_Loop.A_PlayerCharge_%s_Loop"), Kind, Kind));
            auto* Data = LoadObject<UWeaponDataAsset>(nullptr, *FString::Printf(TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_%s.DA_Weapon_%s"), Kind, Kind));
            if (!Start || !Loop || !Data || !Data->AttackMontage || Start->GetSkeleton() != Loop->GetSkeleton()
                || Start->GetSkeleton() != Data->AttackMontage->GetSkeleton()) return 2;
            auto* M = NewObject<UAnimMontage>(CreatePackage(*PackageName), *AssetName, RF_Public | RF_Standalone);
            M->SetSkeleton(Start->GetSkeleton());
            M->SlotAnimTracks.Reset(); // Constructor creates an empty DefaultSlot.
            auto& Track = M->AddSlot(TEXT("DefaultSlot"));
            for (auto* A : {Start, Loop})
            {
                FAnimSegment Segment;
                Segment.SetAnimReference(A);
                Segment.StartPos = A == Start ? 0.0f : Start->GetPlayLength();
                Segment.AnimStartTime = 0.0f; Segment.AnimEndTime = A->GetPlayLength();
                Segment.AnimPlayRate = 1.0f; Segment.LoopingCount = 1;
                Track.AnimTrack.AnimSegments.Add(Segment);
            }
            M->SetCompositeLength(M->CalculateSequenceLength());
            const int32 StartIndex = M->AddAnimCompositeSection(TEXT("Start"), 0.0f);
            const int32 LoopIndex = M->AddAnimCompositeSection(TEXT("Loop"), Start->GetPlayLength());
            M->CompositeSections[StartIndex].NextSectionName = TEXT("Loop");
            M->CompositeSections[LoopIndex].NextSectionName = TEXT("Loop");
            M->BlendIn.SetBlendTime(0.12f); M->BlendOut.SetBlendTime(0.12f);
            M->BlendModeOut = EMontageBlendMode::Inertialization;
            M->PostEditChange();
            FAssetRegistryModule::AssetCreated(M);
            if (!SaveAirAsset(M)) return 3;
            Data->ChargeMontage = M;
            if (FString(Kind) == TEXT("Hammer"))
            {
                UAnimMontage* Attack = Data->AttackMontage;
                float ReleaseTime = -1.0f;
                for (const auto& T : Attack->SlotAnimTracks) for (const auto& S : T.AnimTrack.AnimSegments)
                {
                    if (S.GetAnimReference() && S.GetAnimReference()->GetName() == TEXT("AS_Combo_Attack_01_04_Seq"))
                        ReleaseTime = S.StartPos + (26.0f / S.GetAnimReference()->GetSamplingFrameRate().AsDecimal() - S.AnimStartTime) / S.AnimPlayRate;
                }
                if (ReleaseTime <= 0.0f || ReleaseTime >= Attack->GetPlayLength()) return 4;
                const int32 ReleaseIndex = Attack->AddAnimCompositeSection(TEXT("Release"), ReleaseTime);
                Attack->CompositeSections[ReleaseIndex].NextSectionName = NAME_None;
                const int32 NormalIndex = Attack->GetSectionIndex(TEXT("Attack_01"));
                if (NormalIndex != INDEX_NONE) Attack->CompositeSections[NormalIndex].NextSectionName = TEXT("Release");
                Data->ChargeReleaseSection = TEXT("Release");
                Attack->PostEditChange();
                if (!SaveAirAsset(Attack)) return 5;
                UE_LOG(LogTemp, Display, TEXT("CHARGE Release source frame 26 -> montage time %.6f"), ReleaseTime);
            }
            else
            {
                Data->AttackMontage->BlendModeIn = EMontageBlendMode::Inertialization;
                Data->AttackMontage->BlendIn.SetBlendTime(0.12f);
                if (!SaveAirAsset(Data->AttackMontage)) return 6;
            }
            if (!SaveAirAsset(Data)) return 7;
        }
        UE_LOG(LogTemp, Display, TEXT("CHARGE montage authoring and weapon assignment complete."));
        return 0;
    }
    if (Params.Contains(TEXT("-ChargeInspect")))
    {
        for (const TCHAR* Kind : {TEXT("Hammer"), TEXT("Railgun")})
        {
            auto* Data = LoadObject<UWeaponDataAsset>(nullptr, *FString::Printf(TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_%s.DA_Weapon_%s"), Kind, Kind));
            UE_LOG(LogTemp, Display, TEXT("CHARGE weapon %s montage=%s random=%d duration=%f"), Kind, *GetNameSafe(Data->AttackMontage), Data->bRandomizeAttackSection, Data->MaxChargeTime);
            auto* M = Data->AttackMontage.Get();
            for (const auto& S : M->CompositeSections) UE_LOG(LogTemp, Display, TEXT("CHARGE section %s time=%f next=%s"), *S.SectionName.ToString(), S.GetTime(), *S.NextSectionName.ToString());
            for (const auto& T : M->SlotAnimTracks) for (const auto& S : T.AnimTrack.AnimSegments)
                UE_LOG(LogTemp, Display, TEXT("CHARGE segment slot=%s anim=%s pos=%f start=%f end=%f rate=%f fps=%f"), *T.SlotName.ToString(), *GetNameSafe(S.GetAnimReference()), S.StartPos, S.AnimStartTime, S.AnimEndTime, S.AnimPlayRate, S.GetAnimReference()->GetSamplingFrameRate().AsDecimal());
            for (const auto& E : M->Notifies) UE_LOG(LogTemp, Display, TEXT("CHARGE notify %s time=%f duration=%f"), *E.NotifyName.ToString(), E.GetTime(), E.GetDuration());
            for (const TCHAR* Phase : {TEXT("Start"), TEXT("Loop")})
            {
                auto* A = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Test_Jihong/Animations/A_PlayerCharge_%s_%s.A_PlayerCharge_%s_%s"), Kind, Phase, Kind, Phase));
                UE_LOG(LogTemp, Display, TEXT("CHARGE imported %s duration=%f fps=%f skeleton=%s"), *GetNameSafe(A), A ? A->GetPlayLength() : -1, A ? A->GetSamplingFrameRate().AsDecimal() : -1, A ? *GetNameSafe(A->GetSkeleton()) : TEXT("none"));
            }
        }
        return 0;
    }
    UAnimBlueprint* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Test_Jihong/Animations/Robbie_01_AnimBP.Robbie_01_AnimBP"));
    if (!BP) return 1;
    TArray<UEdGraph*> Graphs;
    BP->GetAllGraphs(Graphs);
    if (Params.Contains(TEXT("-Apply")))
    {
        if (BP->ParentClass != UAnimInstance::StaticClass())
        {
            UE_LOG(LogTemp, Error, TEXT("Unexpected parent or migration already applied: %s"), *GetNameSafe(BP->ParentClass));
            return 1;
        }
        if (!BP->TargetSkeleton || BP->TargetSkeleton->GetReferenceSkeleton().FindBoneIndex(TEXT("spine_01")) == INDEX_NONE) return 2;
        UEdGraph* G = nullptr;
        for (UEdGraph* Candidate : Graphs) if (Candidate->GetName() == TEXT("AnimGraph")) G = Candidate;
        if (!G) return 3;
        UAnimGraphNode_Slot* Slot = nullptr;
        UAnimGraphNode_Root* Root = nullptr;
        for (UEdGraphNode* N : G->Nodes)
        {
            if (auto* S = Cast<UAnimGraphNode_Slot>(N); S && S->Node.SlotName == TEXT("DefaultSlot")) Slot = S;
            if (auto* R = Cast<UAnimGraphNode_Root>(N)) Root = R;
        }
        if (!Slot || !Root || Slot->FindPin(TEXT("Source"))->LinkedTo.Num() != 1
            || Root->FindPin(TEXT("Result"))->LinkedTo.Num() != 1
            || Root->FindPin(TEXT("Result"))->LinkedTo[0]->GetOwningNode() != Slot) return 4;
        UEdGraphPin* OriginalSource = Slot->FindPin(TEXT("Source"))->LinkedTo[0];
        BP->ParentClass = UPlayerLocomotionAnimInstance::StaticClass();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        Slot->FindPin(TEXT("Source"))->BreakAllPinLinks();
        Root->FindPin(TEXT("Result"))->BreakAllPinLinks();
        // Preserve carry and locomotion together, and evaluate the attack slot only once.
        auto* BaseCache = AddAirNode<UAnimGraphNode_SaveCachedPose>(G, TEXT("AirAttackBaseCache"), 0, 400);
        BaseCache->CacheName = TEXT("AirAttackBase"); BaseCache->AllocateDefaultPins();
        AirLink(OriginalSource, BaseCache->FindPin(TEXT("Pose")));
        auto Use = [&](UAnimGraphNode_SaveCachedPose* Cache, const TCHAR* Name, int X, int Y)
        {
            auto* N = AddAirNode<UAnimGraphNode_UseCachedPose>(G, Name, X, Y);
            N->SaveCachedPoseNode = Cache; N->AllocateDefaultPins(); return N;
        };
        auto* BaseForSlot = Use(BaseCache, TEXT("AirAttackSlotBase"), 300, 0);
        AirLink(BaseForSlot->FindPin(TEXT("Pose")), Slot->FindPin(TEXT("Source")));
        Slot->Node.bAlwaysUpdateSourcePose = true;
        auto* AttackCache = AddAirNode<UAnimGraphNode_SaveCachedPose>(G, TEXT("AirAttackFullCache"), 700, 0);
        AttackCache->CacheName = TEXT("AirAttackFull"); AttackCache->AllocateDefaultPins();
        AirLink(Slot->FindPin(TEXT("Pose")), AttackCache->FindPin(TEXT("Pose")));
        auto* BaseForMask = Use(BaseCache, TEXT("AirAttackLowerBody"), 700, 300);
        auto* FullForMask = Use(AttackCache, TEXT("AirAttackUpperBody"), 700, 450);
        auto* Mask = AddAirNode<UAnimGraphNode_LayeredBoneBlend>(G, TEXT("AirAttackMask"), 1000, 300);
        FBranchFilter Filter; Filter.BoneName = TEXT("spine_01"); Filter.BlendDepth = 0;
        Mask->Node.LayerSetup[0].BranchFilters.Add(Filter);
        Mask->Node.bMeshSpaceRotationBlend = true;
        Mask->AllocateDefaultPins();
        AirLink(BaseForMask->FindPin(TEXT("Pose")), Mask->FindPin(TEXT("BasePose")));
        AirLink(FullForMask->FindPin(TEXT("Pose")), Mask->FindPin(TEXT("BlendPoses_0")));
        Mask->FindPin(TEXT("BlendWeights_0"))->DefaultValue = TEXT("1.0");
        auto* FullGround = Use(AttackCache, TEXT("AirAttackGroundFull"), 1000, 600);
        auto* Select = AddAirNode<UAnimGraphNode_BlendListByBool>(G, TEXT("AirAttackSelect"), 1400, 300);
        Select->AllocateDefaultPins();
        AirLink(Mask->FindPin(TEXT("Pose")), Select->FindPin(TEXT("BlendPose_0")));
        AirLink(FullGround->FindPin(TEXT("Pose")), Select->FindPin(TEXT("BlendPose_1")));
        Select->FindPin(TEXT("BlendTime_0"))->DefaultValue = TEXT("0.18");
        Select->FindPin(TEXT("BlendTime_1"))->DefaultValue = TEXT("0.18");
        // A normal crossfade keeps both cached branches updating during the switch.
        auto* AirFlag = AddAirNode<UK2Node_VariableGet>(G, TEXT("AirAttackLatch"), 1100, 100);
        AirFlag->VariableReference.SetSelfMember(TEXT("bUseAirAttackPose")); AirFlag->AllocateDefaultPins();
        AirLink(AirFlag->FindPin(TEXT("bUseAirAttackPose")), Select->FindPin(TEXT("bActiveValue")));
        auto* Inertial = AddAirNode<UAnimGraphNode_Inertialization>(G, TEXT("AirAttackInertialization"), 1800, 300);
        Inertial->AllocateDefaultPins();
        AirLink(Select->FindPin(TEXT("Pose")), Inertial->FindPin(TEXT("Source")));
        AirLink(Inertial->FindPin(TEXT("Pose")), Root->FindPin(TEXT("Result")));
        Root->NodePosX = 2100; Root->NodePosY = 300;
        for (UEdGraph* Graph : Graphs) for (UEdGraphNode* N : Graph->Nodes)
        {
            if (auto* T = Cast<UAnimStateTransitionNode>(N))
            {
                const FString Title = T->GetNodeTitle(ENodeTitleType::ListView).ToString();
                if (Title.Contains(TEXT("JumpLoop -> JumpEnd")) || Title.Contains(TEXT("JumpEnd -> Idle/Run")))
                { T->LogicType = ETransitionLogicType::TLT_Inertialization; T->CrossfadeDuration = 0.18f; }
            }
        }
        BP->ParentClass = UPlayerLocomotionAnimInstance::StaticClass();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error) return 5;
        if (!SaveAirAsset(BP)) return 6;
        for (const TCHAR* Kind : {TEXT("LightMelee"), TEXT("HeavyMelee"), TEXT("LightRange"), TEXT("HeavyRange")})
        {
            FString Path = FString::Printf(TEXT("/Game/Test_Jihong/Animations/%s/AM_PlayerAttack_%s.AM_PlayerAttack_%s"), Kind, Kind, Kind);
            auto* M = LoadObject<UAnimMontage>(nullptr, *Path);
            if (!M) return 7;
            M->BlendModeOut = EMontageBlendMode::Inertialization;
            M->BlendOut.SetBlendTime(0.18f);
            if (!SaveAirAsset(M)) return 8;
        }
        UE_LOG(LogTemp, Display, TEXT("AIRATTACK migration compiled and saved successfully."));
        return 0;
    }
    for (UEdGraph* G : Graphs)
    {
        UE_LOG(LogTemp, Display, TEXT("AIRGRAPH %s"), *G->GetName());
        for (UEdGraphNode* N : G->Nodes)
        {
            UE_LOG(LogTemp, Display, TEXT("AIRNODE %s %s %s"), *N->GetName(), *N->GetClass()->GetName(), *N->GetNodeTitle(ENodeTitleType::ListView).ToString());
            for (UEdGraphPin* P : N->Pins)
            {
                FString Links;
                for (UEdGraphPin* L : P->LinkedTo) Links += L->GetOwningNode()->GetName() + TEXT(".") + L->PinName.ToString() + TEXT(" ");
                UE_LOG(LogTemp, Display, TEXT("AIRPIN %s %d %s = %s -> %s"), *P->PinName.ToString(), int32(P->Direction), *P->PinType.PinCategory.ToString(), *P->DefaultValue, *Links);
            }
        }
    }
    return 0;
#else
    return 1;
#endif
}
