#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TLMFontSetupLibrary.generated.h"

class UFont;
class UFontFace;

/**
 * 에디터 전용 폰트 설정 도우미.
 * 파이썬에서는 Font 에셋의 Composite Font(Font Face 연결)를 직접 바꿀 수 없어서 이 함수로 채운다.
 * Scripts/create_tlm_fonts.py 가 호출한다. 게임 실행 중에는 아무 일도 하지 않는다.
 */
UCLASS()
class THELASTMESSAGE_API UTLMFontSetupLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Font 를 런타임 캐시 컴포지트 폰트로 만든다.
	 * TypefaceNames[i] 이름으로 Faces[i] 를 기본 서체에 넣고, 이 폰트에 없는 글자는 Fallback 서체로 그린다.
	 * 성공하면 true.
	 */
	UFUNCTION(BlueprintCallable, Category = "TLM|Fonts", meta = (DevelopmentOnly))
	static bool ConfigureCompositeFont(UFont* Font,
		const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces,
		const TArray<FName>& FallbackNames, const TArray<UFontFace*>& FallbackFaces);
};
