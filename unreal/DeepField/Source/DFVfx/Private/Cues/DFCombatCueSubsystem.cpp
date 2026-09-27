#include "Cues/DFCombatCueSubsystem.h"

#include "CollisionQueryParams.h"
#include "Combat/DFTargetable.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/DFContentSubsystem.h"
#include "DFGameplayTags.h"
#include "DFWorldCollision.h"
#include "Engine/CollisionProfile.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Look/DFShapeLook.h"
#include "Messages/DFMessages.h"
#include "Misc/App.h"
#include "Rig/DFTowerRig.h"
#include "Rig/DFTowerRigComponent.h"
#include "Towers/DFTower.h"
#include "Towers/DFTowerMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFCombatCueSubsystem)

namespace
{
	/** The engine basic shapes a cue is made of: 1 m, pivot at the centre; the cylinder stands on Z. */
	enum class EShape : uint8
	{
		Sphere,
		Cube,
		Cylinder,
	};

	constexpr uint8 AsByte(EShape Shape) { return static_cast<uint8>(Shape); }

	// Brightness: EmissiveMeshMaterial's Color is the light the part gives off, so past 1 it blooms. The
	// energy colours are 0..1; each look scales them (the old host-only round glowed at about 4).
	constexpr float RoundGlow = 4.f;
	constexpr float FlashGlow = 6.f;
	constexpr float SplashGlow = 2.5f;
	constexpr float ArcGlow = 8.f;
	constexpr float BeamGlowCold = 2.5f;
	constexpr float BeamGlowHot = 10.f;

	// Sizes (cm), read from the demo's orbit camera: a round must be seen, a flash must not hide the body.
	constexpr float BoltRoundCm = 30.f;
	constexpr float FlakRoundCm = 22.f;
	constexpr float MortarRoundCm = 45.f;
	constexpr float BoltFlashCm = 60.f;
	constexpr float MortarFlashCm = 120.f;
	constexpr float ArcThicknessCm = 7.f;
	constexpr float ArcFlashCm = 35.f;
	constexpr float BeamLockFlashCm = 45.f;

	// The lane surface a Mortar's disc lies on is looked for from this far above the landing (or the ground the
	// body stood on, if higher) to this far below: a round can land on its flight line, above the body.
	constexpr double SplashTraceUpCm = 150.0;
	constexpr double SplashTraceDownCm = 450.0;

	/** A lob's height at its middle for a flight of DistanceCm: a third of the distance, within reason. */
	float LobApexCm(float DistanceCm)
	{
		return FMath::Clamp(DistanceCm * 0.3f, 150.f, 900.f);
	}

	/** Alpha (0..1) of the way from From to To, lifted ApexCm at the middle on a parabola (0: a straight line). */
	FVector RoundPosition(const FVector& From, const FVector& To, float ApexCm, float Alpha)
	{
		return FMath::Lerp(From, To, static_cast<double>(Alpha)) + FVector(0.f, 0.f, 4.f * ApexCm * Alpha * (1.f - Alpha));
	}

	/** A 1 m basic shape stretched from From to To and ThicknessCm across: a cube lies along X, a cylinder along Z. */
	FTransform Span(const FVector& From, const FVector& To, float ThicknessCm, bool bAlongZ)
	{
		const FVector Delta = To - From;
		const double Length = Delta.Size();
		const FVector Direction = Length > UE_KINDA_SMALL_NUMBER ? Delta / Length : FVector::UpVector;
		const double Across = ThicknessCm / 100.0;
		const double Along = FMath::Max(Length, 1.0) / 100.0;
		const FQuat Rotation = bAlongZ ? FRotationMatrix::MakeFromZ(Direction).ToQuat() : FRotationMatrix::MakeFromX(Direction).ToQuat();
		return FTransform(Rotation, (From + To) * 0.5, bAlongZ ? FVector(Across, Across, Along) : FVector(Along, Across, Across));
	}

	/** Where a tower's shot at Target ends: its aim point (the sim's 0.8 m above the body) when it is a body. */
	FVector AimPointOf(const AActor& Target)
	{
		const IDFTargetable* Body = Cast<IDFTargetable>(&Target);
		return Body ? Body->GetAimPoint() : Target.GetActorLocation() + FVector(0.f, 0.f, DFTowerMath::ShotAimHeightCm);
	}
}

UDFCombatCueSubsystem* UDFCombatCueSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDFCombatCueSubsystem>() : nullptr;
}

bool UDFCombatCueSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Worlds that play a match; never the editor's own world or a preview scene.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UDFCombatCueSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UDFCombatCueSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// A game that renders draws. A dedicated server (PIE's in-process one too), a -nullrhi run and an
	// automation test's world (PIE functional tests included) do not: a test that wants cues starts them
	// itself, so no other test meets a part it did not ask for.
	if (InWorld.GetNetMode() != NM_DedicatedServer && FApp::CanEverRender() && !GIsAutomationTesting)
	{
		StartDrawing();
	}
}

