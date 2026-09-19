#include "Content/DFContentSubsystem.h"

#include "Content/DFContentDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFContent, Log, All);

namespace
{
	// Table name -> asset name. Keep in sync with the importer (WS-01) and content-rows.md.
	const TCHAR* GTableNames[] = {
		TEXT("towers"), TEXT("traps"), TEXT("enemies"), TEXT("statuses"), TEXT("reactions"), TEXT("factions"),
		TEXT("weapons"), TEXT("melee"), TEXT("meleeAttachments"), TEXT("attachments"), TEXT("ammo"),
		TEXT("conditions"), TEXT("vehicles"), TEXT("maps"), TEXT("balance"),
	};

	FString TableAssetPath(const FString& Table)
	{
		FString Name = Table;
		Name[0] = FChar::ToUpper(Name[0]);
		return FString::Printf(TEXT("/Game/DF/Data/Tables/DT_%s.DT_%s"), *Name, *Name);
	}
}

UDFContentSubsystem* UDFContentSubsystem::Get(const UObject* WorldContext)
{
	if (const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UDFContentSubsystem>();
		}
	}
	return nullptr;
}

void UDFContentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadTables();
}

bool UDFContentSubsystem::LoadTables()
{
	Tables.Reset();
	bool bAllPresent = true;
	for (const TCHAR* Table : GTableNames)
	{
		const FString Path = TableAssetPath(Table);
		// Existence first: TryLoad on a missing package logs an engine warning per table, and an
		// un-imported project is a normal state (the importer has not run yet), not a fault.
		const FString PackageName = FPackageName::ObjectPathToPackageName(Path);
		UDataTable* DT = FPackageName::DoesPackageExist(PackageName) ? Cast<UDataTable>(FSoftObjectPath(Path).TryLoad()) : nullptr;
		if (DT)
		{
			Tables.Add(FName(Table), DT);
		}
		else
		{
			// Until the importer has run once this is expected; log at Verbose so an empty
			// project still boots, and let DF.Content.Bindings be the test that fails.
			UE_LOG(LogDFContent, Verbose, TEXT("content table missing: %s"), *Path);
			bAllPresent = false;
		}
	}
	// Wave tables are per map: DT_Waves_<map>. Discover through the maps table.
	if (const TObjectPtr<UDataTable>* MapsDT = Tables.Find(TEXT("maps")); MapsDT && *MapsDT)
	{
		for (const FName& MapId : (*MapsDT)->GetRowNames())
		{
			const FString Path = FString::Printf(TEXT("/Game/DF/Data/Tables/DT_Waves_%s.DT_Waves_%s"), *MapId.ToString(), *MapId.ToString());
			if (UDataTable* DT = Cast<UDataTable>(FSoftObjectPath(Path).TryLoad()))
			{
				Tables.Add(FName(*FString::Printf(TEXT("waves_%s"), *MapId.ToString())), DT);
			}
		}
	}
	bReady = bAllPresent;
	UE_LOG(LogDFContent, Log, TEXT("content tables loaded: %d (%s)"), Tables.Num(), bReady ? TEXT("complete") : TEXT("incomplete"));
	return bReady;
}

TArray<const FDFWaveGroupRow*> UDFContentSubsystem::Waves(FName MapId) const
{
	TArray<const FDFWaveGroupRow*> Out;
	if (const TObjectPtr<UDataTable>* DT = Tables.Find(FName(*FString::Printf(TEXT("waves_%s"), *MapId.ToString()))); DT && *DT)
	{
		(*DT)->ForeachRow<FDFWaveGroupRow>(TEXT("DFContent"), [&Out](const FName&, const FDFWaveGroupRow& Row) { Out.Add(&Row); });
	}
	else
	{
		ReportMissing(TEXT("waves"), MapId);
	}
	return Out;
}

float UDFContentSubsystem::Balance(FName Dial, float Default) const
{
	if (const FDFBalanceRow* Row = Find<FDFBalanceRow>(TEXT("balance"), TEXT("default")))
	{
		if (const float* V = Row->Dials.Find(Dial))
		{
			return *V;
		}
		ReportMissing(TEXT("balance"), Dial);
	}
	return Default;
}

TArray<FName> UDFContentSubsystem::Ids(FName Table) const
{
	if (const TObjectPtr<UDataTable>* DT = Tables.Find(Table); DT && *DT)
	{
		return (*DT)->GetRowNames();
	}
	return {};
}

UDFContentDefinition* UDFContentSubsystem::Definition(const FPrimaryAssetType& Type, FName Id) const
{
	if (UAssetManager* AM = UAssetManager::GetIfInitialized())
	{
		const FPrimaryAssetId AssetId(Type, Id);
		if (UObject* Obj = AM->GetPrimaryAssetObject(AssetId))
		{
			return Cast<UDFContentDefinition>(Obj);
		}
		const FSoftObjectPath Path = AM->GetPrimaryAssetPath(AssetId);
		if (Path.IsValid())
		{
			return Cast<UDFContentDefinition>(Path.TryLoad());
		}
	}
	ReportMissing(FName(*FString::Printf(TEXT("DA:%s"), *Type.ToString())), Id);
	return nullptr;
}

void UDFContentSubsystem::ReportMissing(FName Table, FName Id) const
{
	const FString Key = Table.ToString() + TEXT("/") + Id.ToString();
	if (!Reported.Contains(Key))
	{
		Reported.Add(Key);
		UE_LOG(LogDFContent, Error, TEXT("missing content: table '%s' has no row '%s'"), *Table.ToString(), *Id.ToString());
	}
}
