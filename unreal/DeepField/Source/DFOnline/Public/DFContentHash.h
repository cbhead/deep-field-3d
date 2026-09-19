#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DFContentHash.generated.h"

class UDataTable;

/** Where a project content hash came from. */
UENUM(BlueprintType)
enum class EDFContentHashSource : uint8
{
	/** Neither imported tables nor the JSON source were found: the hash is of nothing. */
	None,
	/** The imported DataTables under /Game/DF/Data/Tables (the shipped answer). */
	Tables,
	/** The text source of truth, unreal/content/json (a checkout where the importer has not run). */
	JsonSource,
};

// The content hash of the version + content-hash handshake (B§5.9, C14). A host and a client
// with different numbers -- a hand-edited towers.json, an importer that ran on one side only --
// would desync silently; the hash makes the mismatch a named refusal (DF.Message.JoinRejected
// contentMismatch) instead. It is a SHA-1 over the text export of every row of every DataTable
// under /Game/DF/Data/Tables (the importer's output, ADR-0005), in path order, so two builds that
// imported the same JSON agree regardless of asset GUIDs or file timestamps.
UCLASS()
class DFONLINE_API UDFContentHash : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The tables the importer writes (UDFContentSubsystem loads from the same folder). */
	static const TCHAR* TablesRoot();

	/** Hash of every DataTable under TablesRoot; when the folder holds none (importer not run in this
	 *  checkout) the JSON source the importer reads is hashed instead. bOutComplete is false only when
	 *  neither exists. */
	UFUNCTION(BlueprintCallable, Category = "DF|Online")
	static FString ComputeForProject(bool& bOutComplete);

	/** As above, reporting which source the hash came from. */
	static FString ComputeForProjectSource(EDFContentHashSource& OutSource);

	/** The text source of truth: <repo>/unreal/content/json (ADR-0005). Absent in a packaged build. */
	static FString JsonSourceDir();

	/** SHA-1 over every *.json under JsonSourceDir, sorted by file name, "\r" stripped so checkouts agree
	 *  across line-ending settings. bOutFound is false when the folder holds no JSON. */
	static FString HashJsonSource(bool& bOutFound);

	/** Hash of the given tables, sorted by path name first so callers need not care about order. */
	static FString HashTables(TArray<const UDataTable*> Tables);

	/** One table's rows as deterministic text: "table <name> <struct>\n<row>=<export>\n..." with rows sorted by name. */
	static void AppendTableText(const UDataTable* Table, FString& Out);

	/** Lower-case hex SHA-1 of the UTF-8 bytes of Text. */
	static FString Sha1Hex(const FString& Text);
};
