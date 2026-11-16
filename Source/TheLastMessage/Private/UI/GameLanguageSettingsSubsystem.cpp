#include "UI/GameLanguageSettingsSubsystem.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

namespace
{
const TCHAR* const ConfigSection = TEXT("Internationalization");
const TCHAR* const ConfigKey = TEXT("Culture");

// 순서: 2열 그리드에 왼쪽에서 오른쪽, 위에서 아래로 놓인다. 국기만으로는 언어가 구분되지 않아 항상 이름과 함께 보여 준다.
const FGameLanguage SupportedLanguages[] = {
	{TEXT("ko"), TEXT("한국어"), TEXT("kr")},
	{TEXT("en"), TEXT("English"), TEXT("us")},
	{TEXT("zh-Hans"), TEXT("简体中文"), TEXT("cn")},
	// 중국 출시 시 대만 국기 표기가 문제될 수 있어 국기 대신 "繁" 배지를 쓴다.
	{TEXT("zh-Hant"), TEXT("繁體中文"), nullptr},
	{TEXT("ja"), TEXT("日本語"), TEXT("jp")},
	{TEXT("ru"), TEXT("Русский"), TEXT("ru")},
	{TEXT("es-419"), TEXT("Español (Latinoamérica)"), TEXT("mx")},
	{TEXT("pt-BR"), TEXT("Português (Brasil)"), TEXT("br")},
	{TEXT("de"), TEXT("Deutsch"), TEXT("de")},
	{TEXT("fr"), TEXT("Français"), TEXT("fr")},
};

int32 FindExact(const FString& Culture)
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SupportedLanguages); ++Index)
		if (Culture.Equals(SupportedLanguages[Index].Culture, ESearchCase::IgnoreCase)) return Index;
	return INDEX_NONE;
}
}

void UGameLanguageSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 에디터(PIE 포함)에서 바꾸면 에디터 화면 언어까지 바뀌므로 패키징/단독 실행에서만 시작 언어를 정한다.
	if (GIsEditor) return;
	FString CommandLineCulture;
	if (!GetSavedLanguage().IsEmpty() || FParse::Value(FCommandLine::Get(), TEXT("CULTURE="), CommandLineCulture)) return;
	const FString SystemCulture = FInternationalization::Get().GetDefaultLanguage()->GetName();
	const FString Startup = GetStartupLanguage(SystemCulture);
	// 첫 실행에는 저장하지 않는다. 플레이어가 언어 설정에서 직접 고르면 그때 저장된다.
	FInternationalization::Get().SetCurrentCulture(Startup);
	UE_LOG(LogTemp, Display, TEXT("Language: no saved culture, system %s -> %s"), *SystemCulture, *Startup);
}

TConstArrayView<FGameLanguage> UGameLanguageSettingsSubsystem::GetSupportedLanguages()
{
	return SupportedLanguages;
}

FName UGameLanguageSettingsSubsystem::GetLanguageWidgetName(const FGameLanguage& Language)
{
	return FName(*(FString(TEXT("Language_")) + FString(Language.Culture).Replace(TEXT("-"), TEXT("_"))));
}

FString UGameLanguageSettingsSubsystem::GetFlagTexturePath(const FGameLanguage& Language)
{
	return Language.FlagCode
		? FString::Printf(TEXT("/Game/Main/UI/Flags/T_Flag_%s.T_Flag_%s"), Language.FlagCode, Language.FlagCode)
		: FString();
}

int32 UGameLanguageSettingsSubsystem::FindSupportedLanguage(const FString& Culture)
{
	if (const int32 Exact = FindExact(Culture); Exact != INDEX_NONE) return Exact;
	const FCulturePtr Info = FInternationalization::Get().GetCulture(Culture);
	if (!Info.IsValid()) return INDEX_NONE;
	const FString Language = Info->GetTwoLetterISOLanguageName();
	const FString Script = Info->GetScript();
	const FString Region = Info->GetRegion();
	// 지원 목록에는 언어마다 변형이 하나뿐이므로 같은 언어의 다른 지역(es-MX, es-ES, pt-PT 등)은 그 변형으로 본다.
	if (Language == TEXT("zh"))
	{
		const bool bTraditional = Script == TEXT("Hant") || Region == TEXT("TW") || Region == TEXT("HK") || Region == TEXT("MO");
		return FindExact(bTraditional ? TEXT("zh-Hant") : TEXT("zh-Hans"));
	}
	if (Language == TEXT("es")) return FindExact(TEXT("es-419"));
	if (Language == TEXT("pt")) return FindExact(TEXT("pt-BR"));
	return FindExact(Language);
}

FString UGameLanguageSettingsSubsystem::GetCurrentLanguage()
{
	const int32 Index = FindSupportedLanguage(FInternationalization::Get().GetCurrentLanguage()->GetName());
	return Index == INDEX_NONE ? FString() : FString(SupportedLanguages[Index].Culture);
}

FString UGameLanguageSettingsSubsystem::GetStartupLanguage(const FString& SystemCulture)
{
	const int32 Index = FindSupportedLanguage(SystemCulture);
	return Index == INDEX_NONE ? FString(TEXT("en")) : FString(SupportedLanguages[Index].Culture);
}

bool UGameLanguageSettingsSubsystem::SetLanguage(const FString& Culture)
{
	const int32 Index = FindExact(Culture);
	if (Index == INDEX_NONE || !FInternationalization::Get().SetCurrentCulture(SupportedLanguages[Index].Culture)) return false;
	// UKismetInternationalizationLibrary::SetCurrentCulture(Culture, true)와 같은 곳에 저장한다.
	// 엔진 함수는 에디터에서 저장을 건너뛰지만, 여기서는 PIE와 자동화 테스트에서도 저장 결과를 확인할 수 있도록 항상 저장한다.
	// (에디터는 시작할 때 이 값을 읽지 않으므로 에디터 언어에는 영향이 없다.)
	GConfig->SetString(ConfigSection, ConfigKey, SupportedLanguages[Index].Culture, GGameUserSettingsIni);
	GConfig->EmptySection(TEXT("Internationalization.AssetGroupCultures"), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	return true;
}

FString UGameLanguageSettingsSubsystem::GetSavedLanguage()
{
	FString Culture;
	GConfig->GetString(ConfigSection, ConfigKey, Culture, GGameUserSettingsIni);
	return Culture;
}
