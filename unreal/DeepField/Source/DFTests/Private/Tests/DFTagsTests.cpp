// DF.Unit.Tags.* — the C1 registry (ADR-0009, ADR-0020): content ids map to their PascalCase tags,
// every message in the C15 inventory (DFMessageList.inl) registered a live tag under DF.Message, and
// every tag the Config/Tags ini sources and the native list declare actually registered.

#include "DFGameplayTags.h"
#include "GameplayTagsManager.h"
#include "HAL/FileManager.h"
#include "Messages/DFMessages.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTagsContentIdMappingTest, "DF.Unit.Tags.ContentIdMapping",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDFTagsContentIdMappingTest::RunTest(const FString& Parameters)
{
	// The JSON spells ids lower camelCase; the tag is the same word with its first letter upper-cased.
	TestTrue(TEXT("emberPistol -> DF.Weapon.EmberPistol"),
		DFTags::ForContentId(TEXT("DF.Weapon"), TEXT("emberPistol")) == DFTags::Weapon_EmberPistol.GetTag());
	TestTrue(TEXT("lance -> DF.Tower.Lance"),
		DFTags::ForContentId(TEXT("DF.Tower"), TEXT("lance")) == DFTags::Tower_Lance.GetTag());
	TestTrue(TEXT("forge -> DF.Faction.Forge"),
		DFTags::ForContentId(TEXT("DF.Faction"), TEXT("forge")) == DFTags::Faction_Forge.GetTag());

	// Unknown ids, ids under the wrong root, and empty ids are invalid tags — never a silent parent.
	TestFalse(TEXT("unknown id is invalid"), DFTags::ForContentId(TEXT("DF.Weapon"), TEXT("noSuchWeapon")).IsValid());
	TestFalse(TEXT("known id under the wrong root is invalid"), DFTags::ForContentId(TEXT("DF.Tower"), TEXT("emberPistol")).IsValid());
	TestFalse(TEXT("empty id is invalid"), DFTags::ForContentId(TEXT("DF.Weapon"), NAME_None).IsValid());
	TestFalse(TEXT("unknown root is invalid"), DFTags::ForContentId(TEXT("DF.NoSuchRoot"), TEXT("lance")).IsValid());

	// Already-PascalCase input maps too (the importer may hand either spelling through).
	TestTrue(TEXT("EmberPistol -> DF.Weapon.EmberPistol"),
		DFTags::ForContentId(TEXT("DF.Weapon"), TEXT("EmberPistol")) == DFTags::Weapon_EmberPistol.GetTag());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTagsMessageInventoryHasTagPerEntryTest, "DF.Unit.Tags.MessageInventoryHasTagPerEntry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDFTagsMessageInventoryHasTagPerEntryTest::RunTest(const FString& Parameters)
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// Walk the X-macro itself: every DF_MSG entry must have produced a valid native tag whose string
	// is the one the list spells, and must name a real payload struct.
	int32 Entries = 0;
	TSet<FGameplayTag> ListedTags;
#define DF_MSG(Name, Str, Struct) \
	{ \
		++Entries; \
		const FString TagStr(Str); \
		const FGameplayTag Tag = DFTags::Message_##Name.GetTag(); \
		TestTrue(FString::Printf(TEXT("%s: native tag is valid"), *TagStr), Tag.IsValid()); \
		TestEqual(FString::Printf(TEXT("%s: native tag spells its string"), *TagStr), Tag.GetTagName(), FName(*TagStr)); \
		TestTrue(FString::Printf(TEXT("%s: the manager resolves the string to the same tag"), *TagStr), \
			Manager.RequestGameplayTag(FName(*TagStr), /*ErrorIfNotFound*/ false) == Tag); \
		TestTrue(FString::Printf(TEXT("%s: is a child of DF.Message"), *TagStr), Tag.MatchesTag(DFTags::Message)); \
		TestNotNull(FString::Printf(TEXT("%s: payload struct %s is reflected"), *TagStr, *FString(#Struct)), Struct::StaticStruct()); \
		bool bDuplicate = false; \
		ListedTags.Add(Tag, &bDuplicate); \
		TestFalse(FString::Printf(TEXT("%s: listed once"), *TagStr), bDuplicate); \
	}
#include "Messages/DFMessageList.inl"
#undef DF_MSG

	// And from the manager's side: every tag registered under DF.Message is one the list declares,
	// and the inventory is the size the contract promises (Events.cs has ~100 records; messages.md
	// adds eight). A native tag defined outside the list, or a list entry the manager lost, both fail.
	const FGameplayTagContainer Children = Manager.RequestGameplayTagChildren(DFTags::Message);
	TestTrue(TEXT("the manager registered at least 100 tags under DF.Message"), Children.Num() >= 100);
	TestEqual(TEXT("DF_MSG entries in DFMessageList.inl"), Entries, Children.Num());
	for (const FGameplayTag& Child : Children)
	{
		TestTrue(FString::Printf(TEXT("%s is declared in DFMessageList.inl"), *Child.ToString()), ListedTags.Contains(Child));
	}
	AddInfo(FString::Printf(TEXT("%d message tags under DF.Message"), Children.Num()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTagsEveryIniTagRegistersTest, "DF.Unit.Tags.EveryIniTagRegisters",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDFTagsEveryIniTagRegistersTest::RunTest(const FString& Parameters)
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// Both registration paths are pinned here. The loose sources under Config/Tags/ are read one file
	// at a time (GameplayTagsManager.cpp:582, LoadConfig on that file), so a row written in the
	// `+GameplayTagList=` combination form of the Default*.ini hierarchy registers nothing — that is
	// how DF_Gameplay.ini's 47 cue tags and DF_Online.ini's connection states were silently absent
	// (INT, 2026-09-21). Every `Tag="..."` a DF_*.ini spells must resolve.
	const FString TagsDir = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Tags"));
	TArray<FString> IniFiles;
	IFileManager::Get().FindFiles(IniFiles, *FPaths::Combine(TagsDir, TEXT("DF_*.ini")), /*Files*/ true, /*Directories*/ false);
	IniFiles.Sort();
	TestTrue(FString::Printf(TEXT("found DF_*.ini tag sources under %s"), *TagsDir), IniFiles.Num() > 0);

	int32 IniTags = 0;
	for (const FString& File : IniFiles)
	{
		TArray<FString> Lines;
		if (!TestTrue(FString::Printf(TEXT("%s: readable"), *File),
				FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(TagsDir, File))))
		{
			continue;
		}
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString Line = Lines[LineIndex].TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(TEXT(";")) || Line.StartsWith(TEXT("[")))
			{
				continue;
			}
			const FString Where = FString::Printf(TEXT("%s:%d"), *File, LineIndex + 1);
			TestFalse(FString::Printf(TEXT("%s: uses the plain GameplayTagList= key (a '+' row in a loose tag source registers nothing)"), *Where),
				Line.StartsWith(TEXT("+GameplayTagList=")));

			const int32 Open = Line.Find(TEXT("Tag=\""), ESearchCase::CaseSensitive);
			if (Open == INDEX_NONE)
			{
				continue;
			}
			const int32 NameStart = Open + 5;
			const int32 Close = Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, NameStart);
			if (!TestTrue(FString::Printf(TEXT("%s: Tag=\"...\" is closed"), *Where), Close != INDEX_NONE))
			{
				continue;
			}
			const FString TagStr = Line.Mid(NameStart, Close - NameStart);
			++IniTags;
			TestTrue(FString::Printf(TEXT("%s: %s is registered"), *Where, *TagStr),
				Manager.RequestGameplayTag(FName(*TagStr), /*ErrorIfNotFound*/ false).IsValid());
		}
	}

	// The native path: every DF_TAG in DFGameplayTagList.inl produced a valid tag that the manager
	// resolves from the string it spells.
	int32 NativeTags = 0;
#define DF_TAG(Name, Str) \
	{ \
		++NativeTags; \
		const FString TagStr(Str); \
		const FGameplayTag Tag = DFTags::Name.GetTag(); \
		TestTrue(FString::Printf(TEXT("%s: native tag is valid"), *TagStr), Tag.IsValid()); \
		TestTrue(FString::Printf(TEXT("%s: the manager resolves the string to the native tag"), *TagStr), \
			Manager.RequestGameplayTag(FName(*TagStr), /*ErrorIfNotFound*/ false) == Tag); \
	}
#include "DFGameplayTagList.inl"
#undef DF_TAG

	AddInfo(FString::Printf(TEXT("%d ini tags in %d files, %d native tags"), IniTags, IniFiles.Num(), NativeTags));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
