#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Messages/DFMessageBus.h"
#include "Subsystems/WorldSubsystem.h"
#include "DFCombatCueSubsystem.generated.h"

class AActor;
class ADFTower;
class UStaticMesh;
class UStaticMeshComponent;
struct FDFMsg_Shot;

/** What a placeholder combat cue draws (the DF.Unit.Vfx tests count them by kind). */
UENUM()
enum class EDFCombatCue : uint8
{
	/** A Bolt, Flak or Mortar round in flight: a glowing ball (a Mortar's on an arc). */
	Round,
	/** A short flash that shrinks away: where a round landed, a tesla strike ended, a beam locked on. */
	Flash,
	/** Where a Mortar round landed: a flat disc the size of the row's SplashRadius. */
	Splash,
	/** A Tesla strike or chain hop: a jagged bolt of stretched cubes. */
	Arc,
	/** A Beam tower's held beam, from its muzzle to its target, for as long as it has one. */
	Beam,
};

/** The placeholder cues' timings and sizes (looks, not rules: untuned by eye yet). */
namespace DFCombatCue
{
	/** A Tesla bolt shows this long (a strike is instant; the eye needs a few frames). */
	constexpr float ArcSeconds = 0.12f;
	constexpr float FlashSeconds = 0.12f;
	constexpr float SplashSeconds = 0.3f;
	/** A Tesla bolt is this many stretched cubes between jittered points. */
	constexpr int32 ArcSegments = 4;
	/** How far a bolt's kinks may leave the straight line: this share of its length, and never more than
	 *  ArcReachMaxCm; each kink is at least ArcMinKinkFraction of that away, so a bolt never reads as a rod. */
	constexpr float ArcReachFraction = 0.15f;
	constexpr float ArcReachMaxCm = 60.f;
	constexpr float ArcMinKinkFraction = 0.25f;
	inline float ArcReachCm(float LengthCm) { return FMath::Min(LengthCm * ArcReachFraction, ArcReachMaxCm); }
	/** A beam's thickness at no heat and at full heat (cm). */
	constexpr float BeamMinCm = 5.f;
	constexpr float BeamMaxCm = 14.f;
	/** A Mortar's disc: this thick, its underside this far above the lane, so it shows over the lane strip
	 *  (ADFLaneGraphInfo's, 1-3 cm up). */
	constexpr float SplashThicknessCm = 4.f;
	constexpr float SplashLiftCm = 1.f;
	/**
	 * A round whose cue landed on its own is still owed the host's ProjectileLanded (its body walked away, so
	 * the host's homing round took longer) for its flight time plus this. A body walking away at V delays the
	 * host's round at speed S by Lifetime x V / (S - V): under 0.4 x Lifetime for every body in enemies.json
	 * (3.8 m/s at most, a shade with its stealth bonus) against the slowest round (a nova's 14 m/s). The rest
	 * is the relay's jitter.
	 */
	constexpr float LandingGraceSeconds = 0.5f;
	/** The host's round flew at least the straight line from the muzzle to where it landed, at its speed: a
	 *  landing never lands a cue younger than this share of that time (the rest allows for the relay's jitter). */
	constexpr float LandingMinFlightFraction = 0.5f;
	/** The bolts' jitter seed in an automation test (a game's differs every run), so a test's bolts are the
	 *  same on every run and a geometry change cannot pass or fail by luck. */
	constexpr int32 TestJitterSeed = 1414;
}

/** One pooled look part: a basic shape on the Glow material, shown while a cue holds it. */
USTRUCT()
struct FDFCombatCuePart
{
	GENERATED_BODY()

	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Mesh;
	/** Which basic shape (the cpp's EShape): parts are reused only for the same shape. */
	uint8 Shape = 0;
	bool bInUse = false;
	/** The cue kind holding it, while in use. */
	EDFCombatCue Use = EDFCombatCue::Flash;
	/** The Glow colour it carries, so re-showing it in the same colour touches no material. */
	FLinearColor Colour = FLinearColor::Black;
};

