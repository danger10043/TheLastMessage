#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "UI/AudioSettingsWidget.h"
#include "UI/GameLanguageSettingsSubsystem.h"
// +수정----
#include "UI/FactoryUIStyle.h"
// +--------
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
bool HasSubFont(const UFont* Font, const TCHAR* Cultures, int32 Codepoint)
{
	if (!Font) return false;
	for (const FCompositeSubFont& SubFont : Font->GetInternalCompositeFont().SubTypefaces)
	{
		if (SubFont.Cultures != Cultures || SubFont.Typeface.Fonts.IsEmpty()) continue;
		for (const FInt32Range& Range : SubFont.CharacterRanges)
			if (Range.Contains(Codepoint)) return true;
	}
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLanguageSettingsTabTest, "TheLastMessage.UI.LanguageSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLanguageSettingsTabTest::RunTest(const FString& Parameters)
{
	// 1. 지원 언어와 컬처 맞추기 (Docs/LanguageSettingsPlan.md 1, 3절)
	using FLanguages = UGameLanguageSettingsSubsystem;
	const TConstArrayView<FGameLanguage> Languages = FLanguages::GetSupportedLanguages();
	TestEqual(TEXT("Twelve supported languages"), Languages.Num(), 12);
	TestEqual(TEXT("Korean is the source language"), FString(Languages[0].Culture), FString(TEXT("ko")));
	const auto CultureOf = [&](const TCHAR* Culture)
	{
		const int32 Index = FLanguages::FindSupportedLanguage(Culture);
		return Index == INDEX_NONE ? FString() : FString(Languages[Index].Culture);
	};
	TestEqual(TEXT("ko-KR -> ko"), CultureOf(TEXT("ko-KR")), FString(TEXT("ko")));
	TestEqual(TEXT("en-GB -> en"), CultureOf(TEXT("en-GB")), FString(TEXT("en")));
	TestEqual(TEXT("zh-CN -> zh-Hans"), CultureOf(TEXT("zh-CN")), FString(TEXT("zh-Hans")));
	TestEqual(TEXT("zh-TW -> zh-Hant"), CultureOf(TEXT("zh-TW")), FString(TEXT("zh-Hant")));
	TestEqual(TEXT("zh-HK -> zh-Hant"), CultureOf(TEXT("zh-HK")), FString(TEXT("zh-Hant")));
	TestEqual(TEXT("es-MX -> es-419"), CultureOf(TEXT("es-MX")), FString(TEXT("es-419")));
	TestEqual(TEXT("pt-PT -> pt-BR"), CultureOf(TEXT("pt-PT")), FString(TEXT("pt-BR")));
	TestEqual(TEXT("it-IT -> it"), CultureOf(TEXT("it-IT")), FString(TEXT("it")));
	TestEqual(TEXT("tr-TR -> tr"), CultureOf(TEXT("tr-TR")), FString(TEXT("tr")));
	TestEqual(TEXT("Unsupported culture"), CultureOf(TEXT("nl-NL")), FString());
	TestEqual(TEXT("First run uses a supported system language"), FLanguages::GetStartupLanguage(TEXT("ja-JP")), FString(TEXT("ja")));
	TestEqual(TEXT("First run falls back to English"), FLanguages::GetStartupLanguage(TEXT("nl-NL")), FString(TEXT("en")));
	TestEqual(TEXT("Widget names use underscores"), FLanguages::GetLanguageWidgetName(Languages[2]), FName(TEXT("Language_zh_Hans")));

	// 2. 폰트 Sub-Font Family (5절)
	const auto* Title = LoadObject<UFont>(nullptr, TEXT("/Game/Main/UI/Fonts/F_TLM_Title"));
	const auto* UI = LoadObject<UFont>(nullptr, TEXT("/Game/Main/UI/Fonts/F_TLM_UI"));
	const auto* Body = LoadObject<UFont>(nullptr, TEXT("/Game/Main/UI/Fonts/F_TLM_Body"));
	if (TestNotNull(TEXT("Title font"), Title) && TestNotNull(TEXT("UI font"), UI) && TestNotNull(TEXT("Body font"), Body))
	{
		for (const UFont* Font : {Title, UI})
		{
			TestTrue(Font->GetName() + TEXT(" has a Cyrillic sub-font"), HasSubFont(Font, TEXT(""), 0x0416));
		}
		for (const UFont* Font : {Title, UI, Body})
		{
			TestTrue(Font->GetName() + TEXT(" has a Japanese sub-font"), HasSubFont(Font, TEXT("ja"), 0x3042));
			TestTrue(Font->GetName() + TEXT(" has a Simplified Chinese sub-font"), HasSubFont(Font, TEXT("zh-Hans"), 0x4E2D));
			TestTrue(Font->GetName() + TEXT(" has a Traditional Chinese sub-font"), HasSubFont(Font, TEXT("zh-Hant"), 0x9AD4));
		}
		// 다른 언어 화면에서도 언어 버튼의 한자 이름이 보이도록 본문 폰트의 Fallback은 한자가 있는 Noto Sans SC다.
		const auto& Fallback = Body->GetInternalCompositeFont().FallbackTypeface.Typeface.Fonts;
		const UObject* FallbackFace = Fallback.IsEmpty() ? nullptr : Fallback[0].Font.GetFontFaceAsset();
		TestTrue(TEXT("Body fallback covers CJK names"), FallbackFace && FallbackFace->GetName() == TEXT("FF_NotoSansSC_Regular"));
	}

	// 3. 설정 화면의 언어 탭 (2, 3, 7절)
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	struct FTestState
	{
		TStrongObjectPtr<UAudioSettingsWidget> Widget;
		TSharedPtr<SWidget> Slate;
		FString OriginalLanguage, OriginalLocale, OriginalSaved, First;
		bool bHadSaved = false;
	};
	const TSharedRef<FTestState> State = MakeShared<FTestState>();
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, State]
	{
		UWorld* PlayWorld = GEditor->PlayWorld;
		APlayerController* Controller = PlayWorld ? UGameplayStatics::GetPlayerController(PlayWorld, 0) : nullptr;
		auto* Widget = UAudioSettingsWidget::CreateFor(Controller);
		if (!TestNotNull(TEXT("Settings widget"), Widget)) return;
		const TSharedRef<SWidget> SettingsSlate = Widget->TakeWidget();
		auto* Pages = Cast<UWidgetSwitcher>(Widget->FindSettingsWidget(TEXT("SettingsTabPages")));
		auto* Tab = Cast<UButton>(Widget->FindSettingsWidget(TEXT("LanguageSettingsTab")));
		auto* TabText = Cast<UTextBlock>(Widget->FindSettingsWidget(TEXT("LanguageSettingsTabText")));
		auto* Section = Cast<UUserWidget>(Widget->FindSettingsWidget(TEXT("LanguageSettingsSection")));
		if (!TestNotNull(TEXT("Settings pages"), Pages) || !TestNotNull(TEXT("Language tab"), Tab)
			|| !TestNotNull(TEXT("Language tab text"), TabText) || !TestNotNull(TEXT("Language page Blueprint"), Section)) return;
		TestEqual(TEXT("Five settings pages"), Pages->GetChildrenCount(), 5);
		TestTrue(TEXT("Language page is a separate Widget Blueprint"),
			Section->GetClass()->GetPathName().StartsWith(TEXT("/Game/Main/UI/Menu/WBP_LanguageSettings.")));
		TestTrue(TEXT("Language tab caption matches the other tabs"), FMath::IsNearlyEqual(TabText->GetRenderTransform().Translation.Y, -4.f));
		Tab->OnClicked.Broadcast();
		TestEqual(TEXT("Language tab opens the fifth page"), Pages->GetActiveWidgetIndex(), 4);
		TestTrue(TEXT("Language page is shown"), Pages->GetActiveWidget() == Section);
		TestTrue(TEXT("Language tab is highlighted"), TabText->GetColorAndOpacity().GetSpecifiedColor().Equals(FactoryUI::Cyan));

		const TConstArrayView<FGameLanguage> Languages = UGameLanguageSettingsSubsystem::GetSupportedLanguages();
		auto* Grid = Cast<UUniformGridPanel>(Widget->FindSettingsWidget(TEXT("LanguageGrid")));
		TestTrue(TEXT("Twelve buttons in a 2 x 6 grid"), Grid && Grid->GetChildrenCount() == 12);
		int32 ButtonCount = 0;
		for (const FGameLanguage& Language : Languages)
		{
			const FString Name = UGameLanguageSettingsSubsystem::GetLanguageWidgetName(Language).ToString();
			auto* Button = Cast<UButton>(Widget->FindSettingsWidget(*Name));
			auto* Label = Cast<UTextBlock>(Widget->FindSettingsWidget(*(Name + TEXT("_Name"))));
			if (!TestNotNull(*Name, Button) || !TestNotNull(*(Name + TEXT("_Name")), Label)) continue;
			++ButtonCount;
			TestTrue(Name + TEXT(" is focusable for keyboard navigation"), Button->GetIsFocusable());
			TestEqual(Name + TEXT(" shows the native name"), Label->GetText().ToString(), FString(Language.NativeName));
			TestNull(*(Name + TEXT(" has no flag")), Widget->FindSettingsWidget(*(Name + TEXT("_Flag"))));
			TestNull(*(Name + TEXT(" has no badge")), Widget->FindSettingsWidget(*(Name + TEXT("_Badge"))));
		}
		TestEqual(TEXT("All language buttons exist"), ButtonCount, 12);

		// 고르면 바로 바뀌고 저장된다. 테스트가 끝나면 원래 언어와 저장 값을 되돌린다.
		State->Widget.Reset(Widget);
		State->Slate = SettingsSlate;
		State->OriginalLanguage = FInternationalization::Get().GetCurrentLanguage()->GetName();
		State->OriginalLocale = FInternationalization::Get().GetCurrentLocale()->GetName();
		State->bHadSaved = GConfig->GetString(TEXT("Internationalization"), TEXT("Culture"), State->OriginalSaved, GGameUserSettingsIni);
		State->First = UGameLanguageSettingsSubsystem::GetCurrentLanguage() == TEXT("en") ? TEXT("de") : TEXT("en");
		const FString Previous = UGameLanguageSettingsSubsystem::GetCurrentLanguage();
		const FString TargetName = TEXT("Language_") + State->First;
		auto* TargetButton = Cast<UButton>(Widget->FindSettingsWidget(*TargetName));
		if (!TestNotNull(*TargetName, TargetButton)) return;
		TargetButton->OnClicked.Broadcast();
		TestEqual(TEXT("Selecting a language switches the culture"), UGameLanguageSettingsSubsystem::GetCurrentLanguage(), State->First);
		TestEqual(TEXT("Selecting a language saves it"), UGameLanguageSettingsSubsystem::GetSavedLanguage(), State->First);
		// PIE는 에디터와 같은 프로세스다. 게임 언어만 바뀌고 에디터 화면 언어는 그대로여야 한다.
		TestEqual(TEXT("Editor language is unchanged in PIE"), FInternationalization::Get().GetCurrentLanguage()->GetName(), State->OriginalLanguage);
		const auto* Label = Cast<UTextBlock>(Widget->FindSettingsWidget(*(TargetName + TEXT("_Name"))));
		TestTrue(TEXT("Current language name is cyan"), Label && Label->GetColorAndOpacity().GetSpecifiedColor().Equals(FactoryUI::Cyan));
		TestTrue(TEXT("Current language has a cyan border"),
			TargetButton->GetStyle().Disabled.OutlineSettings.Color.GetSpecifiedColor().Equals(FactoryUI::Cyan));
		if (!Previous.IsEmpty())
		{
			const auto* PreviousLabel = Cast<UTextBlock>(Widget->FindSettingsWidget(*(TEXT("Language_") + Previous.Replace(TEXT("-"), TEXT("_")) + TEXT("_Name"))));
			TestTrue(TEXT("Previous language is no longer highlighted"), PreviousLabel
				&& PreviousLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(FactoryUI::Text));
		}
	}, 0.2f));
	// 엔진은 번역 데이터를 비동기로 읽는다. 다 읽은 뒤 드롭다운(문자열로 채움)이 고른 언어로 바뀌었는지 본다.
	// 예전에는 컬처 변경 즉시 다시 채워서 이전 언어 문자열이 들어갔다(영어를 고르면 한국어, 그다음 일본어를 고르면 영어).
	const auto ExpectFrameLimitLabel = [this, State](const FString& Culture)
	{
		// Content/Localization/Game 의 초안 번역 "제한 없음"
		const FString Expected = Culture == TEXT("en") ? TEXT("Unlimited") : Culture == TEXT("de") ? TEXT("Unbegrenzt") : TEXT("無制限");
		const auto* FrameLimit = State->Widget ? Cast<UComboBoxString>(State->Widget->FindSettingsWidget(TEXT("FrameLimitSetting"))) : nullptr;
		TestTrue(FString::Printf(TEXT("Dropdowns are refilled in %s"), *Culture), FrameLimit && FrameLimit->GetOptionCount() == 4);
		if (FrameLimit && FrameLimit->GetOptionCount() == 4)
			TestEqual(FString::Printf(TEXT("Frame limit option is in %s"), *Culture), FrameLimit->GetOptionAtIndex(0), Expected);
		const auto* Graphics = State->Widget ? Cast<UTextBlock>(State->Widget->FindSettingsWidget(TEXT("GeneralSettingsTabText"))) : nullptr;
		const FString ExpectedTab = Culture == TEXT("en") ? TEXT("Graphics") : Culture == TEXT("de") ? TEXT("Grafik") : TEXT("グラフィック");
		TestTrue(FString::Printf(TEXT("Graphics tab caption is in %s"), *Culture), Graphics && Graphics->GetText().ToString() == ExpectedTab);
	};
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, State, ExpectFrameLimitLabel]
	{
		ExpectFrameLimitLabel(State->First);
		if (auto* Japanese = State->Widget ? Cast<UButton>(State->Widget->FindSettingsWidget(TEXT("Language_ja"))) : nullptr)
			Japanese->OnClicked.Broadcast();
	}, 1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, State, ExpectFrameLimitLabel]
	{
		ExpectFrameLimitLabel(TEXT("ja"));
		UGameLanguageSettingsSubsystem::ClearEditorPreview();
		FInternationalization::Get().SetCurrentLanguage(State->OriginalLanguage);
		FInternationalization::Get().SetCurrentLocale(State->OriginalLocale);
		if (State->bHadSaved) GConfig->SetString(TEXT("Internationalization"), TEXT("Culture"), *State->OriginalSaved, GGameUserSettingsIni);
		else GConfig->RemoveKey(TEXT("Internationalization"), TEXT("Culture"), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		TestEqual(TEXT("Original language restored"), FInternationalization::Get().GetCurrentLanguage()->GetName(), State->OriginalLanguage);
		State->Widget.Reset();
		State->Slate.Reset();
	}, 1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
