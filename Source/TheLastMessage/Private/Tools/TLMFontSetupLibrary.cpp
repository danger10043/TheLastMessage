#include "Tools/TLMFontSetupLibrary.h"

#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

UFontFace* UTLMFontSetupLibrary::ImportFontFace(const FString& AssetPath, const FString& SourceFile)
{
#if WITH_EDITOR
	TArray<uint8> Bytes;
	if (!FPackageName::IsValidLongPackageName(AssetPath) || !FFileHelper::LoadFileToArray(Bytes, *SourceFile) || Bytes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ImportFontFace: %s 를 읽지 못했습니다 (%s)."), *SourceFile, *AssetPath);
		return nullptr;
	}
	UPackage* Package = CreatePackage(*AssetPath);
	const FName Name(*FPackageName::GetLongPackageAssetName(AssetPath));
	if (FindObject<UFontFace>(Package, *Name.ToString()))
	{
		UE_LOG(LogTemp, Warning, TEXT("ImportFontFace: %s 는 이미 있습니다."), *AssetPath);
		return nullptr;
	}
	auto* Face = NewObject<UFontFace>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
	// PostEditChange 는 Slate 렌더러의 폰트 캐시를 비우는데, 커맨드렛에는 Slate 앱이 없어서 부르지 않는다.
	Face->InitializeFromBulkData(SourceFile, EFontHinting::Default, Bytes.GetData(), Bytes.Num());
	Face->LoadingPolicy = EFontLoadingPolicy::LazyLoad;
	FAssetRegistryModule::AssetCreated(Face);
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString File = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Package, Face, *File, Args))
	{
		UE_LOG(LogTemp, Warning, TEXT("ImportFontFace: %s 를 저장하지 못했습니다."), *AssetPath);
		return nullptr;
	}
	UE_LOG(LogTemp, Display, TEXT("TLM_FONTFACE_IMPORTED %s (%d bytes)"), *AssetPath, Bytes.Num());
	return Face;
#else
	return nullptr;
#endif
}

bool UTLMFontSetupLibrary::ConfigureCompositeFont(UFont* Font,
	const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces,
	const TArray<FName>& FallbackNames, const TArray<UFontFace*>& FallbackFaces)
{
#if WITH_EDITOR
	if (!IsValid(Font) || Faces.Num() == 0 || TypefaceNames.Num() != Faces.Num()
		|| FallbackNames.Num() != FallbackFaces.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("ConfigureCompositeFont: 인자가 올바르지 않습니다."));
		return false;
	}

	Font->Modify();
	Font->FontCacheType = EFontCacheType::Runtime;

	FCompositeFont& Composite = Font->GetMutableInternalCompositeFont();
	Composite.DefaultTypeface.Fonts.Reset();
	for (int32 Index = 0; Index < Faces.Num(); ++Index)
	{
		if (!IsValid(Faces[Index]))
		{
			UE_LOG(LogTemp, Warning, TEXT("ConfigureCompositeFont: %s 의 Font Face 가 비어 있습니다."), *TypefaceNames[Index].ToString());
			return false;
		}
		FTypefaceEntry& Entry = Composite.DefaultTypeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = TypefaceNames[Index];
		Entry.Font = FFontData(Faces[Index]);
	}

	Composite.FallbackTypeface.Typeface.Fonts.Reset();
	for (int32 Index = 0; Index < FallbackFaces.Num(); ++Index)
	{
		if (!IsValid(FallbackFaces[Index]))
		{
			continue;
		}
		FTypefaceEntry& Entry = Composite.FallbackTypeface.Typeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = FallbackNames[Index];
		Entry.Font = FFontData(FallbackFaces[Index]);
	}

	Composite.SubTypefaces.Reset();
	Composite.MakeDirty();
	Font->PostEditChange();
	Font->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("TLM_FONT_CONFIGURED %s (%d typefaces, %d fallback)"),
		*Font->GetPathName(), Composite.DefaultTypeface.Fonts.Num(), Composite.FallbackTypeface.Typeface.Fonts.Num());
	return true;
#else
	return false;
#endif
}

bool UTLMFontSetupLibrary::AddSubFont(UFont* Font, FName EditorName, const FString& Cultures, const TArray<FIntPoint>& CharacterRanges,
	const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces)
{
#if WITH_EDITOR
	if (!IsValid(Font) || Faces.Num() == 0 || TypefaceNames.Num() != Faces.Num() || CharacterRanges.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddSubFont: 인자가 올바르지 않습니다 (%s)."), *EditorName.ToString());
		return false;
	}
	for (int32 Index = 0; Index < Faces.Num(); ++Index)
	{
		if (!IsValid(Faces[Index]))
		{
			UE_LOG(LogTemp, Warning, TEXT("AddSubFont: %s 의 %s Font Face 가 비어 있습니다."), *EditorName.ToString(), *TypefaceNames[Index].ToString());
			return false;
		}
	}

	Font->Modify();
	FCompositeFont& Composite = Font->GetMutableInternalCompositeFont();
	FCompositeSubFont& SubFont = Composite.SubTypefaces.AddDefaulted_GetRef();
	SubFont.EditorName = EditorName;
	SubFont.Cultures = Cultures;
	for (const FIntPoint& Range : CharacterRanges)
		SubFont.CharacterRanges.Add(FInt32Range(FInt32Range::BoundsType::Inclusive(Range.X), FInt32Range::BoundsType::Inclusive(Range.Y)));
	for (int32 Index = 0; Index < Faces.Num(); ++Index)
	{
		FTypefaceEntry& Entry = SubFont.Typeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = TypefaceNames[Index];
		Entry.Font = FFontData(Faces[Index]);
	}
	Composite.MakeDirty();
	Font->PostEditChange();
	Font->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("TLM_SUBFONT_ADDED %s %s (cultures '%s', %d ranges, %d typefaces)"),
		*Font->GetName(), *EditorName.ToString(), *Cultures, SubFont.CharacterRanges.Num(), SubFont.Typeface.Fonts.Num());
	return true;
#else
	return false;
#endif
}