/**
 * Placeholder combat cues (WS-14): every tower kind is seen fighting on every machine that renders,
 * until the Niagara systems of C10 (vfx.md; UDFVfxSubsystem) replace them. Engine basic shapes on
 * DFShapeLook's Glow material, in the firing tower's placeholder energy colour
 * (DFTowerRig::PlaceholderEnergy), so a shot reads back to the tower that fired it.
 *
 * It draws only from what every machine has. The host's towers announce their shots as team messages,
 * which ADFEventRelay re-broadcasts on every client's bus (messages.md), and a Beam tower replicates
 * its target and heat:
 * - **TowerFired** from a Bolt or Flak: a glowing round that flies Origin -> the aim point at the row's
 *   ProjectileSpeed, then a short flash. From a Mortar (or any indirect row): the same on a parabolic
 *   arc, and a disc of the row's SplashRadius where it lands.
 * - **TowerFired** from a Tesla (the strike and each chain hop): a jagged bolt for ArcSeconds.
 * - **ProjectileLanded**: the host's round arrived. Rounds from one tower at one body land in the order
 *   they were fired, on the host and here, so the message belongs to the oldest of them not yet landed
 *   by the host. If that one's cue already landed on its own (its body walked away, so the host's homing
 *   round took longer), the message is its and draws nothing; otherwise the oldest round of that tower
 *   at that body still in the air lands there and then (its body walked toward it).
 * - **EnemyKilled / EnemyLeaked**: the host drops every round at that body without a word (it lands
 *   nowhere: ADFTower::StepShots), so its cues vanish where they are, with no flash and no splash.
 * - **BeamHeld**: a flash where a Beam locked on. The beam itself is drawn every frame, from each Beam
 *   ADFTower with a replicated CurrentTarget to that target's aim point, thicker and brighter with Heat.
 *
 * Impact in FDFMsg_Shot is the target's position; a round and a bolt end 0.8 m above it
 * (DFTowerMath::ShotAimHeightCm), where the sim aims and where the host's round lands. A Mortar's disc
 * lies on the DF_LaneSurface under where its round landed, along the slope, as ADFSocket's in-lane pads do.
 *
 * Nothing is allocated per frame in steady state: parts come from a pool that grows only when more are
 * showing at once than ever before, hidden parts wait for reuse, the cue, owed-landing and tower lists
 * keep their capacity, and the towers are tracked from the actor-spawned hook rather than an actor iterator.
 *
 * Runs in game and PIE worlds only, never on a dedicated server or under -nullrhi, and never in an
 * automation test's world unless the test calls StartDrawing (a test that did not ask sees no parts).
 */
