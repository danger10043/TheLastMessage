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

	/**
	 * Font 에 Sub-Font Family 를 하나 추가한다. ConfigureCompositeFont 가 Sub-Font 를 모두 지우므로 그 다음에 부른다.
	 * Cultures 는 세미콜론으로 구분한 컬처 목록(예: "ja", "zh-Hans"). 비우면 언어와 상관없이 문자 범위로만 고른다.
	 * CharacterRanges 의 각 값은 (첫 코드포인트, 마지막 코드포인트) 이며 둘 다 포함한다.
	 * TypefaceNames[i] 이름(기본 서체의 Regular/Bold 등과 같은 이름)으로 Faces[i] 를 넣는다. 성공하면 true.
	 */
	UFUNCTION(BlueprintCallable, Category = "TLM|Fonts", meta = (DevelopmentOnly))
	static bool AddSubFont(UFont* Font, FName EditorName, const FString& Cultures, const TArray<FIntPoint>& CharacterRanges,
		const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces);

	/**
	 * 폰트 파일(ttf/otf)로 Font Face 에셋을 만들어 저장한다(힌팅 Default, 필요할 때 불러오기 LazyLoad).
	 * 엔진의 폰트 가져오기는 Slate 앱이 있어야 해서 커맨드렛(-run=pythonscript)에서는 멈춘다. 이 함수는 Slate 없이도 된다.
	 * AssetPath 예: /Game/Main/UI/Fonts/Faces/FF_NotoSansSC_Regular. 실패하면 nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "TLM|Fonts", meta = (DevelopmentOnly))
	static UFontFace* ImportFontFace(const FString& AssetPath, const FString& SourceFile);
};
