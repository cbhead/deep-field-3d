#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UDataTable;
class UScriptStruct;

struct FDFContentImportSummary
{
	FString Table;
	FString ObjectPath;
	int32 Rows = 0;
	bool bCreated = false;
};

// JSON table -> DataTable asset (ADR-0005). Rows are replaced wholesale in JSON order; an unknown key fails
// the whole table before the asset is touched (FDFContentJsonShaper); the asset is saved under
// Content/DF/Data/Tables. The commandlet and the tests share this class so both read the JSON the same way.
class DFCONTENTPIPELINE_API FDFContentImporter
{
public:
	explicit FDFContentImporter(const FString& InJsonDir);

	/** Imports one table. On failure OutError names the table, row and key, and nothing is saved. */
	bool ImportTable(const FString& Table, FDFContentImportSummary& OutSummary, FString& OutError) const;

	/** Reads <JsonDir>/<table>.json, checks the C3 envelope, returns the rows in file order. */
	static bool LoadTableJson(const FString& JsonDir, const FString& Table, TArray<TSharedPtr<FJsonObject>>& OutRows, FString& OutError);

	/** The "id" of a row: a non-empty string, the DataTable row name. */
	static bool RowId(const TSharedPtr<FJsonObject>& Row, FString& OutId, const FString& Path, FString& OutError);

	/** Fills an initialised row struct from a JSON row; "id" is skipped (it is the row name, not a field). */
	static bool FillRow(const UScriptStruct* RowStruct, const TSharedRef<FJsonObject>& Row, void* OutRow, const FString& Path, FString& OutError);

private:
	UDataTable* FindOrCreateTable(const FString& Table, UScriptStruct* RowStruct, bool& bOutCreated, FString& OutError) const;
	static bool SaveTable(UDataTable* Table, FString& OutError);

	FString JsonDir;
};
