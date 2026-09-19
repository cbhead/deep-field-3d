#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FAutomationTestBase;
class UDataTable;
class UDFContentSubsystem;

// Shared by the DF.Content.* tests: they all start from the committed tables and the text source of truth.
namespace DFContentTest
{
	/**
	 * A UDFContentSubsystem a headless test can call. It is a GameInstance subsystem (ClassWithin), so it
	 * may only be created inside a UGameInstance; a test session has none, so it gets a bare transient one.
	 * Nothing calls Init() on it, so no other subsystem comes to life.
	 */
	UDFContentSubsystem* MakeSubsystem();

	/** Loads a committed table asset; adds an error and returns null if the importer has not written it. */
	UDataTable* LoadTable(FAutomationTestBase& Test, const FString& Table);

	/** Parses any JSON file into an object; adds an error and returns null on failure. */
	TSharedPtr<FJsonObject> LoadJsonObject(FAutomationTestBase& Test, const FString& File);

	/** Parses unreal/content/content-ids.json into table -> ids (in file order). */
	bool LoadContentIds(FAutomationTestBase& Test, TMap<FString, TArray<FString>>& OutIds);
}
