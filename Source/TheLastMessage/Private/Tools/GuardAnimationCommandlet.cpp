#include "Tools/GuardAnimationCommandlet.h"
#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxImportUI.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace GuardAnim
{
    const TCHAR* SkeletonPath = TEXT("/Game/Test_Jihong/Blueprints/Player/Robbie_01_Skeleton.Robbie_01_Skeleton");
    const TCHAR* DestDir = TEXT("/Game/Test_Jihong/Animations/Guard");
    const TCHAR* MontageName = TEXT("AM_PlayerGuard");

    bool Save(UObject* Object)
    {
        UPackage* Package = Object->GetOutermost();
        Package->MarkPackageDirty();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        return UPackage::SavePackage(Package, Object, *File, Args);
    }

    UAnimSequence* Import(USkeleton* Skeleton, const FString& SourceFile, const FString& AssetName)
    {
        UFbxImportUI* UI = NewObject<UFbxImportUI>();
        UI->bAutomatedImportShouldDetectType = false;
        UI->MeshTypeToImport = FBXIT_Animation;
        UI->OriginalImportType = FBXIT_SkeletalMesh;
        UI->bImportAsSkeletal = true;
        UI->bImportMesh = false;
        UI->bImportAnimations = true;
        UI->bImportMaterials = false;
        UI->bImportTextures = false;
        UI->Skeleton = Skeleton;
        UI->AnimSequenceImportData->AnimationLength = FBXALIT_AnimatedKey;
        UI->AnimSequenceImportData->bImportBoneTracks = true;

        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        Task->Filename = SourceFile;
        Task->DestinationPath = DestDir;
        Task->DestinationName = AssetName;
        Task->bAutomated = true;
        Task->bReplaceExisting = true;
        Task->bSave = true;
        Task->Options = UI;
        FAssetToolsModule::GetModule().Get().ImportAssetTasks({Task});

        const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), DestDir, *AssetName, *AssetName);
        return LoadObject<UAnimSequence>(nullptr, *ObjectPath);
    }
}
#endif

int32 UGuardAnimationCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    using namespace GuardAnim;
    // FbxImportUI 옵션(스켈레톤 지정, 메시 제외)을 그대로 쓰도록 레거시 FBX 임포터로 가져온다.
    if (IConsoleVariable* Interchange = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.FBX")))
    {
        Interchange->Set(false, ECVF_SetByCode);
    }

    USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
    if (!Skeleton)
    {
        UE_LOG(LogTemp, Error, TEXT("GUARD skeleton not found: %s"), SkeletonPath);
        return 1;
    }

    const FString SourceDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Claude outputs/AnimSource"));
    TArray<UAnimSequence*> Sequences;
    for (const TCHAR* Phase : {TEXT("Start"), TEXT("Loop"), TEXT("End")})
    {
        const FString AssetName = FString::Printf(TEXT("A_PlayerGuard_%s"), Phase);
        const FString SourceFile = SourceDir / (AssetName + TEXT(".fbx"));
        if (!FPaths::FileExists(SourceFile))
        {
            UE_LOG(LogTemp, Error, TEXT("GUARD source missing: %s"), *SourceFile);
            return 2;
        }
        UAnimSequence* Sequence = Import(Skeleton, SourceFile, AssetName);
        if (!Sequence || Sequence->GetSkeleton() != Skeleton || Sequence->GetPlayLength() <= 0.0f)
        {
            UE_LOG(LogTemp, Error, TEXT("GUARD import failed: %s"), *AssetName);
            return 3;
        }
        UE_LOG(LogTemp, Display, TEXT("GUARD imported %s length=%.3f fps=%.1f"), *AssetName,
            Sequence->GetPlayLength(), Sequence->GetSamplingFrameRate().AsDecimal());
        Sequences.Add(Sequence);
    }

    const FString PackageName = FString(DestDir) / MontageName;
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, MontageName));
    const bool bCreated = Montage == nullptr;
    if (bCreated)
    {
        Montage = NewObject<UAnimMontage>(CreatePackage(*PackageName), MontageName, RF_Public | RF_Standalone);
    }
    Montage->SetSkeleton(Skeleton);
    Montage->SlotAnimTracks.Reset();
    Montage->CompositeSections.Reset();
    FSlotAnimationTrack& Track = Montage->AddSlot(TEXT("DefaultSlot"));

    static const FName SectionNames[] = {TEXT("Start"), TEXT("Loop"), TEXT("End")};
    float Position = 0.0f;
    TArray<int32> SectionIndices;
    for (int32 Index = 0; Index < Sequences.Num(); ++Index)
    {
        UAnimSequence* Sequence = Sequences[Index];
        FAnimSegment Segment;
        Segment.SetAnimReference(Sequence);
        Segment.StartPos = Position;
        Segment.AnimStartTime = 0.0f;
        Segment.AnimEndTime = Sequence->GetPlayLength();
        Segment.AnimPlayRate = 1.0f;
        Segment.LoopingCount = 1;
        Track.AnimTrack.AnimSegments.Add(Segment);
        Position += Sequence->GetPlayLength();
    }
    Montage->SetCompositeLength(Montage->CalculateSequenceLength());
    Position = 0.0f;
    for (int32 Index = 0; Index < Sequences.Num(); ++Index)
    {
        SectionIndices.Add(Montage->AddAnimCompositeSection(SectionNames[Index], Position));
        Position += Sequences[Index]->GetPlayLength();
    }
    // 누르고 있는 동안 Loop를 반복하고, 키를 떼면 GA_CoreGuard가 End로 넘긴다.
    Montage->CompositeSections[SectionIndices[0]].NextSectionName = TEXT("Loop");
    Montage->CompositeSections[SectionIndices[1]].NextSectionName = TEXT("Loop");
    Montage->CompositeSections[SectionIndices[2]].NextSectionName = NAME_None;
    Montage->BlendIn.SetBlendTime(0.12f);
    Montage->BlendModeIn = EMontageBlendMode::Inertialization;
    Montage->BlendOut.SetBlendTime(0.18f);
    Montage->BlendModeOut = EMontageBlendMode::Inertialization;
    Montage->PostEditChange();
    if (bCreated)
    {
        FAssetRegistryModule::AssetCreated(Montage);
    }
    if (!Save(Montage))
    {
        UE_LOG(LogTemp, Error, TEXT("GUARD montage save failed"));
        return 4;
    }
    for (const FCompositeSection& Section : Montage->CompositeSections)
    {
        UE_LOG(LogTemp, Display, TEXT("GUARD section %s time=%.3f next=%s"), *Section.SectionName.ToString(),
            Section.GetTime(), *Section.NextSectionName.ToString());
    }
    UE_LOG(LogTemp, Display, TEXT("GUARD montage %s ready (length %.3f)."), *PackageName, Montage->GetPlayLength());
    return 0;
#else
    return 1;
#endif
}
