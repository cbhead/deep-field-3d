#include "Backends/DFSessionBackendListenP2P.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFOnlineBackend, Log, All);

bool FDFSessionBackendListenP2P::StartHost(UWorld* World, FName MapId, const FString& Options)
{
	if (!World || MapId.IsNone())
	{
		return false;
	}
	const FString Level = MapIdToLevelName(MapId);
	UE_LOG(LogDFOnlineBackend, Log, TEXT("listen host: %s?listen%s"), *Level, *Options);
	UGameplayStatics::OpenLevel(World, FName(*Level), /*bAbsolute*/ true, TEXT("listen") + Options);
	return true;
}

bool FDFSessionBackendListenP2P::Join(UWorld* World, const FString& ConnectString, const FString& Options)
{
	if (!World || ConnectString.IsEmpty() || !GEngine)
	{
		return false;
	}
	const FString URL = ConnectString + Options;
	UE_LOG(LogDFOnlineBackend, Log, TEXT("client travel: %s"), *URL);
	GEngine->SetClientTravel(World, *URL, TRAVEL_Absolute);
	return true;
}

void FDFSessionBackendListenP2P::Leave(UWorld* World)
{
	// Only a world that is actually networked travels back; a front-end or test world stays put.
	if (!World || !World->GetNetDriver())
	{
		return;
	}
	const FString FrontEnd = FrontEndMap();
	if (FrontEnd.IsEmpty())
	{
		return;
	}
	UE_LOG(LogDFOnlineBackend, Log, TEXT("leaving session -> %s"), *FrontEnd);
	UGameplayStatics::OpenLevel(World, FName(*FrontEnd), /*bAbsolute*/ true);
}

FName FDFSessionBackendListenP2P::GetNetDriverClassName() const
{
	// Read what the engine will instantiate rather than assuming: the EOS switch is one ini line.
	TArray<FString> Definitions;
	GConfig->GetArray(TEXT("/Script/Engine.GameEngine"), TEXT("NetDriverDefinitions"), Definitions, GEngineIni);
	for (const FString& Definition : Definitions)
	{
		FString DefName;
		FParse::Value(*Definition, TEXT("DefName="), DefName);
		if (DefName.TrimQuotes() == FName(NAME_GameNetDriver).ToString())
		{
			FString ClassName;
			FParse::Value(*Definition, TEXT("DriverClassName="), ClassName);
			return FName(*ClassName.TrimQuotes());
		}
	}
	return TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
}

FString FDFSessionBackendListenP2P::MapIdToLevelName(FName MapId)
{
	FString Id = MapId.ToString();
	if (Id.StartsWith(TEXT("/")) || Id.StartsWith(TEXT("L_")))
	{
		return Id;
	}
	if (!Id.IsEmpty())
	{
		Id[0] = FChar::ToUpper(Id[0]);
	}
	return TEXT("L_") + Id;
}

FString FDFSessionBackendListenP2P::FrontEndMap()
{
	return UGameMapsSettings::GetGameDefaultMap();
}
