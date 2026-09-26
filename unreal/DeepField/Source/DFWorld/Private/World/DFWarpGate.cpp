#include "World/DFWarpGate.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFWarpGate, Log, All);

FName ADFWarpGate::StableIdFor(FName InNodeId, FName InEdgeId)
{
	return FName(*(InNodeId.ToString() + TEXT("@") + InEdgeId.ToString()));
}

bool ADFWarpGate::SplitStableId(FName StableId, FName& OutNodeId, FName& OutEdgeId)
{
	FString Node;
	FString Edge;
	if (!StableId.ToString().Split(TEXT("@"), &Node, &Edge, ESearchCase::CaseSensitive, ESearchDir::FromStart) || Node.IsEmpty() || Edge.IsEmpty())
	{
		return false;
	}
	OutNodeId = FName(*Node);
	OutEdgeId = FName(*Edge);
	return true;
}

void ADFWarpGate::AssignStableId(FName NewId)
{
	if (!SplitStableId(NewId, NodeId, EdgeId))
	{
		UE_LOG(LogDFWarpGate, Error, TEXT("%s: warp gate id '%s' is not \"<node>@<edge>\"; NodeId and EdgeId left as they were"), *GetName(), *NewId.ToString());
	}
}
