#include "DFWorldSubsystem.h"

#include "Engine/World.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"
#include "World/DFLaneGraphInfo.h"
#include "World/DFSocket.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFWorldSubsystem, Log, All);

UDFWorldSubsystem* UDFWorldSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDFWorldSubsystem>() : nullptr;
}

bool UDFWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UDFWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UDFWorldSubsystem::OnLevelChanged);
	LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UDFWorldSubsystem::OnLevelChanged);
}

void UDFWorldSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
	Super::Deinitialize();
}

void UDFWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Resolve();
}

void UDFWorldSubsystem::OnLevelChanged(ULevel* Level, UWorld* World)
{
	if (World == GetWorld())
	{
		Invalidate();
	}
}

void UDFWorldSubsystem::Invalidate()
{
	bResolved = false;
	LaneGraph = nullptr;
	Info = nullptr;
	SocketsById.Reset();
}

void UDFWorldSubsystem::Resolve()
{
	bResolved = true;
	LaneGraph = nullptr;
	Info = nullptr;
	SocketsById.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ADFLaneGraphInfo> It(World); It; ++It)
	{
		if (It->LaneGraph)
		{
			Info = *It;
			LaneGraph = It->LaneGraph;
			break;
		}
	}

	if (!LaneGraph)
	{
		// Convention: /Game/DF/Maps/<Map>/L_<Map> -> /Game/DF/Data/Defs/LaneGraphs/DA_LaneGraph_<Map>.
		const FString MapName = UWorld::RemovePIEPrefix(FPackageName::GetShortName(World->GetOutermost()->GetName()));
		if (MapName.StartsWith(TEXT("L_")))
		{
			const FString Map = MapName.RightChop(2);
			const FString Package = FString::Printf(TEXT("/Game/DF/Data/Defs/LaneGraphs/DA_LaneGraph_%s"), *Map);
			if (FPackageName::DoesPackageExist(Package))
			{
				LaneGraph = Cast<UDFLaneGraphAsset>(FSoftObjectPath(Package + TEXT(".DA_LaneGraph_") + Map).TryLoad());
			}
		}
	}

	for (TActorIterator<ADFSocket> It(World); It; ++It)
	{
		if (!It->SocketId.IsNone())
		{
			SocketsById.Add(It->SocketId, *It);
		}
	}

	if (LaneGraph)
	{
		UE_LOG(LogDFWorldSubsystem, Log, TEXT("%s: lane graph %s (%d nodes, %d edges, %d sockets placed)"),
			*World->GetName(), *LaneGraph->GetName(), LaneGraph->Nodes.Num(), LaneGraph->Edges.Num(), SocketsById.Num());
	}
}

UDFLaneGraphAsset* UDFWorldSubsystem::GetLaneGraph()
{
	if (!bResolved)
	{
		Resolve();
	}
	if (!LaneGraph && !bReportedMissing && GetWorld() && GetWorld()->IsGameWorld())
	{
		bReportedMissing = true;
		UE_LOG(LogDFWorldSubsystem, Error, TEXT("%s has no lane graph: run -run=DFLevelImport -map=<map> (no ADFLaneGraphInfo and no DA_LaneGraph_<Map>)"), *GetWorld()->GetName());
	}
	return LaneGraph;
}

ADFSocket* UDFWorldSubsystem::FindSocket(FName SocketId)
{
	if (!bResolved)
	{
		Resolve();
	}
	const TWeakObjectPtr<ADFSocket>* Found = SocketsById.Find(SocketId);
	return Found ? Found->Get() : nullptr;
}

TArray<ADFSocket*> UDFWorldSubsystem::GetSockets()
{
	if (!bResolved)
	{
		Resolve();
	}
	TArray<ADFSocket*> Out;
	for (const auto& Pair : SocketsById)
	{
		if (ADFSocket* Socket = Pair.Value.Get())
		{
			Out.Add(Socket);
		}
	}
	return Out;
}
