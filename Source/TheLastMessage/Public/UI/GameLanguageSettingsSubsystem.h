#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "GameLanguageSettingsSubsystem.generated.h"

/** 게임이 지원하는 언어 하나. 표시 이름은 그 언어로 쓴 이름이라 번역하지 않는다. */
struct FGameLanguage
{
	/** 컬처 코드 (예: ko, zh-Hans, es-419) */
	const TCHAR* Culture;
	/** 그 언어로 쓴 언어 이름 (예: 한국어, 简体中文). 언어 설정 화면에는 국기 없이 이 이름만 보여 준다. */
	const TCHAR* NativeName;
};

/**
 * 게임 언어(컬처)를 고르고 저장한다. Docs/LanguageSettingsPlan.md 참고.
 * - 저장 위치: GameUserSettings.ini 의 [Internationalization] Culture= (엔진이 시작할 때 이 값을 읽는다)
 * - 첫 실행(저장된 값이 없을 때): 시스템 언어가 지원 목록에 있으면 그 언어, 없으면 en. 이때는 저장하지 않는다.
 * 에디터에서는 시작 언어를 바꾸지 않는다(에디터 화면 언어까지 바뀌기 때문).
 */
UCLASS()
class THELASTMESSAGE_API UGameLanguageSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** 에디터(PIE)에서 켠 게임 언어 미리보기를 PIE가 끝날 때 끈다. */
	virtual void Deinitialize() override;
	/** 에디터에서 SetLanguage 로 켠 게임 언어 미리보기를 끄고 원문(ko)으로 돌린다. 게임에서는 아무 일도 하지 않는다. */
	static void ClearEditorPreview();

	/** 지원 언어 12개. 순서가 언어 설정 화면의 버튼 순서(2열, 왼쪽에서 오른쪽, 위에서 아래)다. 첫 항목(ko)이 원문 언어다. */
	static TConstArrayView<FGameLanguage> GetSupportedLanguages();
	/** 언어 버튼 위젯 이름. 하이픈은 밑줄로 바꾼다 (예: zh-Hans -> Language_zh_Hans). 이름은 <이름>_Name. */
	static FName GetLanguageWidgetName(const FGameLanguage& Language);
	/** 아무 컬처(예: es-MX, zh-TW, en-US)를 지원 언어 목록의 위치로 바꾼다. 맞는 언어가 없으면 INDEX_NONE. */
	static int32 FindSupportedLanguage(const FString& Culture);
	/** 현재 게임 언어에 해당하는 지원 언어의 컬처 코드. 맞는 언어가 없으면 빈 문자열. */
	static FString GetCurrentLanguage();
	/** 처음 실행할 때 쓸 언어. SystemCulture 가 지원 목록에 있으면 그 언어, 없으면 en. */
	static FString GetStartupLanguage(const FString& SystemCulture);
	/**
	 * 언어를 바로 바꾸고 GameUserSettings.ini 에 저장한다. 지원하지 않는 컬처거나 바꾸지 못하면 false.
	 * 에디터(PIE)는 에디터와 같은 프로세스라 컬처를 바꾸면 에디터 화면 언어까지 바뀐다. 그래서 에디터에서는
	 * 게임 문구만 바꾸는 엔진의 게임 언어 미리보기(Game Localization Preview)를 쓴다.
	 * 미리보기에서는 폰트의 컬처별 Sub-Font 가 에디터 언어 기준으로 골라지므로, 글꼴 확인은 Standalone/패키징에서 한다.
	 */
	static bool SetLanguage(const FString& Culture);
	/** GameUserSettings.ini 에 저장된 언어. 없으면 빈 문자열. */
	static FString GetSavedLanguage();
};
