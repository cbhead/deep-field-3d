#include "DFContentImportCommandlet.h"

#include "DFContentImporter.h"
#include "DFContentPipelineModule.h"
#include "DFContentTables.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

UDFContentImportCommandlet::UDFContentImportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UDFContentImportCommandlet::Main(const FString& Params)
{
	FString JsonDir = DFContentTables::DefaultJsonDir();
	FParse::Value(*Params, TEXT("json="), JsonDir);
	JsonDir = FPaths::ConvertRelativePathToFull(JsonDir);

	TArray<FString> Tables;
	FString TablesArg;
	if (FParse::Value(*Params, TEXT("tables="), TablesArg))
	{
		TablesArg.ParseIntoArray(Tables, TEXT(","), /*CullEmpty*/ true);
		for (FString& Table : Tables)
		{
			Table.TrimStartAndEndInline();
		}
	}
	else
	{
		Tables = DFContentTables::TablesIn(JsonDir);
	}
	if (Tables.Num() == 0)
	{
		UE_LOG(LogDFContentPipeline, Error, TEXT("no tables to import (looked in %s)"), *JsonDir);
		return 1;
	}

	UE_LOG(LogDFContentPipeline, Display, TEXT("importing %d table(s) from %s into %s"), Tables.Num(), *JsonDir, DFContentTables::Root());
	const FDFContentImporter Importer(JsonDir);
	int32 Failed = 0;
	int32 RowsTotal = 0;
	for (const FString& Table : Tables)
	{
		FDFContentImportSummary Summary;
		FString Error;
		if (Importer.ImportTable(Table, Summary, Error))
		{
			RowsTotal += Summary.Rows;
			UE_LOG(LogDFContentPipeline, Display, TEXT("%-18s %3d rows -> %s (%s)"), *Table, Summary.Rows, *Summary.ObjectPath, Summary.bCreated ? TEXT("created") : TEXT("updated"));
		}
		else
		{
			// Keep going so one run reports every broken table, then fail.
			++Failed;
			UE_LOG(LogDFContentPipeline, Error, TEXT("%s: FAILED: %s"), *Table, *Error);
		}
	}
	UE_LOG(LogDFContentPipeline, Display, TEXT("done: %d table(s) imported (%d rows), %d failed"), Tables.Num() - Failed, RowsTotal, Failed);
	return Failed == 0 ? 0 : 1;
}
