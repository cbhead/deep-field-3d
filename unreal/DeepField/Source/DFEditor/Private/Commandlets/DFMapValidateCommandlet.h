#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DFMapValidateCommandlet.generated.h"

class UDFLaneGraphAsset;

// -run=DFMapValidate -map=<id>[,<id>] | -all        (or -run=DFEditor.DFMapValidate)
//
// (DFEditor loads at the Default phase, so the bare name resolves; the module-qualified spelling still works.)
//
// DF.Map.Validate, the seed (CONTRACTS/map-authoring-3d.md §2): the rules that need no terrain.
//   sealing (11)      RFC-0001: sealing combinations are REPORTED (the runtime refuses the last
//                     closure); a single-edge seal or a monotonicity break FAILS; <= 8 closable edges
//   spawnApron (12)   no socket within 8 m of a spawn
//   socketOffset (4)  every tower pad >= 3.5 m off any lane centreline (the distance half of rule 4)
//   corridor (2)      reported as "no navmesh" — never passed until the corridor walk exists
//   coverage (5)      every 4 m of every walk edge sees >= 3 tower pads within range (16 m ground /
//                     15 m air) by a real DF_Sight trace, 1.6 m over the pad to 1.6 m over the lane;
//                     dead-ground report to unreal/content/levels/reports/<map>.coverage.json
//   routeIds          every routeId in unreal/content/json/waves_<map>.json is an itinerary id in the
//                     lane graph (map-authoring-3d.md "Route ids are referenced by the wave tables");
//                     the failure names the row, the route and both files; SKIP when the map has no table
// Ratchet: unreal/map-validation-baseline.tsv lists (map, rule) pairs known to fail; those are
// warnings, anything else failing is an error and the commandlet exits 1.
// Writes only the report (text, not LFS-lockable); it loads L_<Map> and saves no package, so it
// needs no `git lfs lock` — see DFLevelImportCommandlet.h for the etiquette when it ever does.
UCLASS()
class UDFMapValidateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDFMapValidateCommandlet();

	virtual int32 Main(const FString& Params) override;

private:
	enum class EStatus { Pass, Fail, Warn, Skip };

	struct FResult
	{
		FString Rule;
		EStatus Status = EStatus::Pass;
		TArray<FString> Notes;
	};

	struct FSegment
	{
		FName Edge;
		float T = 0.f;
		FVector Position = FVector::ZeroVector;
		bool bAir = false;
		bool bApron = false;
		TArray<FName> Covering;
	};

	bool ValidateMap(const FString& MapId, TArray<FResult>& OutResults);

	FResult CheckSealing(const UDFLaneGraphAsset& Graph);
	FResult CheckSpawnApron(const UDFLaneGraphAsset& Graph);
	FResult CheckSocketOffset(const UDFLaneGraphAsset& Graph);
	FResult CheckCorridor(const UDFLaneGraphAsset& Graph);
	FResult CheckCoverage(UWorld* World, const UDFLaneGraphAsset& Graph, const FString& MapId);
	FResult CheckRouteIds(const UDFLaneGraphAsset& Graph, const FString& MapId);

	static TSet<FString> LoadBaseline();
	static FString WaveTablePath(const FString& MapId);
	static FString BaselinePath();
	static FString ReportPath(const FString& MapId);
	static float DistanceToPolylineCm(const FVector& P, const TArray<FVector>& Points);
	static FVector PointAlongPolyline(const TArray<FVector>& Points, double DistanceCm);
};
