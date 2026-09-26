#include "DFContentTestUtil.h"
#include "Content/DFContentSubsystem.h"
#include "Engine/GameInstance.h"

#include "DFContentTables.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace DFContentTest
{
	UDataTable* LoadTable(FAutomationTestBase& Test, const FString& Table)
	{
		const FString PackageName = DFContentTables::PackageNameFor(Table);
		if (!FPackageName::DoesPackageExist(PackageName))
		{
			Test.AddError(FString::Printf(TEXT("%s: %s has not been imported (run -run=DFContentImport)"), *Table, *PackageName));
			return nullptr;
		}
		UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *DFContentTables::ObjectPathFor(Table));
		if (!DataTable)
		{
			Test.AddError(FString::Printf(TEXT("%s: %s is not a loadable DataTable"), *Table, *PackageName));
		}
		return DataTable;
	}

	TSharedPtr<FJsonObject> LoadJsonObject(FAutomationTestBase& Test, const FString& File)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			Test.AddError(FString::Printf(TEXT("cannot read %s"), *File));
			return nullptr;
		}
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("%s: not valid JSON (%s)"), *File, *Reader->GetErrorMessage()));
			return nullptr;
		}
		return Object;
	}

	bool LoadContentIds(FAutomationTestBase& Test, TMap<FString, TArray<FString>>& OutIds)
	{
		const TSharedPtr<FJsonObject> Root = LoadJsonObject(Test, DFContentTables::ContentIdsPath());
		if (!Root.IsValid())
		{
			return false;
		}
		for (const auto& Pair : Root->Values)
		{
			const FString Table(Pair.Key);
			const TArray<TSharedPtr<FJsonValue>>* Ids = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetArray(Ids))
			{
				Test.AddError(FString::Printf(TEXT("content-ids.json: '%s' is not an array"), *Table));
				return false;
			}
			TArray<FString>& List = OutIds.Add(Table);
			for (const TSharedPtr<FJsonValue>& Id : *Ids)
			{
				List.Add(Id->AsString());
			}
		}
		return true;
	}
}

UDFContentSubsystem* DFContentTest::MakeSubsystem()
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	return NewObject<UDFContentSubsystem>(GameInstance);
}
