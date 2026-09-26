#include "DFContentHash.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "UObject/UnrealType.h"

const TCHAR* UDFContentHash::TablesRoot()
{
	return TEXT("/Game/DF/Data/Tables");
}

FString UDFContentHash::ComputeForProject(bool& bOutComplete)
{
	EDFContentHashSource Source = EDFContentHashSource::None;
	const FString Hash = ComputeForProjectSource(Source);
	bOutComplete = Source != EDFContentHashSource::None;
	return Hash;
}

FString UDFContentHash::ComputeForProjectSource(EDFContentHashSource& OutSource)
{
	TArray<const UDataTable*> Tables;
	if (IAssetRegistry* Registry = IAssetRegistry::Get())
	{
		// A cooked build has its registry ready; the editor may still be scanning, so ask for the folder explicitly.
		Registry->ScanPathsSynchronous({ TablesRoot() }, /*bForceRescan*/ false);
		TArray<FAssetData> Assets;
		Registry->GetAssetsByPath(FName(TablesRoot()), Assets, /*bRecursive*/ true);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath == UDataTable::StaticClass()->GetClassPathName())
			{
				if (const UDataTable* Table = Cast<UDataTable>(Asset.GetAsset()))
				{
					Tables.Add(Table);
				}
			}
		}
	}
	if (Tables.Num() > 0)
	{
		OutSource = EDFContentHashSource::Tables;
		return HashTables(MoveTemp(Tables));
	}
	// No imported tables in this checkout (the WS-01 importer has not run): the JSON the importer
	// reads is the same source of truth (ADR-0005), so two checkouts of the same content still agree.
	bool bFound = false;
	const FString JsonHash = HashJsonSource(bFound);
	OutSource = bFound ? EDFContentHashSource::JsonSource : EDFContentHashSource::None;
	return bFound ? JsonHash : Sha1Hex(FString());
}

FString UDFContentHash::JsonSourceDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/json")));
}

FString UDFContentHash::HashJsonSource(bool& bOutFound)
{
	const FString Dir = JsonSourceDir();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), /*Files*/ true, /*Directories*/ false);
	Files.Sort();
	FString Text;
	for (const FString& File : Files)
	{
		FString Body;
		if (!FFileHelper::LoadFileToString(Body, *FPaths::Combine(Dir, File)))
		{
			continue;
		}
		Body.ReplaceInline(TEXT("\r"), TEXT(""));
		Text += FString::Printf(TEXT("file %s\n"), *File);
		Text += Body;
		Text += TEXT("\n");
	}
	bOutFound = !Text.IsEmpty();
	return Sha1Hex(Text);
}

FString UDFContentHash::HashTables(TArray<const UDataTable*> Tables)
{
	Tables.RemoveAll([](const UDataTable* T) { return T == nullptr; });
	Tables.Sort([](const UDataTable& A, const UDataTable& B) { return A.GetPathName() < B.GetPathName(); });
	FString Text;
	for (const UDataTable* Table : Tables)
	{
		AppendTableText(Table, Text);
	}
	return Sha1Hex(Text);
}

void UDFContentHash::AppendTableText(const UDataTable* Table, FString& Out)
{
	if (!Table)
	{
		return;
	}
	const UScriptStruct* RowStruct = Table->GetRowStruct();
	Out += FString::Printf(TEXT("table %s %s\n"), *Table->GetName(), RowStruct ? *RowStruct->GetName() : TEXT("?"));
	if (!RowStruct)
	{
		return;
	}
	TArray<FName> RowNames = Table->GetRowNames();
	RowNames.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	const TMap<FName, uint8*>& Rows = Table->GetRowMap();
	for (const FName& RowName : RowNames)
	{
		if (const uint8* const* RowData = Rows.Find(RowName); RowData && *RowData)
		{
			FString RowText;
			// Text export, not the binary archive: it is stable across engine serialization changes
			// and readable when two hashes need to be diffed by hand.
			RowStruct->ExportText(RowText, *RowData, nullptr, nullptr, PPF_None, nullptr);
			Out += RowName.ToString();
			Out += TEXT("=");
			Out += RowText;
			Out += TEXT("\n");
		}
	}
}

FString UDFContentHash::Sha1Hex(const FString& Text)
{
	const FTCHARToUTF8 Utf8(*Text);
	uint8 Digest[FSHA1::DigestSize];
	FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Digest);
	return BytesToHex(Digest, FSHA1::DigestSize).ToLower();
}
