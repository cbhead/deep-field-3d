#include "DFContentImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DFContentJsonShaper.h"
#include "DFContentPipelineModule.h"
#include "DFContentTables.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"

FDFContentImporter::FDFContentImporter(const FString& InJsonDir)
	: JsonDir(InJsonDir)
{
}

bool FDFContentImporter::LoadTableJson(const FString& JsonDir, const FString& Table, TArray<TSharedPtr<FJsonObject>>& OutRows, FString& OutError)
{
	const FString File = FPaths::Combine(JsonDir, Table + TEXT(".json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		OutError = FString::Printf(TEXT("cannot read %s"), *File);
		return false;
	}

	TSharedPtr<FJsonObject> Envelope;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Envelope) || !Envelope.IsValid())
	{
		OutError = FString::Printf(TEXT("%s: not valid JSON (%s)"), *File, *Reader->GetErrorMessage());
		return false;
	}

	// The C3 envelope: {"$schema", "table", "generated", "sourceSha"?, "rows"} and nothing else.
	static const TSet<FString> EnvelopeKeys = { TEXT("$schema"), TEXT("table"), TEXT("generated"), TEXT("sourceSha"), TEXT("rows") };
	for (const auto& Pair : Envelope->Values)
	{
		if (!EnvelopeKeys.Contains(FString(Pair.Key)))
		{
			OutError = FString::Printf(TEXT("%s: unknown envelope key '%s'"), *File, *Pair.Key);
			return false;
		}
	}
	FString Schema;
	Envelope->TryGetStringField(TEXT("$schema"), Schema);
	if (Schema != TEXT("deepfield-content/1"))
	{
		OutError = FString::Printf(TEXT("%s: $schema is '%s', this importer reads deepfield-content/1"), *File, *Schema);
		return false;
	}
	FString Declared;
	Envelope->TryGetStringField(TEXT("table"), Declared);
	if (Declared != Table)
	{
		OutError = FString::Printf(TEXT("%s: envelope says table '%s' but the file is %s.json"), *File, *Declared, *Table);
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Envelope->TryGetArrayField(TEXT("rows"), Rows))
	{
		OutError = FString::Printf(TEXT("%s: no 'rows' array"), *File);
		return false;
	}

	OutRows.Reset(Rows->Num());
	for (int32 i = 0; i < Rows->Num(); ++i)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (!(*Rows)[i].IsValid() || !(*Rows)[i]->TryGetObject(Row))
		{
			OutError = FString::Printf(TEXT("%s: rows[%d] is not an object"), *File, i);
			return false;
		}
		OutRows.Add(*Row);
	}
	return true;
}

bool FDFContentImporter::RowId(const TSharedPtr<FJsonObject>& Row, FString& OutId, const FString& Path, FString& OutError)
{
	if (!Row->TryGetStringField(TEXT("id"), OutId) || OutId.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s: every row needs a non-empty string 'id'"), *Path);
		return false;
	}
	return true;
}

bool FDFContentImporter::FillRow(const UScriptStruct* RowStruct, const TSharedRef<FJsonObject>& Row, void* OutRow, const FString& Path, FString& OutError)
{
	TSharedRef<FJsonObject> Fields = MakeShared<FJsonObject>();
	Fields->Values = Row->Values;
	Fields->RemoveField(TEXT("id"));

	const TSharedPtr<FJsonObject> Shaped = FDFContentJsonShaper::ShapeRow(RowStruct, Fields, Path, OutError);
	if (!Shaped.IsValid())
	{
		return false;
	}

	// Not strict: the JSON may omit optional fields, which keep the struct default InitializeStruct gave them.
	FText Reason;
	if (!FJsonObjectConverter::JsonObjectToUStruct(Shaped.ToSharedRef(), RowStruct, OutRow, /*CheckFlags*/ 0, /*SkipFlags*/ 0, /*bStrictMode*/ false, &Reason))
	{
		OutError = FString::Printf(TEXT("%s: %s"), *Path, *Reason.ToString());
		return false;
	}
	return true;
}