UCLASS()
class DFVFX_API UDFCombatCueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDFCombatCueSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bDrawing; }
	virtual TStatId GetStatId() const override;

	/** Listen on the world's message bus, track its towers and draw. OnWorldBeginPlay calls it in a game
	 *  that renders; an automation test calls it itself (and its bolts are shaped from TestJitterSeed).
	 *  Does nothing without a message bus. */
	void StartDrawing();
	/** Stop listening and put every part away (the holder actor and the pool with it). */
	void StopDrawing();
	bool IsDrawing() const { return bDrawing; }

	/** One frame: age every cue by DeltaSeconds and redraw the beams. Tick's body. */
	void Advance(float DeltaSeconds);

	// ---- read (tests) --------------------------------------------------------------------------------
	/** Cues of Kind showing now (for Beam: the beams drawn at the last Advance). */
	int32 NumCues(EDFCombatCue Kind) const;
	/** The parts cues of Kind are showing now, in no particular order. */
	TArray<UStaticMeshComponent*> GetParts(EDFCombatCue Kind) const;
	/** Every part made so far, shown or waiting: what the pool has grown to. */
	int32 GetPoolSize() const { return Parts.Num(); }
	int32 GetPartsInUse() const;
	/** Rounds whose cue landed on its own and whose host landing has not come yet. */
	int32 NumOwedLandings() const { return Owed.Num(); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** What a tower id's shots look like, from its row (cached per id: rows do not change in a match). */
	struct FStyle
	{
		EDFTowerKind Kind = EDFTowerKind::Bolt;
		float SpeedCmPerSecond = 0.f;
		float SplashRadiusCm = 0.f;
		bool bIndirect = false;
		FLinearColor Energy = FLinearColor::White;
	};

	struct FCue
	{
		EDFCombatCue Kind = EDFCombatCue::Flash;
		int32 StructureId = 0;
		int32 TargetId = 0;
		FVector From = FVector::ZeroVector;
		FVector To = FVector::ZeroVector;
		/** Round: the arc's height above the straight line at its middle (0 for a direct round). */
		float ApexCm = 0.f;
		/** Round: the row's speed, which the host's round flies at too. */
		float SpeedCmPerSecond = 0.f;
		/** Round and Flash: the ball's diameter. Splash: the disc's. Arc: the bolt's thickness. */
		float SizeCm = 0.f;
		/** Round: what it leaves on landing: a disc this wide (a Mortar), and a flash this wide. */
		float SplashRadiusCm = 0.f;
		float LandFlashCm = 0.f;
		float Age = 0.f;
		float Lifetime = 0.f;
		/** Arc: when to throw the bolt into a new shape (once, halfway: a flicker). */
		float RejitterAt = 0.f;
		/** The firing tower's energy colour; each look scales it to its own brightness. */
		FLinearColor Energy = FLinearColor::White;
		/** The parts it shows: one, or an Arc's segments. */
		int32 PartIndices[DFCombatCue::ArcSegments] = {};
		int32 NumParts = 0;
	};

	/** A round whose cue landed on its own before the host's round did: the host's ProjectileLanded for it is
	 *  still to come, and must not land a later round of the same tower at the same body. */
	struct FOwedLanding
	{
		int32 StructureId = 0;
		int32 TargetId = 0;
		/** Since the cue landed; the record is dropped when it reaches Wait (the cue's flight + LandingGraceSeconds). */
		float Age = 0.f;
		float Wait = 0.f;
	};

	struct FTrackedTower
	{
		TWeakObjectPtr<ADFTower> Tower;
		/** The DefId the style below was read for (a client learns it by replication, after the spawn). */
		FName DefId;
		bool bBeam = false;
		FLinearColor Energy = FLinearColor::White;
		int32 BeamPart = INDEX_NONE;
	};

	void OnTowerFired(const FDFMsg_Shot& Shot);
	void OnProjectileLanded(const FDFMsg_Shot& Shot);
	void OnBeamHeld(const FDFMsg_Shot& Shot);
	/** EnemyKilled / EnemyLeaked: the body's rounds vanish unlanded, and no landing is owed for it any more. */
	void OnBodyGone(int32 TargetId);
	void OnActorSpawned(AActor* Actor);
	void Track(ADFTower* Tower);

	/** The style for DefId, or null without a row (none, or not in towers.json: the content logs that). */
	const FStyle* FindStyle(FName DefId);

	void SpawnRound(const FDFMsg_Shot& Shot, const FStyle& Style, const FVector& End);
	void SpawnFlash(const FVector& At, float SizeCm, const FLinearColor& Energy);
	/** A disc on the lane surface under At, tilted along it; level at GroundZ where no lane surface is found. */
	void SpawnSplash(const FVector& At, double GroundZ, float RadiusCm, const FLinearColor& Energy);
	void SpawnArc(const FVector& From, const FVector& To, const FLinearColor& Energy);
	/** A round arrived At (its own end, or where the host's round landed): its flash, and a Mortar's disc. */
	void Land(const FCue& Round, const FVector& At);
	/** A round reached its own end before any host landing for it: it lands there, and its landing is owed. */
	void LandOnItsOwn(const FCue& Round);
	/** Lay an arc's segments along a freshly jittered path from its From to its To. */
	void ShapeArc(const FCue& Arc);
	void StepCues(float DeltaSeconds);
	void DrawBeams();
	/** StopDrawing's body. The holder actor is destroyed only when the world is not going away with it. */
	void Stop(bool bDestroyHolder);

	/** A part of Shape in Colour, shown, for a cue of Use. INDEX_NONE if none can be made. */
	int32 AcquirePart(uint8 Shape, EDFCombatCue Use, const FLinearColor& Colour);
	void ReleasePart(int32 Index);
	void ReleaseCue(FCue& Cue);
	void SetPartColour(int32 Index, const FLinearColor& Colour);
	UStaticMeshComponent* PartMesh(int32 Index) const;
	TArray<int32>& FreeListFor(uint8 Shape);
	/** Spawn the actor the parts hang off, and find the shapes. False if either cannot be had. */
	bool EnsureHolder();
	/** The holder went away under us (its level did): its parts went with it, and so do the cues using them. */
	void ForgetParts();

	bool bDrawing = false;
	TWeakObjectPtr<UDFMessageBus> Bus;
	FDFMessageHandle FiredHandle;
	FDFMessageHandle LandedHandle;
	FDFMessageHandle BeamHandle;
	FDFMessageHandle KilledHandle;
	FDFMessageHandle LeakedHandle;
	FDelegateHandle SpawnedHandle;

	/** A transient, unreplicated actor in the world that owns every part. */
	UPROPERTY(Transient) TObjectPtr<AActor> Holder;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY(Transient) TArray<FDFCombatCuePart> Parts;
	/** Hidden parts by shape, ready for reuse. */
	TArray<int32> FreeSpheres;
	TArray<int32> FreeCubes;
	TArray<int32> FreeCylinders;

	TArray<FCue> Cues;
	TArray<FOwedLanding> Owed;
	TArray<FTrackedTower> Towers;
	TMap<FName, FStyle> Styles;
	FRandomStream Jitter;
};