void UDFCombatCueSubsystem::Deinitialize()
{
	Stop(/*bDestroyHolder*/ false);   // the world is going away, and its actors with it
	Super::Deinitialize();
}

TStatId UDFCombatCueSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDFCombatCueSubsystem, STATGROUP_Tickables);
}

void UDFCombatCueSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Advance(DeltaTime);
}

void UDFCombatCueSubsystem::StartDrawing()
{
	UWorld* World = GetWorld();
	UDFMessageBus* MessageBus = UDFMessageBus::Get(this);
	if (bDrawing || !World || !MessageBus)
	{
		return;
	}
	bDrawing = true;
	Bus = MessageBus;
	// Exact tags: these three are leaves, and a parent subscription would hear every message there is.
	TWeakObjectPtr<UDFCombatCueSubsystem> WeakThis(this);
	FiredHandle = MessageBus->Subscribe<FDFMsg_Shot>(DFTags::Message_TowerFired, [WeakThis](const FGameplayTag&, const FDFMsg_Shot& Shot)
	{
		if (UDFCombatCueSubsystem* Self = WeakThis.Get())
		{
			Self->OnTowerFired(Shot);
		}
	}, /*bIncludeChildren*/ false);
	LandedHandle = MessageBus->Subscribe<FDFMsg_Shot>(DFTags::Message_ProjectileLanded, [WeakThis](const FGameplayTag&, const FDFMsg_Shot& Shot)
	{
		if (UDFCombatCueSubsystem* Self = WeakThis.Get())
		{
			Self->OnProjectileLanded(Shot);
		}
	}, /*bIncludeChildren*/ false);
	BeamHandle = MessageBus->Subscribe<FDFMsg_Shot>(DFTags::Message_BeamHeld, [WeakThis](const FGameplayTag&, const FDFMsg_Shot& Shot)
	{
		if (UDFCombatCueSubsystem* Self = WeakThis.Get())
		{
			Self->OnBeamHeld(Shot);
		}
	}, /*bIncludeChildren*/ false);
	// A body that died or reached the core: team messages too (ADFEnemy), relayed like the shots.
	KilledHandle = MessageBus->Subscribe<FDFMsg_Kill>(DFTags::Message_EnemyKilled, [WeakThis](const FGameplayTag&, const FDFMsg_Kill& Kill)
	{
		if (UDFCombatCueSubsystem* Self = WeakThis.Get())
		{
			Self->OnBodyGone(Kill.EnemyId);
		}
	}, /*bIncludeChildren*/ false);
	LeakedHandle = MessageBus->Subscribe<FDFMsg_Enemy>(DFTags::Message_EnemyLeaked, [WeakThis](const FGameplayTag&, const FDFMsg_Enemy& Leak)
	{
		if (UDFCombatCueSubsystem* Self = WeakThis.Get())
		{
			Self->OnBodyGone(Leak.EnemyId);
		}
	}, /*bIncludeChildren*/ false);

	// The towers there are now, then each as it spawns: on a client that is when the host's tower replicates
	// in, whenever that is relative to its TowerPlaced message. One iteration here, none per frame.
	SpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UDFCombatCueSubsystem::OnActorSpawned));
	for (TActorIterator<ADFTower> It(World); It; ++It)
	{
		Track(*It);
	}
	// A game's bolts differ from run to run; a test's are the same on every run.
	if (GIsAutomationTesting)
	{
		Jitter.Initialize(DFCombatCue::TestJitterSeed);
	}
	else
	{
		Jitter.GenerateNewSeed();
	}
	Cues.Reserve(64);
	Owed.Reserve(32);
	Towers.Reserve(32);
}

void UDFCombatCueSubsystem::StopDrawing()
{
	Stop(/*bDestroyHolder*/ true);
}

void UDFCombatCueSubsystem::Stop(bool bDestroyHolder)
{
	if (!bDrawing)
	{
		return;
	}
	bDrawing = false;
	if (UDFMessageBus* MessageBus = Bus.Get())
	{
		MessageBus->Unsubscribe(FiredHandle);
		MessageBus->Unsubscribe(LandedHandle);
		MessageBus->Unsubscribe(BeamHandle);
		MessageBus->Unsubscribe(KilledHandle);
		MessageBus->Unsubscribe(LeakedHandle);
	}
	Bus.Reset();
	FiredHandle = FDFMessageHandle();
	LandedHandle = FDFMessageHandle();
	BeamHandle = FDFMessageHandle();
	KilledHandle = FDFMessageHandle();
	LeakedHandle = FDFMessageHandle();
	UWorld* World = GetWorld();
	if (World)
	{
		World->RemoveOnActorSpawnedHandler(SpawnedHandle);
	}
	SpawnedHandle.Reset();

	// The parts belong to the holder and go with it.
	if (bDestroyHolder && IsValid(Holder) && !Holder->IsActorBeingDestroyed() && World && !World->bIsTearingDown)
	{
		Holder->Destroy();
	}
	Holder = nullptr;
	Parts.Reset();
	FreeSpheres.Reset();
	FreeCubes.Reset();
	FreeCylinders.Reset();
	Cues.Reset();
	Owed.Reset();
	Towers.Reset();
}

