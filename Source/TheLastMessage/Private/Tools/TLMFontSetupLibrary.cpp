#include "Tools/TLMFontSetupLibrary.h"

#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"

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

	FCompositeFont& Composite = Font->CompositeFont;
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
