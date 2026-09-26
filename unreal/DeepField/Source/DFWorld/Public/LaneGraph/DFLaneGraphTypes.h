#pragma once

#include "CoreMinimal.h"
#include "Content/DFContentRows.h"
#include "DFLaneGraphTypes.generated.h"

// C6 — the lane graph's value types (CONTRACTS/lanegraph.md). Positions are Unreal: centimetres,
// Z-up, +X forward. The sim's frame (metres, Y-up, -Z forward) is converted once, at import, by
// FDFSimFrame; nothing past the importer sees a metre or a Y-up vector.
//
// Ids are permanent (the contract's one hard rule): a node, edge or socket id appears in save data,
// in wave tables (RouteId -> itinerary) and in fixtures, so it is never renamed, only added.

/** Where a route begins, where routes meet or split, where a route ends. */
UENUM(BlueprintType)
enum class EDFLaneNodeKind : uint8
{
	Spawn,      // a spawn gate, or a warp arrival pad (an entrance in every sense the apron rule means)
	Junction,   // a fork, a join, or a warp departure
	Core,       // every ground route ends here
};

UENUM(BlueprintType)
enum class EDFLaneEdgeKind : uint8
{
	Walk,       // walked at the enemy's speed along the waypoints
	Warp,       // crossed in a tick; length zero for every purpose
};

UENUM(BlueprintType)
enum class EDFLaneEdgeState : uint8
{
	Open,
	Closed,
};

/** What can close an edge, if anything. */
UENUM(BlueprintType)
enum class EDFLaneClosableBy : uint8
{
	None,
	Socket,     // a barricade built on Gates[].SocketId
	Mutable,    // a mutable (floodgate, wall, ...) on Mutables[].Id
	Lever,      // an operated gate on OperatedGates[].Id
};

UENUM(BlueprintType)
enum class EDFMutableKind : uint8
{
	Floodgate, Crusher, Wall, Cache, Nest, Barrel, Container,
};

UENUM(BlueprintType)
enum class EDFTraversalKind : uint8
{
	Ladder, Zipline, Launcher, Teleporter, Elevator, Nest, Armory, Mantle,
};

USTRUCT(BlueprintType)
struct DFWORLD_API FDFLaneNode
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;   // cm, projected to terrain
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFLaneNodeKind Kind = EDFLaneNodeKind::Junction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFEnemyLayer Layer = EDFEnemyLayer::Ground;
};

/** One authored span between two nodes. Waypoints include both endpoints, so an edge carries its
 *  whole geometry and a walker needs nothing but the edge to know where it is. */
USTRUCT(BlueprintType)
struct DFWORLD_API FDFLaneEdge
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;                       // "<from>-<to>" (+ "@air" for the air layer)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName From;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName To;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFEnemyLayer Layer = EDFEnemyLayer::Ground;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFLaneEdgeKind Kind = EDFLaneEdgeKind::Walk;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FVector> Waypoints;       // cm; spline control points
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LengthMeters = 0.f;        // walked length; 0 for a warp
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxGradePercent = 0.f;     // steepest 1 m sample (validator rule 1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CostFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFLaneEdgeState State = EDFLaneEdgeState::Open;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFLaneClosableBy ClosableBy = EDFLaneClosableBy::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ClosableById;              // the socket / mutable / lever id
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AglMeters = 0.f;           // air only: authored height above ground
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CorridorWidthMeters = 3.4f;

	bool IsWarp() const { return Kind == EDFLaneEdgeKind::Warp; }
};

/** The authored route as the ordered list of nodes it passes through (WaveGroup.RouteId names one). */
USTRUCT(BlueprintType)
struct DFWORLD_API FDFLaneItinerary
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFEnemyLayer Layer = EDFEnemyLayer::Ground;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Via;
};

/** A barricade socket that closes an edge. */
USTRUCT(BlueprintType)
struct DFWORLD_API FDFLaneGateDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EdgeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SocketId;
};

/** A lever and the lane it shuts (free, instant, reversible; stops walkers, not sieges). */
USTRUCT(BlueprintType)
struct DFWORLD_API FDFOperatedGateDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EdgeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector At = FVector::ZeroVector;   // cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Label;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownSeconds = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ReachMeters = 9.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BodyCheckMeters = 4.f;
};

USTRUCT(BlueprintType)
struct DFWORLD_API FDFMutableDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFMutableKind Kind = EDFMutableKind::Wall;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EdgeId;                    // optional
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector At = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> Params;
};

USTRUCT(BlueprintType)
struct DFWORLD_API FDFBossRouteDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Via;              // edge ids: <=15 % grade, 8 m clearance, no warps
};

USTRUCT(BlueprintType)
struct DFWORLD_API FDFSocketDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SocketId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFSocketTag Tag = EDFSocketTag::Ground;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PadYaw = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PadSlopePercent = 0.f;
};

USTRUCT(BlueprintType)
struct DFWORLD_API FDFStationDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct DFWORLD_API FDFTraversalDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFTraversalKind Kind = EDFTraversalKind::Ladder;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> Params;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Label;
};

USTRUCT(BlueprintType)
struct DFWORLD_API FDFVehicleSpawnDef
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName VehicleId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector Position = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Yaw = 0.f;                // Unreal degrees
};

/** The sim frame -> Unreal frame mapping, in one place (PROGRAMME.md conventions). */
struct DFWORLD_API FDFSimFrame
{
	/** metres Y-up -Z-forward -> cm Z-up +X-forward: X = -Z*100, Y = X*100, Z = Y*100. */
	static FVector ToUnreal(const FVector& SimMetres)
	{
		return FVector(-SimMetres.Z * 100.0, SimMetres.X * 100.0, SimMetres.Y * 100.0);
	}

	/** The inverse, for anything that has to compare against sim numbers (tests, node ordering). */
	static FVector ToSim(const FVector& UnrealCm)
	{
		return FVector(UnrealCm.Y / 100.0, UnrealCm.Z / 100.0, -UnrealCm.X / 100.0);
	}

	/** A sim yaw turns about +Y from -Z towards -X; the same turn in Unreal is about +Z from +X towards -Y. */
	static float YawToUnreal(float SimYawDegrees) { return -SimYawDegrees; }
};