void UDFCombatCueSubsystem::Advance(float DeltaSeconds)
{
	if (!bDrawing)
	{
		return;
	}
	StepCues(DeltaSeconds);
	DrawBeams();
}

// ---- messages ---------------------------------------------------------------------------------------

void UDFCombatCueSubsystem::OnTowerFired(const FDFMsg_Shot& Shot)
{
	const FStyle* Found = FindStyle(Shot.DefId);
	if (!Found)
	{
		return;
	}
	const FStyle Style = *Found;
	const FVector End = Shot.Impact + FVector(0.f, 0.f, DFTowerMath::ShotAimHeightCm);
	switch (Style.Kind)
	{
	case EDFTowerKind::Bolt:
	case EDFTowerKind::Mortar:
	case EDFTowerKind::Flak:
		SpawnRound(Shot, Style, End);
		break;
	case EDFTowerKind::Tesla:
		// The strike and every chain hop arrive as their own TowerFired (ADFTower::FireTesla).
		SpawnArc(Shot.Origin, End, Style.Energy);
		SpawnFlash(End, ArcFlashCm, Style.Energy);
		break;
	default:
		break;   // a Beam is drawn from its replicated target; Aura, Barricade and Support never fire
	}
}

void UDFCombatCueSubsystem::OnProjectileLanded(const FDFMsg_Shot& Shot)
{
	// The host's round landed at Impact. Rounds from one tower at one body land in the order they were fired,
	// on the host (same muzzle, same speed, homing on the same aim point) and here (the body cannot close on a
	// round faster than it flies), so this landing is the oldest round's that the host has not landed yet.
	// If that round's cue already landed on its own, this is its landing arriving late (its body walked away,
	// so the host's round flew further than the cue): it has shown its flash, and a later round keeps flying.
	int32 OldestOwed = INDEX_NONE;
	for (int32 i = 0; i < Owed.Num(); ++i)
	{
		const FOwedLanding& Entry = Owed[i];
		if (Entry.StructureId == Shot.StructureId && Entry.TargetId == Shot.TargetId
			&& (OldestOwed == INDEX_NONE || Entry.Age > Owed[OldestOwed].Age))
		{
			OldestOwed = i;
		}
	}
	if (OldestOwed != INDEX_NONE)
	{
		Owed.RemoveAtSwap(OldestOwed, EAllowShrinking::No);
		return;
	}

	// Otherwise it is the oldest of that tower's rounds at that body still in the air here, which arrived early
	// (its body walked toward it): it lands where the host's did, now.
	int32 Oldest = INDEX_NONE;
	for (int32 i = 0; i < Cues.Num(); ++i)
	{
		const FCue& Cue = Cues[i];
		if (Cue.Kind == EDFCombatCue::Round && Cue.StructureId == Shot.StructureId && Cue.TargetId == Shot.TargetId
			&& (Oldest == INDEX_NONE || Cue.Age > Cues[Oldest].Age))
		{
			Oldest = i;
		}
	}
	if (Oldest == INDEX_NONE)
	{
		return;
	}
	// Unless it cannot be: the host's round flew at least the straight line from the muzzle to where it landed.
	// A cue much younger than that is a later round, and this landing is one of a round this machine never saw
	// fly (it joined since) or whose owed record ran out: it draws nothing.
	const FCue& Candidate = Cues[Oldest];
	const float MinFlightSeconds = Candidate.SpeedCmPerSecond > 0.f
		? static_cast<float>(FVector::Dist(Candidate.From, Shot.Impact)) / Candidate.SpeedCmPerSecond : 0.f;
	if (Candidate.Age < DFCombatCue::LandingMinFlightFraction * MinFlightSeconds)
	{
		return;
	}
	const FCue Round = Candidate;
	ReleaseCue(Cues[Oldest]);
	Cues.RemoveAtSwap(Oldest, EAllowShrinking::No);
	Land(Round, Shot.Impact);
}

