#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DFContentImportCommandlet.generated.h"

// UnrealEditor-Cmd <project> -run=DFContentImport [-tables=towers,enemies] [-json=<dir>]
// Writes /Game/DF/Data/Tables/DT_<Table> for every unreal/content/json/<table>.json (or the listed ones).
// Exit code 1 if any table failed; the log names the table, row and key. Run it through
// unreal/Build/editor-lock.sh (one editor process per machine) — see unreal/content/README.md.
UCLASS()
class DFCONTENTPIPELINE_API UDFContentImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDFContentImportCommandlet();

	virtual int32 Main(const FString& Params) override;
};
