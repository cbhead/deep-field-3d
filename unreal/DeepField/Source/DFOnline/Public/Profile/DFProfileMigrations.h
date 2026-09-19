#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DFProfileMigrations.generated.h"

class UDFProfileSave;

// C13: every profile layout change is a numbered step here. A profile from an older build is
// walked forward one version at a time; one from a newer build is left alone (the player
// downgraded) and reported, so the newer fields are never silently discarded by a save.
UCLASS()
class DFONLINE_API UDFProfileMigrations : public UObject
{
	GENERATED_BODY()

public:
	/** Bring Profile to UDFProfileSave::CurrentVersion. Returns false if it is from a newer build. */
	static bool Migrate(UDFProfileSave* Profile);

	/** Which version a freshly created profile carries. */
	static int32 CurrentVersion();

private:
	/** v0 (pre-versioned / uninitialised) -> v1: nothing to move, the defaults are the v1 layout. */
	static void MigrateV0ToV1(UDFProfileSave& Profile);
};