void UDFCombatCueSubsystem::OnBodyGone(int32 TargetId)
{
	// The host drops a round whose body died or left, dealing nothing and saying nothing (ADFTower::StepShots:
	// "it lands nowhere"), so a cue still flying at it vanishes where it is: no flash, and no disc claiming a
	// splash that never happened. The round that killed it landed first (the host sends that landing in the
	// frame of the hit and EnemyKilled the frame after, and the relay's one reliable multicast keeps the order).
	for (int32 i = Cues.Num() - 1; i >= 0; --i)
	{
		if (Cues[i].Kind == EDFCombatCue::Round && Cues[i].TargetId == TargetId)
		{
			ReleaseCue(Cues[i]);
			Cues.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}
	// And a landing owed at it will never come.
	for (int32 i = Owed.Num() - 1; i >= 0; --i)
	{
		if (Owed[i].TargetId == TargetId)
		{
			Owed.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}
}

void UDFCombatCueSubsystem::OnBeamHeld(const FDFMsg_Shot& Shot)
{
	// A Beam locked on something new: a spark on it. The beam itself is DrawBeams', every frame.
	if (const FStyle* Style = FindStyle(Shot.DefId))
	{
		const FLinearColor Energy = Style->Energy;
		SpawnFlash(Shot.Impact + FVector(0.f, 0.f, DFTowerMath::ShotAimHeightCm), BeamLockFlashCm, Energy);
	}
}

void UDFCombatCueSubsystem::OnActorSpawned(AActor* Actor)
{
	if (ADFTower* Tower = Cast<ADFTower>(Actor))
	{
		Track(Tower);
	}
}

void UDFCombatCueSubsystem::Track(ADFTower* Tower)
{
	if (!Tower)
	{
		return;
	}
	for (const FTrackedTower& Known : Towers)
	{
		if (Known.Tower.Get() == Tower)
		{
			return;
		}
	}
	FTrackedTower& Tracked = Towers.AddDefaulted_GetRef();
	Tracked.Tower = Tower;
}

const UDFCombatCueSubsystem::FStyle* UDFCombatCueSubsystem::FindStyle(FName DefId)
{
	if (DefId.IsNone())
	{
		return nullptr;
	}
	if (const FStyle* Known = Styles.Find(DefId))
	{
		return Known;
	}
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	const FDFTowerRow* Row = (Content && Content->IsReady()) ? Content->Tower(DefId) : nullptr;
	if (!Row)
	{
		return nullptr;   // not in towers.json: the content subsystem has said so, once
	}
	FStyle Style;
	Style.Kind = Row->Kind;
	Style.SpeedCmPerSecond = Row->ProjectileSpeed * 100.f;
	Style.SplashRadiusCm = Row->SplashRadius * 100.f;
	// towers.json marks no row Indirect yet: a Mortar lobs by its kind (WS-04's "Mortar(indirect)"), and the
	// flag adds any other row that should.
	Style.bIndirect = Row->bIndirect || Row->Kind == EDFTowerKind::Mortar;
	Style.Energy = DFTowerRig::PlaceholderEnergy(DefId);
	return &Styles.Add(DefId, Style);
}

// ---- cues -------------------------------------------------------------------------------------------

void UDFCombatCueSubsystem::SpawnRound(const FDFMsg_Shot& Shot, const FStyle& Style, const FVector& End)
{
	const float DistanceCm = static_cast<float>(FVector::Dist(Shot.Origin, End));
	FCue Round;
	Round.Kind = EDFCombatCue::Round;
	Round.StructureId = Shot.StructureId;
	Round.TargetId = Shot.TargetId;
	Round.From = Shot.Origin;
	Round.To = End;
	Round.ApexCm = (Style.bIndirect || Shot.bIndirect) ? LobApexCm(DistanceCm) : 0.f;
	Round.SpeedCmPerSecond = Style.SpeedCmPerSecond;
	Round.SizeCm = Style.Kind == EDFTowerKind::Mortar ? MortarRoundCm : (Style.Kind == EDFTowerKind::Flak ? FlakRoundCm : BoltRoundCm);
	Round.SplashRadiusCm = Style.SplashRadiusCm;
	Round.LandFlashCm = Style.SplashRadiusCm > 0.f ? MortarFlashCm : BoltFlashCm;
	Round.Energy = Style.Energy;
	if (Style.SpeedCmPerSecond <= 0.f || DistanceCm < 1.f)
	{
		LandOnItsOwn(Round);   // nothing to fly: it lands at once, before the host's round can
		return;
	}
	// The sim's rounds fly straight at their speed, so a lob takes the straight line's time too and lands
	// when the host's round does.
	Round.Lifetime = DistanceCm / Style.SpeedCmPerSecond;
	const int32 Part = AcquirePart(AsByte(EShape::Sphere), EDFCombatCue::Round, Round.Energy * RoundGlow);
	UStaticMeshComponent* Mesh = PartMesh(Part);
	if (!Mesh)
	{
		return;
	}
	Mesh->SetWorldTransform(FTransform(FQuat::Identity, Round.From, FVector(Round.SizeCm / 100.f)));
	Round.PartIndices[0] = Part;
	Round.NumParts = 1;
	Cues.Add(Round);
}

void UDFCombatCueSubsystem::SpawnFlash(const FVector& At, float SizeCm, const FLinearColor& Energy)
{
	const int32 Part = AcquirePart(AsByte(EShape::Sphere), EDFCombatCue::Flash, Energy * FlashGlow);
	UStaticMeshComponent* Mesh = PartMesh(Part);
	if (!Mesh)
	{
		return;
	}
	Mesh->SetWorldTransform(FTransform(FQuat::Identity, At, FVector(SizeCm / 100.f)));
	FCue Flash;
	Flash.Kind = EDFCombatCue::Flash;
	Flash.From = At;
	Flash.To = At;
	Flash.SizeCm = SizeCm;
	Flash.Lifetime = DFCombatCue::FlashSeconds;
	Flash.Energy = Energy;
	Flash.PartIndices[0] = Part;
	Flash.NumParts = 1;
	Cues.Add(Flash);
}

void UDFCombatCueSubsystem::SpawnSplash(const FVector& At, double GroundZ, float RadiusCm, const FLinearColor& Energy)
{
	const int32 Part = AcquirePart(AsByte(EShape::Cylinder), EDFCombatCue::Splash, Energy * SplashGlow);
	UStaticMeshComponent* Mesh = PartMesh(Part);
	if (!Mesh)
	{
		return;
	}
	// A disc the splash's size: everything inside it took damage (Step.cs splashes every body within the radius
	// of the struck body's position on the lane). It lies on the lane surface under the landing, along it, found
	// the way ADFSocket::AlignToLaneSurface finds an in-lane pad's: Foundry's lanes climb 13-25 %, where a level
	// 6.4 m disc would hang 0.8 m over the lane at its downhill rim and sink as far into it uphill. With no lane
	// surface there (or one steeper than 60 degrees), it lies level on the ground the body stood on.
	FVector Surface(At.X, At.Y, GroundZ);
	FVector Normal = FVector::UpVector;
	if (UWorld* World = GetWorld())
	{
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(DFCueSplashSurface), /*bTraceComplex*/ true);
		const FVector Top(At.X, At.Y, FMath::Max(At.Z, GroundZ) + SplashTraceUpCm);
		const FVector Bottom(At.X, At.Y, FMath::Min(At.Z, GroundZ) - SplashTraceDownCm);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Top, Bottom, DFCollision::LaneSurface, Params) && Hit.ImpactNormal.Z >= 0.5)
		{
			Surface = Hit.ImpactPoint;
			Normal = Hit.ImpactNormal;
		}
	}
	const double Across = 2.0 * RadiusCm / 100.0;
	const FVector Centre = Surface + Normal * (DFCombatCue::SplashLiftCm + DFCombatCue::SplashThicknessCm * 0.5);
	Mesh->SetWorldTransform(FTransform(FQuat::FindBetweenNormals(FVector::UpVector, Normal), Centre,
		FVector(Across, Across, DFCombatCue::SplashThicknessCm / 100.0)));
	FCue Splash;
	Splash.Kind = EDFCombatCue::Splash;
	Splash.From = Centre;
	Splash.To = Centre;
	Splash.SizeCm = 2.f * RadiusCm;
	Splash.Lifetime = DFCombatCue::SplashSeconds;
	Splash.Energy = Energy;
	Splash.PartIndices[0] = Part;
	Splash.NumParts = 1;
	Cues.Add(Splash);
}