bool FDFContentImporter::ImportTable(const FString& Table, FDFContentImportSummary& OutSummary, FString& OutError) const
{
	OutSummary = FDFContentImportSummary();
	OutSummary.Table = Table;

	UScriptStruct* RowStruct = DFContentTables::RowStructFor(Table);
	if (!RowStruct)
	{
		OutError = FString::Printf(TEXT("%s: no row struct is mapped for this table (DFContentTables.cpp)"), *Table);
		return false;
	}

	TArray<TSharedPtr<FJsonObject>> Rows;
	if (!LoadTableJson(JsonDir, Table, Rows, OutError))
	{
		return false;
	}

	// Convert every row before touching the asset, so one bad row leaves the table exactly as it was.
	TArray<TPair<FName, TSharedPtr<FStructOnScope>>> Pending;
	Pending.Reserve(Rows.Num());
	TSet<FString> Seen;
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		FString Id;
		if (!RowId(Rows[i], Id, FString::Printf(TEXT("%s.json rows[%d]"), *Table, i), OutError))
		{
			return false;
		}
		if (Seen.Contains(Id))
		{
			OutError = FString::Printf(TEXT("%s.json rows[%d]: duplicate id '%s'"), *Table, i, *Id);
			return false;
		}
		Seen.Add(Id);

		TSharedPtr<FStructOnScope> Value = MakeShared<FStructOnScope>(RowStruct);
		if (!FillRow(RowStruct, Rows[i].ToSharedRef(), Value->GetStructMemory(), FString::Printf(TEXT("%s.json row '%s'"), *Table, *Id), OutError))
		{
			return false;
		}
		Pending.Emplace(FName(*Id), Value);
	}

	UDataTable* DataTable = FindOrCreateTable(Table, RowStruct, OutSummary.bCreated, OutError);
	if (!DataTable)
	{
		return false;
	}

	// Wholesale replacement: clear under the old struct (it owns the old rows' memory), then swap structs
	// if the table was created against an older definition, then add in JSON order (TMap keeps insertion order).
	DataTable->EmptyTable();
	if (DataTable->GetRowStruct() != RowStruct)
	{
		DataTable->RowStruct = RowStruct;
	}
	for (const auto& Row : Pending)
	{
		DataTable->AddRow(Row.Key, Row.Value->GetStructMemory(), RowStruct);
	}
	DataTable->MarkPackageDirty();

	OutSummary.Rows = Pending.Num();
	OutSummary.ObjectPath = DataTable->GetPathName();
	return SaveTable(DataTable, OutError);
}

UDataTable* FDFContentImporter::FindOrCreateTable(const FString& Table, UScriptStruct* RowStruct, bool& bOutCreated, FString& OutError) const
{
	const FString PackageName = DFContentTables::PackageNameFor(Table);
	const FString AssetName = DFContentTables::AssetNameFor(Table);
	bOutCreated = false;

	if (FPackageName::DoesPackageExist(PackageName))
	{
		UObject* Existing = LoadObject<UObject>(nullptr, *DFContentTables::ObjectPathFor(Table));
		UDataTable* DataTable = Cast<UDataTable>(Existing);
		if (!DataTable)
		{
			OutError = FString::Printf(TEXT("%s: %s exists but is not a DataTable"), *Table, *PackageName);
			return nullptr;
		}
		return DataTable;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("%s: cannot create package %s"), *Table, *PackageName);
		return nullptr;
	}
	Package->FullyLoad();
	UDataTable* DataTable = NewObject<UDataTable>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	DataTable->RowStruct = RowStruct;
	FAssetRegistryModule::AssetCreated(DataTable);
	bOutCreated = true;
	return DataTable;
}

bool FDFContentImporter::SaveTable(UDataTable* Table, FString& OutError)
{
	UPackage* Package = Table->GetOutermost();
	const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FileName), /*Tree*/ true);

	// The tables are git-lfs "lockable" (unreal/.gitattributes), so a checkout leaves them read-only as the
	// reminder to lock a shared asset before editing it by hand. Nothing edits these by hand: this commandlet
	// is their only writer (C3), so it clears the bit itself instead of failing after the JSON has already
	// been proven. Ownership of the resulting change is checked at the PR, not here (OWNERSHIP.md).
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*FileName) && PlatformFile.IsReadOnly(*FileName))
	{
		if (!PlatformFile.SetReadOnly(*FileName, false))
		{
			OutError = FString::Printf(TEXT("%s is read-only and could not be made writable"), *FileName);
			return false;
		}
		UE_LOG(LogDFContentPipeline, Display, TEXT("cleared the read-only flag on %s (git-lfs lockable; the importer is its only writer)"), *FileName);
	}

	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Table, *FileName, Args))
	{
		OutError = FString::Printf(TEXT("failed to save %s"), *FileName);
		return false;
	}
	return true;
}
