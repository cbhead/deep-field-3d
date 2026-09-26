#pragma once

#include "CoreMinimal.h"
#include "LaneGraph/DFLaneGraphTypes.h"

// unreal/content/levels/<map>.level.json (docs/MAP-AUTHORING.md §2, extended by
// CONTRACTS/map-authoring-3d.md §1) and its legacy export (levels/legacy/<map>.level.json,
// "$schema": "deepfield-level/legacy", straight out of Maps.cs). Both spellings of every record are
// accepted — the legacy files are objects ({"id","tag","pos"}), the authored format is the terse
// array form (["g8", [-38,0,-1], "ground"]) — so one loader serves the importer and the tests.
//
// Everything here is already in the Unreal frame (cm, Z-up, +X forward); the conversion is the
// loader's, applied once, so nothing downstream can forget it.

struct DFWORLD_API FDFLevelRoute
{
	FName Id;
	EDFEnemyLayer Layer = EDFEnemyLayer::Ground;
	TArray<FVector> Waypoints;      // cm
	TArray<int32> TeleportLegs;     // leg i runs Waypoints[i] -> Waypoints[i+1]
	bool bElevated = false;         // authored: keep the heights, do not project to terrain

	int32 LegCount() const { return Waypoints.Num() - 1; }
	bool IsTeleportLeg(int32 Leg) const { return TeleportLegs.Contains(Leg); }
};

struct DFWORLD_API FDFLevelSocket
{
	FName Id;
	EDFSocketTag Tag = EDFSocketTag::Ground;
	FVector Position = FVector::ZeroVector;
	bool bPad = false;              // the importer cuts a level pad (terrain maps)
};

struct DFWORLD_API FDFLevelStation
{
	FName Id;
	FVector Position = FVector::ZeroVector;
};

struct DFWORLD_API FDFLevelVehicle
{
	FName Id;
	FName DefId;
	FVector Position = FVector::ZeroVector;
	float Yaw = 0.f;                // Unreal degrees (converted)
};

struct DFWORLD_API FDFLevelNodeName
{
	FName Id;
	FVector At = FVector::ZeroVector;
};

struct DFWORLD_API FDFLevelLaneGate
{
	FName EdgeId;
	FName SocketId;
};

struct DFWORLD_API FDFLevelOperatedGate
{
	FName Id;
	FName EdgeId;
	FVector At = FVector::ZeroVector;
	FString Label;
};

struct DFWORLD_API FDFLevelFile
{
	FString Schema;
	FName Id;
	FVector2D FieldMeters = FVector2D(110.f, 80.f);
	int32 TotalWaves = 10;
	FVector HeroSpawn = FVector::ZeroVector;
	FVector Armory = FVector::ZeroVector;
	TArray<FDFLevelRoute> Routes;
	TArray<FDFLevelSocket> Sockets;
	TArray<FDFLevelStation> Stations;
	TMap<int32, FName> ConditionSchedule;
	TArray<FDFLevelVehicle> Vehicles;
	TArray<FDFLevelNodeName> LaneNodeNames;
	TArray<FDFLevelLaneGate> LaneGates;
	TArray<FDFLevelOperatedGate> OperatedGates;
	FString SourcePath;             // where it was read from
	bool bLegacy = false;           // read from levels/legacy/

	/** unreal/content/levels, absolute (FPaths::ProjectDir()/../content/levels). */
	static FString ContentLevelsDir();

	/** The file for a map: levels/<map>.level.json, or levels/legacy/<map>.level.json when asked
	 *  for (bPreferLegacy) or when the primary file does not exist. Empty if neither exists. */
	static FString ResolvePath(const FString& MapId, bool bPreferLegacy, bool* bOutLegacy = nullptr);

	/** Every map id with a level file, primary or legacy, sorted. */
	static TArray<FString> AllMapIds();

	static bool Load(const FString& Path, FDFLevelFile& Out, FString& OutError);
};