void UDFCombatCueSubsystem::SpawnArc(const FVector& From, const FVector& To, const FLinearColor& Energy)
{
	FCue Arc;
	Arc.Kind = EDFCombatCue::Arc;
	Arc.From = From;
	Arc.To = To;
	Arc.SizeCm = ArcThicknessCm;
	Arc.Lifetime = DFCombatCue::ArcSeconds;
	Arc.RejitterAt = DFCombatCue::ArcSeconds * 0.5f;
	Arc.Energy = Energy;
	for (int32 i = 0; i < DFCombatCue::ArcSegments; ++i)
	{
		const int32 Part = AcquirePart(AsByte(EShape::Cube), EDFCombatCue::Arc, Energy * ArcGlow);
		if (!PartMesh(Part))
		{
			break;
		}
		Arc.PartIndices[Arc.NumParts++] = Part;
	}
	if (Arc.NumParts == 0)
	{
		return;
	}
	ShapeArc(Arc);
	Cues.Add(Arc);
}

void UDFCombatCueSubsystem::ShapeArc(const FCue& Arc)
{
	// A bolt's kinks: each inner point leaves the straight line in a random direction across it, by a quarter
	// of Reach to all of it (DFCombatCue::ArcReachCm: 15 % of its length, 60 cm at most). The direction is an
	// angle round the line, so no kink strays past Reach (two independent side and up offsets would reach a
	// square's corner, Reach x 1.41). The ends stay on the muzzle (or the body it hops from) and the body.
	const FVector Delta = Arc.To - Arc.From;
	const double Length = Delta.Size();
	const FVector Direction = Length > UE_KINDA_SMALL_NUMBER ? Delta / Length : FVector::UpVector;
	FVector Side;
	FVector Up;
	Direction.FindBestAxisVectors(Side, Up);   // unit, and square to Direction and each other
	const double Reach = DFCombatCue::ArcReachCm(static_cast<float>(Length));
	FVector Previous = Arc.From;
	for (int32 i = 0; i < Arc.NumParts; ++i)
	{
		FVector Next = Arc.To;
		if (i < Arc.NumParts - 1)
		{
			const double Alpha = static_cast<double>(i + 1) / Arc.NumParts;
			const float Angle = Jitter.FRandRange(0.f, 2.f * UE_PI);
			const double Offset = Reach * Jitter.FRandRange(DFCombatCue::ArcMinKinkFraction, 1.f);
			Next = FMath::Lerp(Arc.From, Arc.To, Alpha) + (Side * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * Offset;
		}
		if (UStaticMeshComponent* Mesh = PartMesh(Arc.PartIndices[i]))
		{
			Mesh->SetWorldTransform(Span(Previous, Next, Arc.SizeCm, /*bAlongZ*/ false));
		}
		Previous = Next;
	}
}

void UDFCombatCueSubsystem::Land(const FCue& Round, const FVector& At)
{
	if (Round.SplashRadiusCm > 0.f)
	{
		// Under the landing point. At is the round's end (the body's aim point, 0.8 m up) or the host's round's
		// last position, short of the body on its flight line and so higher when the tower stands above it: the
		// ground to fall back to is where the body stood when the round was fired, 0.8 m under Round.To.
		SpawnSplash(At, Round.To.Z - DFTowerMath::ShotAimHeightCm, Round.SplashRadiusCm, Round.Energy);
	}
	SpawnFlash(At, Round.LandFlashCm, Round.Energy);
}

void UDFCombatCueSubsystem::LandOnItsOwn(const FCue& Round)
{
	// The host's round is still flying after a body that walked away, and its ProjectileLanded is this round's,
	// not the next one's. If it never comes (its body died or left, or its tower went), EnemyKilled/EnemyLeaked
	// or the wait clears the record.
	FOwedLanding& Entry = Owed.AddDefaulted_GetRef();
	Entry.StructureId = Round.StructureId;
	Entry.TargetId = Round.TargetId;
	Entry.Wait = Round.Lifetime + DFCombatCue::LandingGraceSeconds;
	Land(Round, Round.To);
}

void UDFCombatCueSubsystem::StepCues(float DeltaSeconds)
{
	// Owed landings first, so one a round adds below starts its wait next frame. One whose wait ran out was
	// never coming: its round went with its tower.
	for (int32 i = Owed.Num() - 1; i >= 0; --i)
	{
		Owed[i].Age += DeltaSeconds;
		if (Owed[i].Age >= Owed[i].Wait)
		{
			Owed.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}

	// Backwards: a finished cue is swapped for the last one, which has then been visited already, and a cue
	// a landing adds (appended) waits for the next frame.
	for (int32 i = Cues.Num() - 1; i >= 0; --i)
	{
		FCue& Cue = Cues[i];
		Cue.Age += DeltaSeconds;
		if (Cue.Age >= Cue.Lifetime)
		{
			const FCue Done = Cue;
			ReleaseCue(Cue);
			Cues.RemoveAtSwap(i, EAllowShrinking::No);
			if (Done.Kind == EDFCombatCue::Round)
			{
				LandOnItsOwn(Done);   // after the release: the flash takes the round's own ball
			}
			continue;
		}
		const float Alpha = Cue.Age / Cue.Lifetime;
		// None once ForgetParts took them (the cue runs out unseen): its old index may be another cue's part now.
		UStaticMeshComponent* Mesh = Cue.NumParts > 0 ? PartMesh(Cue.PartIndices[0]) : nullptr;
		switch (Cue.Kind)
		{
		case EDFCombatCue::Round:
			if (Mesh)
			{
				Mesh->SetWorldLocation(RoundPosition(Cue.From, Cue.To, Cue.ApexCm, Alpha));
			}
			break;
		case EDFCombatCue::Flash:
			if (Mesh)
			{
				Mesh->SetWorldScale3D(FVector(FMath::Max(1.f, Cue.SizeCm * (1.f - Alpha)) / 100.f));   // shrinks away
			}
			break;
		case EDFCombatCue::Arc:
			if (Cue.RejitterAt > 0.f && Cue.Age >= Cue.RejitterAt)
			{
				Cue.RejitterAt = 0.f;
				ShapeArc(Cue);   // one flicker into a new shape
			}
			break;
		default:
			break;
		}
	}
}

void UDFCombatCueSubsystem::DrawBeams()
{
	for (int32 i = Towers.Num() - 1; i >= 0; --i)
	{
		FTrackedTower& Tracked = Towers[i];
		ADFTower* Tower = Tracked.Tower.Get();
		if (!Tower || Tower->IsActorBeingDestroyed())
		{
			ReleasePart(Tracked.BeamPart);   // sold or destroyed
			Towers.RemoveAtSwap(i, EAllowShrinking::No);
			continue;
		}
		if (Tower->GetDefId() != Tracked.DefId)
		{
			// Once per tower (on a client, when its DefId replicates): what it is, and its colour.
			Tracked.DefId = Tower->GetDefId();
			const FStyle* Style = FindStyle(Tracked.DefId);
			Tracked.bBeam = Style && Style->Kind == EDFTowerKind::Beam;
			Tracked.Energy = Style ? Style->Energy : FLinearColor::White;
		}
		AActor* Target = Tracked.bBeam ? Tower->GetCurrentTarget() : nullptr;
		if (!IsValid(Target))
		{
			ReleasePart(Tracked.BeamPart);
			Tracked.BeamPart = INDEX_NONE;
			continue;
		}
		if (!PartMesh(Tracked.BeamPart))
		{
			Tracked.BeamPart = AcquirePart(AsByte(EShape::Cylinder), EDFCombatCue::Beam, Tracked.Energy * BeamGlowCold);
		}
		UStaticMeshComponent* Mesh = PartMesh(Tracked.BeamPart);
		if (!Mesh)
		{
			continue;
		}
		// Heat is the ramp (0..255 of the way to its cap), replicated for exactly this: the longer it holds
		// one target, the thicker and hotter the beam.
		const float Heat = Tower->GetHeat() / 255.f;
		const FVector Muzzle = Tower->GetRig() ? Tower->GetRig()->GetMuzzleLocation()
			: Tower->GetActorLocation() + FVector(0.f, 0.f, DFTowerMath::ShotMuzzleHeightCm);
		Mesh->SetWorldTransform(Span(Muzzle, AimPointOf(*Target), FMath::Lerp(DFCombatCue::BeamMinCm, DFCombatCue::BeamMaxCm, Heat), /*bAlongZ*/ true));
		SetPartColour(Tracked.BeamPart, Tracked.Energy * FMath::Lerp(BeamGlowCold, BeamGlowHot, Heat));
	}
}

// ---- the pool ---------------------------------------------------------------------------------------

TArray<int32>& UDFCombatCueSubsystem::FreeListFor(uint8 Shape)
{
	switch (static_cast<EShape>(Shape))
	{
	case EShape::Cube:     return FreeCubes;
	case EShape::Cylinder: return FreeCylinders;
	default:               return FreeSpheres;
	}
}

UStaticMeshComponent* UDFCombatCueSubsystem::PartMesh(int32 Index) const
{
	return (Parts.IsValidIndex(Index) && IsValid(Parts[Index].Mesh)) ? Parts[Index].Mesh.Get() : nullptr;
}

int32 UDFCombatCueSubsystem::AcquirePart(uint8 Shape, EDFCombatCue Use, const FLinearColor& Colour)
{
	if (!EnsureHolder())
	{
		return INDEX_NONE;
	}
	TArray<int32>& Free = FreeListFor(Shape);
	int32 Index = INDEX_NONE;
	while (Index == INDEX_NONE && Free.Num() > 0)
	{
		const int32 Waiting = Free.Pop(EAllowShrinking::No);
		Index = PartMesh(Waiting) ? Waiting : INDEX_NONE;
	}
	if (Index == INDEX_NONE)
	{
		// More showing at once than ever before: the pool grows by one. Not per frame in steady state.
		UStaticMesh* Mesh = Shape == AsByte(EShape::Cube) ? CubeMesh.Get() : (Shape == AsByte(EShape::Cylinder) ? CylinderMesh.Get() : SphereMesh.Get());
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Holder.Get(), MakeUniqueObjectName(Holder.Get(), UStaticMeshComponent::StaticClass(), TEXT("CuePart")), RF_Transient);
		Part->SetStaticMesh(Mesh);
		Part->SetMobility(EComponentMobility::Movable);
		// A look and nothing else (DFWorldLook's rule): no query, sweep, overlap or navigation ever meets it.
		// The NoCollision profile names only the engine channels, so every channel is said explicitly.
		Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCollisionResponseToAllChannels(ECR_Ignore);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		Part->SetUsingAbsoluteLocation(true);
		Part->SetUsingAbsoluteRotation(true);
		Part->SetUsingAbsoluteScale(true);
		Part->SetupAttachment(Holder->GetRootComponent());
		Part->RegisterComponent();
		DFShapeLook::Glow(Part, Colour);
		Index = Parts.AddDefaulted();
		Parts[Index].Mesh = Part;
		Parts[Index].Shape = Shape;
		Parts[Index].Colour = Colour;
	}
	FDFCombatCuePart& Entry = Parts[Index];
	Entry.bInUse = true;
	Entry.Use = Use;
	SetPartColour(Index, Colour);
	Entry.Mesh->SetVisibility(true);
	return Index;
}

void UDFCombatCueSubsystem::ReleasePart(int32 Index)
{
	if (!Parts.IsValidIndex(Index) || !Parts[Index].bInUse)
	{
		return;
	}
	FDFCombatCuePart& Entry = Parts[Index];
	Entry.bInUse = false;
	if (IsValid(Entry.Mesh))
	{
		Entry.Mesh->SetVisibility(false);
		FreeListFor(Entry.Shape).Add(Index);
	}
}

void UDFCombatCueSubsystem::ReleaseCue(FCue& Cue)
{
	for (int32 i = 0; i < Cue.NumParts; ++i)
	{
		ReleasePart(Cue.PartIndices[i]);
	}
	Cue.NumParts = 0;
}

void UDFCombatCueSubsystem::SetPartColour(int32 Index, const FLinearColor& Colour)
{
	if (!Parts.IsValidIndex(Index) || Parts[Index].Colour.Equals(Colour))
	{
		return;
	}
	if (UStaticMeshComponent* Mesh = PartMesh(Index))
	{
		DFShapeLook::Glow(Mesh, Colour);   // the part's own dynamic instance, re-tinted
		Parts[Index].Colour = Colour;
	}
}

bool UDFCombatCueSubsystem::EnsureHolder()
{
	if (IsValid(Holder) && !Holder->IsActorBeingDestroyed())
	{
		return true;
	}
	if (Holder || Parts.Num() > 0)
	{
		ForgetParts();
	}
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return false;
	}
	if (!SphereMesh || !CubeMesh || !CylinderMesh)
	{
		SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (!SphereMesh || !CubeMesh || !CylinderMesh)
		{
			return false;
		}
	}
	// This machine's own actor: transient (never saved), unreplicated (AActor's default), in the persistent level.
	FActorSpawnParameters Params;
	Params.OverrideLevel = World->PersistentLevel.Get();
	Params.Name = MakeUniqueObjectName(World->PersistentLevel.Get(), AActor::StaticClass(), TEXT("DFCombatCues"));
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Spawned = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Spawned)
	{
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Spawned, TEXT("Root"));
	Spawned->SetRootComponent(Root);
	Root->RegisterComponent();
	Holder = Spawned;
	return true;
}

