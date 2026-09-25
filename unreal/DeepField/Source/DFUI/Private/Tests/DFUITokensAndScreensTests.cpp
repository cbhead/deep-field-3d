#include "Input/UIActionBindingHandle.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Screens/DFUILayout.h"
#include "Screens/DFUITags.h"
#include "Tests/DFUITestScreen.h"
#include "Tokens/DFUITokenNames.h"
#include "Tokens/DFUITokens.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags GFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	bool Near(const FLinearColor& Color, uint8 R, uint8 G, uint8 B)
	{
		return Color.ToFColorSRGB() == FColor(R, G, B, Color.ToFColorSRGB().A);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUITokensDesignSystem, "DF.UI.Tokens.NamesMatchDesignSystem", GFlags)
bool FDFUITokensDesignSystem::RunTest(const FString&)
{
	UDFUITokens* Tokens = NewObject<UDFUITokens>();
	TArray<FString> Errors;
	const bool bOk = Tokens->FillFromDesignSystem(UDFUITokens::DesignSystemDir(), Errors);
	for (const FString& Error : Errors)
	{
		AddError(Error);
	}
	TestTrue(FString::Printf(TEXT("the design system parses (%s)"), *UDFUITokens::DesignSystemDir()), bOk);

	// Code -> CSS: every name code may use is a token of the kind it says.
	TSet<FName> Listed;
	for (const DFTokens::FEntry& Entry : DFTokens::All())
	{
		Listed.Add(Entry.Name);
		if (!Tokens->Has(Entry.Name, Entry.Kind))
		{
			AddError(FString::Printf(TEXT("DFUITokenList.inl names --%s as %s, but the design system has no such token"), *Entry.Name.ToString(), *UEnum::GetValueAsString(Entry.Kind)));
		}
	}
	// CSS -> code: a token added to the design system gets a line (Raw values are read by name where needed).
	auto CheckListed = [&](const TCHAR* Kind, const TArray<FName>& Names)
	{
		for (const FName& Name : Names)
		{
			if (!Listed.Contains(Name))
			{
				AddError(FString::Printf(TEXT("the design system has %s --%s, but DFUITokenList.inl has no line for it"), Kind, *Name.ToString()));
			}
		}
	};
	TArray<FName> Names;
	Tokens->Colors.GetKeys(Names);    CheckListed(TEXT("colour"), Names);
	Tokens->Lengths.GetKeys(Names);   CheckListed(TEXT("length"), Names);
	Tokens->Durations.GetKeys(Names); CheckListed(TEXT("duration"), Names);
	Tokens->Easings.GetKeys(Names);   CheckListed(TEXT("easing"), Names);
	Tokens->Numbers.GetKeys(Names);   CheckListed(TEXT("number"), Names);

	// Spot values, including the ones that arrive through var().
	TestTrue(TEXT("surface-panel = obsidian-700 = #131926"), Near(Tokens->Color(DFTokens::SurfacePanel), 0x13, 0x19, 0x26));
	TestTrue(TEXT("text-accent = brass-400 = #E3BC66"), Near(Tokens->Color(DFTokens::TextAccent), 0xE3, 0xBC, 0x66));
	TestTrue(TEXT("surface-overlay = rgba(8,11,17,.78)"), Near(Tokens->Color(DFTokens::SurfaceOverlay), 8, 11, 17) && FMath::IsNearlyEqual(Tokens->Color(DFTokens::SurfaceOverlay).A, 0.78f));
	TestEqual(TEXT("space-6"), Tokens->Length(DFTokens::Space6), 16.f);
	TestEqual(TEXT("space-0 is a length"), Tokens->Length(DFTokens::Space0), 0.f);
	TestEqual(TEXT("hud-topbar-h"), Tokens->Length(DFTokens::HudTopbarH), 56.f);
	TestEqual(TEXT("chamfer-md"), Tokens->Length(DFTokens::ChamferMd), 10.f);
	TestEqual(TEXT("dur-fast = 140ms"), Tokens->Duration(DFTokens::DurFast), 0.14f);
	TestEqual(TEXT("weight-bold"), Tokens->Number(DFTokens::WeightBold), 700.f);
	TestEqual(TEXT("tracking-label = .14em"), Tokens->Number(DFTokens::TrackingLabel), 0.14f);
	TestTrue(TEXT("ease-out = cubic-bezier(.16,1,.3,1)"), Tokens->Easings.FindRef(DFTokens::EaseOut).Equals(FVector4(0.16, 1.0, 0.3, 1.0), 1e-6));
	TestTrue(TEXT("composite and look-rule values are kept verbatim"), Tokens->Has(TEXT("shadow-panel"), EDFUITokenKind::Raw) && Tokens->Has(TEXT("text-h1"), EDFUITokenKind::Raw) && Tokens->Raw.FindRef(TEXT("font-mono")).Contains(TEXT("JetBrains Mono")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUITokensParser, "DF.UI.Tokens.ParserAndFallbacks", GFlags)
bool FDFUITokensParser::RunTest(const FString&)
{
	UDFUITokens* Tokens = NewObject<UDFUITokens>();
	TArray<FString> Errors;

	// Two files, comments, var() chains, every value shape, and things that are not declarations.
	const bool bOk = Tokens->FillFromCss({
		TEXT(":root{/* ramp */--ink-900:#080B11;--ink:#abc;--glass:#11223380;\n--surface:var(--ink-900);--panel : var( --surface ) ;}"),
		TEXT(".btn{color:var(--ink);margin:-4px}:root{--gap:12px;--none:0;--fast:140ms;--slow:1.6s;--snap:cubic-bezier(.2,.9,.25,1);--bold:700;--track:.14em;--half:50%;"
		     "--shadow:0 2px 0 rgba(8,11,17,.9),0 8px 20px rgba(8,11,17,.55);--pad:0 16px;--font:'Oxanium',sans-serif;--last:rgb(1, 2, 3)}") }, Errors);
	TestTrue(FString::Printf(TEXT("well-formed CSS parses (%s)"), *FString::Join(Errors, TEXT(" | "))), bOk && Errors.Num() == 0);
	TestEqual(TEXT("17 declarations (the .btn rule adds nothing); the bare 0 sits in two tables"), Tokens->Num(), 17 + 1);
	TestTrue(TEXT("#RGB"), Near(Tokens->Color(TEXT("ink")), 0xAA, 0xBB, 0xCC));
	TestTrue(TEXT("#RRGGBBAA"), Near(Tokens->Color(TEXT("glass")), 0x11, 0x22, 0x33) && FMath::IsNearlyEqual(Tokens->Color(TEXT("glass")).A, 128.f / 255.f));
	TestTrue(TEXT("var() through two hops, with spaces"), Tokens->Color(TEXT("panel")) == Tokens->Color(TEXT("ink-900")));
	TestTrue(TEXT("rgb() with spaces, last declaration without a semicolon"), Near(Tokens->Color(TEXT("last")), 1, 2, 3));
	TestEqual(TEXT("px"), Tokens->Length(TEXT("gap")), 12.f);
	TestTrue(TEXT("a bare 0 is a length and a number"), Tokens->Has(TEXT("none"), EDFUITokenKind::Length) && Tokens->Has(TEXT("none"), EDFUITokenKind::Number));
	TestEqual(TEXT("ms"), Tokens->Duration(TEXT("fast")), 0.14f);
	TestEqual(TEXT("s"), Tokens->Duration(TEXT("slow")), 1.6f);
	TestEqual(TEXT("unitless"), Tokens->Number(TEXT("bold")), 700.f);
	TestEqual(TEXT("em"), Tokens->Number(TEXT("track")), 0.14f);
	TestEqual(TEXT("%"), Tokens->Number(TEXT("half")), 0.5f);
	TestTrue(TEXT("what is not one value stays raw"), Tokens->Has(TEXT("shadow"), EDFUITokenKind::Raw) && Tokens->Has(TEXT("pad"), EDFUITokenKind::Raw) && Tokens->Has(TEXT("font"), EDFUITokenKind::Raw));

	// Easing: endpoints, symmetry, overshoot allowed in y.
	TestEqual(TEXT("ease(0)"), Tokens->Ease(TEXT("snap"), 0.f), 0.f);
	TestEqual(TEXT("ease(1)"), Tokens->Ease(TEXT("snap"), 1.f), 1.f);
	TestTrue(TEXT("snap is ahead of linear"), Tokens->Ease(TEXT("snap"), 0.3f) > 0.6f);
	TestTrue(TEXT("a symmetric curve crosses the middle"), FMath::IsNearlyEqual(UDFUITokens::EvaluateBezier(FVector4(0.65, 0, 0.35, 1), 0.5f), 0.5f, 1e-4f));
	TestTrue(TEXT("overshoot goes past 1"), UDFUITokens::EvaluateBezier(FVector4(0.34, 1.56, 0.64, 1), 0.7f) > 1.f);
	float Previous = 0.f;
	bool bMonotonic = true;
	for (int32 i = 1; i <= 20; ++i)
	{
		const float Y = UDFUITokens::EvaluateBezier(FVector4(0.16, 1, 0.3, 1), i / 20.f);
		bMonotonic &= Y >= Previous;
		Previous = Y;
	}
	TestTrue(TEXT("ease-out never goes backwards"), bMonotonic);

	// A missing token: one error naming it, and a value nobody mistakes for a design decision.
	AddExpectedError(TEXT("missing UI token: no colour named '--nope'"), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("missing colour is magenta"), Tokens->Color(TEXT("nope")) == FLinearColor(1.f, 0.f, 1.f, 1.f));
	Tokens->Color(TEXT("nope"));   // reported once
	AddExpectedError(TEXT("missing UI token: no easing named '--nope-ease'"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("missing easing is linear"), Tokens->Ease(TEXT("nope-ease"), 0.25f), 0.25f);

	// Refusals name the token.
	struct FCase { const TCHAR* What; const TCHAR* Css; const TCHAR* Expect; };
	const FCase Cases[] = {
		{ TEXT("unknown var"),   TEXT(":root{--a:var(--b);}"),                       TEXT("--a: var(--b) names no token") },
		{ TEXT("circular var"),  TEXT(":root{--a:var(--b);--b:var(--a);}"),          TEXT("circular var()") },
		{ TEXT("bad hex"),       TEXT(":root{--a:#12;}"),                            TEXT("--a: '#12' is not a valid hex colour") },
		{ TEXT("bad hex digit"), TEXT(":root{--a:#GGHHII;}"),                        TEXT("is not a valid hex colour") },
		{ TEXT("bad rgba"),      TEXT(":root{--a:rgba(1,2,3);}"),                    TEXT("is not a valid rgb()/rgba() colour") },
		{ TEXT("bad bezier"),    TEXT(":root{--a:cubic-bezier(.2,.9,1);}"),          TEXT("is not a valid cubic-bezier()") },
		{ TEXT("redefinition"),  TEXT(":root{--a:1px;}:root{--a:2px;}"),             TEXT("--a: defined twice ('1px' and '2px')") },
	};
	for (const FCase& Case : Cases)
	{
		TArray<FString> CaseErrors;
		const bool bCaseOk = NewObject<UDFUITokens>()->FillFromCss({ Case.Css }, CaseErrors);
		TestTrue(FString::Printf(TEXT("%s -> [%s]"), Case.What, *FString::Join(CaseErrors, TEXT(" | "))),
			!bCaseOk && CaseErrors.ContainsByPredicate([&Case](const FString& E) { return E.Contains(Case.Expect, ESearchCase::CaseSensitive); }));
	}
	TArray<FString> Same;
	TestTrue(TEXT("the same value twice is not a conflict"), NewObject<UDFUITokens>()->FillFromCss({ TEXT(":root{--a:1px;}"), TEXT(":root{--a:1px;}") }, Same));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUIScreensTags, "DF.UI.Screens.TagsResolve", GFlags)
bool FDFUIScreensTags::RunTest(const FString&)
{
	const FDFUITags& Tags = FDFUITags::Get();
	TestEqual(TEXT("four layers"), Tags.Layers().Num(), 4);
	// The Godot client's 17 screens + the teleport picker, split by whether they are exclusive on a
	// layer (screens) or drawn together over the world (parts).
	TestEqual(TEXT("13 screens"), Tags.Screens().Num(), 13);
	TestEqual(TEXT("5 HUD parts"), Tags.Parts().Num(), 5);
	for (const FGameplayTag& Part : Tags.Parts())
	{
		TestTrue(FString::Printf(TEXT("part tag resolves: %s"), *Part.ToString()), Part.IsValid());
		TestFalse(FString::Printf(TEXT("a part is not on a layer: %s"), *Part.ToString()), Tags.LayerOf(Part).IsValid());
	}
	for (const FGameplayTag& Layer : Tags.Layers())
	{
		TestTrue(TEXT("layer tag resolves (is it in Config/Tags/DF_UI.ini?)"), Layer.IsValid());
	}
	for (const FGameplayTag& Screen : Tags.Screens())
	{
		TestTrue(FString::Printf(TEXT("screen tag resolves and has a layer: %s"), *Screen.ToString()), Screen.IsValid() && Tags.Layers().Contains(Tags.LayerOf(Screen)));
	}
	TestTrue(TEXT("the HUD is play chrome, the connection modal is on top"), Tags.LayerOf(Tags.Screen_Hud) == Tags.Layer_Game && Tags.LayerOf(Tags.Screen_Connection) == Tags.Layer_Modal);

	// A layer displays one widget at a time, so two screens on a layer are mutually exclusive by
	// construction. Layer.Game must show the HUD *and* the crosshair *and* the prompts at once, so
	// it may hold exactly one screen - the HUD layout - and the rest are its parts. Putting the
	// chrome back on the layer would hide the HUD behind the crosshair, which is what this catches.
	TestEqual(TEXT("Layer.Game holds exactly one screen (the HUD; the chrome is parts inside it)"), Tags.ScreenCountOn(Tags.Layer_Game), 1);
	TestTrue(TEXT("the other layers hold mutually exclusive screens"), Tags.ScreenCountOn(Tags.Layer_GameMenu) == 5 && Tags.ScreenCountOn(Tags.Layer_Menu) == 6 && Tags.ScreenCountOn(Tags.Layer_Modal) == 1);
	TestFalse(TEXT("a layer is not a screen"), Tags.LayerOf(Tags.Layer_Menu).IsValid());

	// The ini and the code list the same tags: a tag only in the ini is dead, one only in code is invalid at runtime.
	FString Ini;
	const FString IniPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Tags"), TEXT("DF_UI.ini"));
	if (TestTrue(FString::Printf(TEXT("%s is readable"), *IniPath), FFileHelper::LoadFileToString(Ini, *IniPath)))
	{
		TArray<FString> InIni;
		int32 At = 0;
		const FString Marker = TEXT("(Tag=\"");
		while ((At = Ini.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart, At)) != INDEX_NONE)
		{
			const int32 End = Ini.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, At + Marker.Len());
			InIni.Add(Ini.Mid(At + Marker.Len(), End - At - Marker.Len()));
			At = End;
		}
		const TArray<FString> InCode = FDFUITags::DeclaredNames();
		for (const FString& Name : InCode)
		{
			TestTrue(FString::Printf(TEXT("%s is in DF_UI.ini"), *Name), InIni.Contains(Name));
		}
		for (const FString& Name : InIni)
		{
			TestTrue(FString::Printf(TEXT("%s (DF_UI.ini) is known to DFUITags"), *Name), InCode.Contains(Name));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUIScreensLayout, "DF.UI.Screens.LayersAndInput", GFlags)
bool FDFUIScreensLayout::RunTest(const FString&)
{
	const FDFUITags& Tags = FDFUITags::Get();

	// The layer registry. (Pushing needs a live Slate tree and a player: that is L_Test_UI's job, PR 3.)
	UDFUILayout* Layout = NewObject<UDFUILayout>();
	UCommonActivatableWidgetStack* Game = NewObject<UCommonActivatableWidgetStack>(Layout);
	UCommonActivatableWidgetStack* Other = NewObject<UCommonActivatableWidgetStack>(Layout);
	TestFalse(TEXT("no layers yet"), Layout->HasAllLayers());
	TestTrue(TEXT("register"), Layout->RegisterLayer(Tags.Layer_Game, Game) && Layout->GetLayer(Tags.Layer_Game) == Game);
	TestTrue(TEXT("registering the same container again is fine"), Layout->RegisterLayer(Tags.Layer_Game, Game));
	AddExpectedError(TEXT("already has a container"), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("a second container for a layer is refused"), !Layout->RegisterLayer(Tags.Layer_Game, Other) && Layout->GetLayer(Tags.Layer_Game) == Game);
	AddExpectedError(TEXT("cannot register layer"), EAutomationExpectedErrorFlags::Contains, 2);
	TestFalse(TEXT("a screen tag is not a layer"), Layout->RegisterLayer(Tags.Screen_Hud, Other));
	TestFalse(TEXT("no container"), Layout->RegisterLayer(Tags.Layer_Menu, nullptr));
	Layout->RegisterLayer(Tags.Layer_GameMenu, NewObject<UCommonActivatableWidgetStack>(Layout));
	Layout->RegisterLayer(Tags.Layer_Menu, NewObject<UCommonActivatableWidgetStack>(Layout));
	TestFalse(TEXT("three of four"), Layout->HasAllLayers());
	Layout->RegisterLayer(Tags.Layer_Modal, Other);
	TestTrue(TEXT("all four"), Layout->HasAllLayers());
	TestNull(TEXT("nothing is open"), Layout->FindOpenScreen(Tags.Screen_Hud));
	AddExpectedError(TEXT("has no DF.UI.Screen tag"), EAutomationExpectedErrorFlags::Contains, 1);
	TestNull(TEXT("a screen class with no tag is refused"), Layout->PushScreen(UDFUITestScreen::StaticClass()));

	// FindOpenScreen must return the displayed instance, never one buried under it: the widget list
	// grows upwards, so a forward search would hand back the oldest. AddWidgetInstance is the real
	// API and only touches the list when there is no Slate tree, so the order here is the true one.
	auto MakeScreen = [&](const FGameplayTag& Tag)
	{
		UDFUITestScreen* Made = NewObject<UDFUITestScreen>(Layout);
		Made->Configure(Tag, EDFScreenInputMode::Menu);
		return Made;
	};
	UDFUITestScreen* Buried = MakeScreen(Tags.Screen_Connection);
	UDFUITestScreen* Displayed = MakeScreen(Tags.Screen_Connection);
	Other->AddWidgetInstance(*Buried);
	Other->AddWidgetInstance(*Displayed);
	TestTrue(TEXT("the list grows upwards"), Other->GetWidgetList().Num() == 2 && Other->GetWidgetList().Last() == Displayed);
	TestTrue(TEXT("FindOpenScreen returns the topmost instance, not the buried one"), Layout->FindOpenScreen(Tags.Screen_Connection) == Displayed);
	TestNull(TEXT("a screen that is not open is not found"), Layout->FindOpenScreen(Tags.Screen_Lobby));

	// Pushing a screen that is already open would bury the live one under a copy nobody can see.
	AddExpectedMessagePlain(TEXT("is already open on"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	UDFUITestScreen::FScopedScreenTag Connection(Tags.Screen_Connection);
	TestTrue(TEXT("a duplicate push returns the open screen instead of stacking a copy"), Layout->PushScreen(UDFUITestScreen::StaticClass()) == Displayed);
	TestEqual(TEXT("and adds nothing"), Other->GetWidgetList().Num(), 2);

	// A screen's tag fixes its layer; its input mode is its CommonUI input config.
	UDFUITestScreen* Screen = NewObject<UDFUITestScreen>();
	Screen->Configure(Tags.Screen_Armory, EDFScreenInputMode::GameAndMenu);
	TestTrue(TEXT("armory lives on GameMenu"), Screen->GetLayerTag() == Tags.Layer_GameMenu);
	TOptional<FUIInputConfig> Config = Screen->GetDesiredInputConfig();
	TestTrue(TEXT("GameAndMenu -> All, mouse still captured by the game"), Config.IsSet() && Config->GetInputMode() == ECommonInputMode::All && Config->GetMouseCaptureMode() == EMouseCaptureMode::CapturePermanently);
	Screen->Configure(Tags.Screen_Pause, EDFScreenInputMode::Menu);
	Config = Screen->GetDesiredInputConfig();
	TestTrue(TEXT("Menu -> Menu, cursor free"), Config.IsSet() && Config->GetInputMode() == ECommonInputMode::Menu && Config->GetMouseCaptureMode() == EMouseCaptureMode::NoCapture);
	Screen->Configure(Tags.Screen_Wheel, EDFScreenInputMode::Game);
	Config = Screen->GetDesiredInputConfig();
	TestTrue(TEXT("Game -> Game"), Config.IsSet() && Config->GetInputMode() == ECommonInputMode::Game);
	Screen->Configure(Tags.Screen_Hud, EDFScreenInputMode::Default);
	TestFalse(TEXT("Default has no opinion"), Screen->GetDesiredInputConfig().IsSet());
	TestNull(TEXT("no match view model outside a game instance"), Screen->GetMatch());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
