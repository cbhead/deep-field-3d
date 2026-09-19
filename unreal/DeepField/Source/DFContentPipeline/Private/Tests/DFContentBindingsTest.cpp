#include "Content/DFContentDefinition.h"
#include "Content/DFContentSubsystem.h"
#include "DFContentTables.h"
#include "DFContentTestUtil.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// For every id in every table that has a PrimaryAssetType (DefaultGame.ini), the binding asset
// (DA_<Domain>_<id>, a UDFContentDefinition) is looked up the way gameplay does, through
// UDFContentSubsystem::Definition. A missing binding is a warning ("unbound: Tower lance") because the
// bindings are other workstreams' deliverables; a binding that exists but is flagged bPlaceholder fails,
// because a stand-in must never reach a cook (C§7.5).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFContentBindingsTest, "DF.Content.Bindings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDFContentBindingsTest::RunTest(const FString& Parameters)
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		AddError(TEXT("the Asset Manager is not initialised"));
		return false;
	}
	TArray<FPrimaryAssetTypeInfo> Types;
	AssetManager->GetPrimaryAssetTypeInfoList(Types);

	// Definition() needs no tables, only the Asset Manager; a transient instance is enough.
	UDFContentSubsystem* Content = NewObject<UDFContentSubsystem>(GetTransientPackage());

	int32 Bound = 0;
	int32 Unbound = 0;
	int32 TypesChecked = 0;
	for (const FPrimaryAssetTypeInfo& Info : Types)
	{
		const FString Table = DFContentTables::TableForPrimaryAssetType(Info.PrimaryAssetType);
		if (Table.IsEmpty())
		{
			continue;   // levels and anything else that is not a content binding
		}
		UDataTable* DataTable = DFContentTest::LoadTable(*this, Table);
		if (!DataTable)
		{
			continue;
		}
		++TypesChecked;
		const FString Type = Info.PrimaryAssetType.ToString();
		for (const FName& Id : DataTable->GetRowNames())
		{
			const FPrimaryAssetId AssetId(Info.PrimaryAssetType, Id);
			const FSoftObjectPath Path = AssetManager->GetPrimaryAssetPath(AssetId);
			if (!Path.IsValid())
			{
				++Unbound;
				AddWarning(FString::Printf(TEXT("unbound: %s %s"), *Type, *Id.ToString()));
				continue;
			}
			UDFContentDefinition* Definition = Content->Definition(Info.PrimaryAssetType, Id);
			if (!Definition)
			{
				AddError(FString::Printf(TEXT("%s %s: %s is not a UDFContentDefinition"), *Type, *Id.ToString(), *Path.ToString()));
				continue;
			}
			if (Definition->bPlaceholder)
			{
				AddError(FString::Printf(TEXT("placeholder: %s %s (%s) has bPlaceholder=true"), *Type, *Id.ToString(), *Path.ToString()));
			}
			if (Definition->ContentId != Id || Definition->PrimaryType.GetName() != Info.PrimaryAssetType)
			{
				AddError(FString::Printf(TEXT("%s %s: %s declares %s:%s"), *Type, *Id.ToString(), *Path.ToString(), *Definition->PrimaryType.ToString(), *Definition->ContentId.ToString()));
			}
			++Bound;
		}
	}
	AddInfo(FString::Printf(TEXT("bindings: %d bound, %d unbound across %d primary asset type(s)"), Bound, Unbound, TypesChecked));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