void UDFCombatCueSubsystem::ForgetParts()
{
	Holder = nullptr;
	Parts.Reset();
	FreeSpheres.Reset();
	FreeCubes.Reset();
	FreeCylinders.Reset();
	// The cues run out their time unseen (this can happen mid-StepCues, so the list keeps its length).
	for (FCue& Cue : Cues)
	{
		Cue.NumParts = 0;
	}
	for (FTrackedTower& Tracked : Towers)
	{
		Tracked.BeamPart = INDEX_NONE;
	}
}

// ---- read (tests) -----------------------------------------------------------------------------------

int32 UDFCombatCueSubsystem::NumCues(EDFCombatCue Kind) const
{
	int32 Count = 0;
	if (Kind == EDFCombatCue::Beam)
	{
		for (const FTrackedTower& Tracked : Towers)
		{
			Count += PartMesh(Tracked.BeamPart) ? 1 : 0;
		}
		return Count;
	}
	for (const FCue& Cue : Cues)
	{
		Count += Cue.Kind == Kind ? 1 : 0;
	}
	return Count;
}

TArray<UStaticMeshComponent*> UDFCombatCueSubsystem::GetParts(EDFCombatCue Kind) const
{
	TArray<UStaticMeshComponent*> Out;
	for (const FDFCombatCuePart& Part : Parts)
	{
		if (Part.bInUse && Part.Use == Kind && IsValid(Part.Mesh))
		{
			Out.Add(Part.Mesh);
		}
	}
	return Out;
}

int32 UDFCombatCueSubsystem::GetPartsInUse() const
{
	int32 Count = 0;
	for (const FDFCombatCuePart& Part : Parts)
	{
		Count += Part.bInUse ? 1 : 0;
	}
	return Count;
}
