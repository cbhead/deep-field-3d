#include "DFContentTestUtil.h"
#include "DFGameplayTags.h"
#include "GameplayTagsManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Every content id in content-ids.json resolves to a native tag under its C1 root (DF.Tower.Lance for
// towers/lance). A root that does not exist yet (attachments have none) is reported, not failed: the id
// list is still right, the tag registry just has not caught up (an append to DFGameplayTagList.inl).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFContentTagCoverageTest, "DF.Content.TagCoverage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDFContentTagCoverageTest::RunTest(const FString& Parameters)
{
	TMap<FString, TArray<FString>> ContentIds;
	if (!DFContentTest::LoadContentIds(*this, ContentIds))
	{
		return false;
	}

	struct FRoot
	{
		const TCHAR* Table;
		const TCHAR* Root;
	};
	static const FRoot Roots[] = {
		{ TEXT("towers"),           TEXT("DF.Tower") },
		{ TEXT("traps"),            TEXT("DF.Trap") },
		{ TEXT("enemies"),          TEXT("DF.Enemy") },
		{ TEXT("statuses"),         TEXT("DF.Status") },
		{ TEXT("reactions"),        TEXT("DF.Reaction") },
		{ TEXT("factions"),         TEXT("DF.Faction") },
		{ TEXT("weapons"),          TEXT("DF.Weapon") },
		{ TEXT("melee"),            TEXT("DF.Melee") },
		{ TEXT("attachments"),      TEXT("DF.Attachment") },
		{ TEXT("meleeAttachments"), TEXT("DF.MeleeAttachment") },
		{ TEXT("ammo"),             TEXT("DF.Ammo") },
		{ TEXT("conditions"),       TEXT("DF.Condition") },
	};

	UGameplayTagsManager& Tags = UGameplayTagsManager::Get();
	for (const FRoot& Spec : Roots)
	{
		const TArray<FString>* Ids = ContentIds.Find(Spec.Table);
		if (!Ids)
		{
			AddError(FString::Printf(TEXT("content-ids.json has no '%s' list"), Spec.Table));
			continue;
		}
		const bool bRootExists = Tags.RequestGameplayTag(FName(Spec.Root), /*ErrorIfNotFound*/ false).IsValid();
		int32 Missing = 0;
		for (const FString& Id : *Ids)
		{
			if (DFTags::ForContentId(Spec.Root, FName(*Id)).IsValid())
			{
				continue;
			}
			++Missing;
			if (bRootExists)
			{
				FString Pascal = Id;
				Pascal[0] = FChar::ToUpper(Pascal[0]);
				AddError(FString::Printf(TEXT("%s '%s' has no native tag %s.%s (append it to DFGameplayTagList.inl)"), Spec.Table, *Id, Spec.Root, *Pascal));
			}
		}
		if (bRootExists)
		{
			AddInfo(FString::Printf(TEXT("%s: %d/%d ids have native tags under %s"), Spec.Table, Ids->Num() - Missing, Ids->Num(), Spec.Root));
		}
		else
		{
			AddInfo(FString::Printf(TEXT("informational: no tag root %s yet, so %d %s id(s) are unchecked"), Spec.Root, Ids->Num(), Spec.Table));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
