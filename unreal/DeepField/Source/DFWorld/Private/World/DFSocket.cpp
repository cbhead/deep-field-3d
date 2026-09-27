#include "World/DFSocket.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DFGameplayTags.h"
#include "DFWorldCollision.h"
#include "Engine/World.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "World/DFWorldLook.h"

ADFSocket::ADFSocket()
{
	Pad = CreateDefaultSubobject<UBoxComponent>(TEXT("Pad"));
	Pad->SetupAttachment(RootComponent);
	Pad->SetMobility(EComponentMobility::Static);
	Pad->InitBoxExtent(FVector(PadRadiusCm, PadRadiusCm, PadHalfHeightCm));
	Pad->SetRelativeLocation(FVector(0.f, 0.f, PadHalfHeightCm));
	Pad->SetCollisionProfileName(DFCollision::GrayboxProfile());
	// The profile is "behaves as world"; a pad is an interact target that must not cast a
	// sight shadow or catch the lane-surface projection.
	Pad->SetCollisionResponseToChannel(DFCollision::Interact, ECR_Block);
	Pad->SetCollisionResponseToChannel(DFCollision::Build, ECR_Block);
	Pad->SetCollisionResponseToChannel(DFCollision::Sight, ECR_Ignore);
	Pad->SetCollisionResponseToChannel(DFCollision::LaneSurface, ECR_Ignore);
	Pad->SetCollisionResponseToChannel(DFCollision::Weapon, ECR_Ignore);
	Pad->SetGenerateOverlapEvents(false);
	Pad->SetCanEverAffectNavigation(false);

	using DFWorldLook::EShape;
	// The plate fills the pad box exactly, so a tower (placed at GetPadTop) stands on it.
	Plate = DFWorldLook::CreatePart(*this, RootComponent, TEXT("Plate"), EShape::Cylinder,
		FVector(0.f, 0.f, PadHalfHeightCm), FVector(PlateRadiusCm * 2.f, PlateRadiusCm * 2.f, PadHalfHeightCm * 2.f), EComponentMobility::Static);
	FlushPlate = DFWorldLook::CreatePart(*this, RootComponent, TEXT("FlushPlate"), EShape::Cylinder,
		FVector(0.f, 0.f, FlushPlateHeightCm * 0.5f), FVector(PlateRadiusCm * 2.f, PlateRadiusCm * 2.f, FlushPlateHeightCm), EComponentMobility::Movable);
	FlushPlate->SetVisibility(false);
	FlushPlate->SetCastShadow(false);
	// The ring is a thin, wider disc under the plate: only its rim shows, as an outline on the ground
	// (over a flush plate the whole disc shows, which reads as the pad lighting up).
	const float RingDiameterCm = (PlateRadiusCm + AimRingMarginCm) * 2.f;
	AimRing = DFWorldLook::CreatePart(*this, RootComponent, TEXT("AimRing"), EShape::Cylinder,
		FVector(0.f, 0.f, AimRingHeightCm * 0.5f), FVector(RingDiameterCm, RingDiameterCm, AimRingHeightCm), EComponentMobility::Movable);
	AimRing->SetVisibility(false);
	AimRing->SetCastShadow(false);
	// The range ring hangs off the root for its position only: laid on the world's ground, not turned or
	// tilted with the pad.
	RangeRing = DFWorldLook::CreateLaidPart(*this, RootComponent, TEXT("RangeRing"));
	RangeRing->SetUsingAbsoluteRotation(true);
	RangeRing->SetUsingAbsoluteScale(true);
	RangeRing->SetVisibility(false);
	RangeRing->SetCastShadow(false);
}

void ADFSocket::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLook();
}

void ADFSocket::BeginPlay()
{
	Super::BeginPlay();
	// A level loaded into a game is not constructed again, and the tint is transient (never saved).
	RefreshLook();
	if (GetWorld() && GetWorld()->IsGameWorld() && LiesInLane(Tag) && !AlignToLaneSurface())
	{
		// The terrain may stream in after the gameplay level: try once more when it has.
		GetWorldTimerManager().SetTimer(AlignRetry, FTimerDelegate::CreateWeakLambda(this, [this]() { AlignToLaneSurface(); }), 1.f, false);
	}
}

bool ADFSocket::AlignToLaneSurface()
{
	UWorld* World = GetWorld();
	if (!World || !LiesInLane(Tag) || !FlushPlate || !AimRing)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DFPadLaneSurface), /*bTraceComplex*/ true, this);
	FHitResult Hit;
	const FVector Origin = GetActorLocation();
	if (!World->LineTraceSingleByChannel(Hit, Origin + FVector(0.f, 0.f, 300.f), Origin - FVector(0.f, 0.f, 300.f), DFCollision::LaneSurface, Params)
		|| Hit.ImpactNormal.Z < 0.5)   // steeper than 60 degrees is not a lane: leave the plate level
	{
		return false;
	}
	// Up along the surface, the pad's own facing kept as far as the slope allows.
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Hit.ImpactNormal);
	const FQuat Facing = GetActorQuat();
	const FVector Surface = Hit.ImpactPoint;
	for (UStaticMeshComponent* Part : { FlushPlate.Get(), AimRing.Get() })
	{
		const float HalfHeightCm = Part == FlushPlate ? FlushPlateHeightCm * 0.5f : AimRingHeightCm * 0.5f;
		Part->SetWorldLocationAndRotation(Surface + Hit.ImpactNormal * HalfHeightCm, Tilt * Facing);
	}
	return true;
}

FLinearColor ADFSocket::PlateColourFor(EDFSocketTag InTag)
{
	switch (InTag)
	{
	case EDFSocketTag::Trap:      return DFWorldLook::PadTrap();
	case EDFSocketTag::Barricade: return DFWorldLook::PadBarricade();
	case EDFSocketTag::Wall:      return DFWorldLook::PadWall();
	case EDFSocketTag::Ground:
	default:                      return DFWorldLook::PadGround();
	}
}

void ADFSocket::RefreshLook()
{
	const bool bFlush = LiesInLane(Tag);
	Plate->SetVisibility(!bFlush);
	FlushPlate->SetVisibility(bFlush);
	DFWorldLook::Tint(bFlush ? FlushPlate.Get() : Plate.Get(), PlateColourFor(Tag));
	const EDFPadHighlight Current = AimHighlight;
	AimHighlight = EDFPadHighlight::None;   // force the ring to be re-applied
	SetAimHighlight(Current);
	if (RangeRing && RangeTone != EDFPadHighlight::None)   // its glow instance is transient too
	{
		DFWorldLook::Glow(RangeRing.Get(), AimColourFor(RangeTone));
	}
}

FLinearColor ADFSocket::AimColourFor(EDFPadHighlight Highlight)
{
	return Highlight == EDFPadHighlight::Free ? DFWorldLook::PadAimFree()
		: Highlight == EDFPadHighlight::Occupied ? DFWorldLook::PadAimOccupied()
		: DFWorldLook::PadAimBlocked();
}

void ADFSocket::SetAimHighlight(EDFPadHighlight Highlight)
{
	if (Highlight == AimHighlight || !AimRing)
	{
		return;
	}
	AimHighlight = Highlight;
	const bool bShow = Highlight != EDFPadHighlight::None;
	if (bShow)
	{
		DFWorldLook::Glow(AimRing, AimColourFor(Highlight));
	}
	AimRing->SetVisibility(bShow);
}

int32 ADFSocket::RangeSegmentsFor(float RadiusCm)
{
	return FMath::Clamp(FMath::CeilToInt32(UE_TWO_PI * FMath::Max(RadiusCm, 0.f) / RangeSegmentSpacingCm), MinRangeSegments, MaxRangeSegments);
}

void ADFSocket::SetRangePreview(float RadiusCm, EDFPadHighlight Tone, float MinRadiusCm)
{
	if (!RangeRing)
	{
		return;
	}
	if (RadiusCm <= 0.f || Tone == EDFPadHighlight::None)
	{
		if (RangeTone != EDFPadHighlight::None)
		{
			RangeTone = EDFPadHighlight::None;
			RangeRing->SetVisibility(false);   // the dashes stay: aiming back at the pad shows them again
		}
		return;
	}
	// An inner ring only inside the outer one (a row whose dead zone swallowed its reach would fire at nothing).
	MinRadiusCm = MinRadiusCm < RadiusCm ? FMath::Max(MinRadiusCm, 0.f) : 0.f;
	// Half a centimetre is nothing to see: a reach recomputed each frame with float noise lays nothing new.
	constexpr float SameCm = 0.5f;
	if (RangeRingLays == 0 || !FMath::IsNearlyEqual(RadiusCm, RangeRingRadiusCm, SameCm) || !FMath::IsNearlyEqual(MinRadiusCm, RangeRingMinRadiusCm, SameCm))
	{
		LayRangeRing(RadiusCm, MinRadiusCm);
	}
	if (Tone != RangeTone)
	{
		DFWorldLook::Glow(RangeRing.Get(), AimColourFor(Tone));
		if (RangeTone == EDFPadHighlight::None)
		{
			RangeRing->SetVisibility(true);
		}
		RangeTone = Tone;
	}
}

bool ADFSocket::IsRangePreviewShown() const
{
	return RangeRing && RangeTone != EDFPadHighlight::None && RangeRing->IsVisible();
}

void ADFSocket::GroundUnder(const FVector& Point, float ReachCm, FVector& OutGround, FVector& OutUp) const
{
	// From as high as ground in reach could be (a pad in a pit) down past anything in reach, so ground far
	// below is found and judged out of reach rather than missed and drawn level.
	const double Top = GetPadTop().Z;
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(DFPadRangeRing), /*bTraceComplex*/ true, this);
	const UWorld* World = GetWorld();
	if (World && World->LineTraceSingleByChannel(Hit, FVector(Point.X, Point.Y, Top + ReachCm), FVector(Point.X, Point.Y, Top - RangeGroundDepthCm),
		DFCollision::LaneSurface, Params))
	{
		OutGround = Hit.ImpactPoint;
		OutUp = Hit.ImpactNormal.Z > 0.5 ? FVector(Hit.ImpactNormal) : FVector::UpVector;   // a wall face is no ground to lie along
		return;
	}
	OutGround = FVector(Point.X, Point.Y, GetActorLocation().Z);
	OutUp = FVector::UpVector;
}

bool ADFSocket::FindReachOnGround(const FVector& Out, float ReachCm, FVector& OutGround, FVector& OutUp) const
{
	const FVector Top = GetPadTop();
	// How far a body standing on the ground D cm out along Out is from the tower's position.
	auto Probe = [this, &Top, &Out, ReachCm](double D, FVector& Ground, FVector& Up)
	{
		GroundUnder(Top + Out * D, ReachCm, Ground, Up);
		return FVector::Dist(Top, Ground);
	};
	FVector Ground;
	FVector Up;
	if (Probe(ReachCm, Ground, Up) <= ReachCm + RangeReachToleranceCm)
	{
		OutGround = Ground;   // the ground out there is about level with the pad: the reach is straight across
		OutUp = Up;
		return true;
	}
	// Well above or below it: bisect between the pad (its own ground, in reach) and ReachCm across (out of
	// it), keeping the farthest ground found in reach. At a drop this ends at the edge.
	double Near = 0.0;
	double Far = ReachCm;
	bool bFound = false;
	for (int32 i = 0; i < RangeReachProbes; ++i)
	{
		const double Mid = (Near + Far) * 0.5;
		if (Probe(Mid, Ground, Up) <= ReachCm)
		{
			Near = Mid;
			OutGround = Ground;
			OutUp = Up;
			bFound = true;
		}
		else
		{
			Far = Mid;
		}
	}
	return bFound;
}

void ADFSocket::LayRangeRing(float RadiusCm, float MinRadiusCm)
{
	RangeDashes.Reset(RangeSegmentsFor(RadiusCm) + (MinRadiusCm > 0.f ? RangeSegmentsFor(MinRadiusCm) : 0));
	const FVector Top = GetPadTop();
	for (const float Radius : { RadiusCm, MinRadiusCm })
	{
		if (Radius <= 0.f)
		{
			continue;
		}
		const int32 Count = RangeSegmentsFor(Radius);
		const double Step = UE_DOUBLE_TWO_PI / Count;
		for (int32 i = 0; i < Count; ++i)
		{
			const double Angle = Step * i;
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
			FVector Ground;
			FVector Up;
			if (!FindReachOnGround(Out, Radius, Ground, Up))
			{
				continue;   // no ground this way is in reach (a pad high over a pit): nothing to promise there
			}
			// A flat bar on the ground along the circle's tangent; the dash is a share of the chord its step
			// spans where it lies, so the gaps are even however far across the reach came to.
			const double Across = FVector::Dist2D(Ground, Top);
			const double DashCm = 2.0 * Across * FMath::Sin(Step * 0.5) * RangeDashFraction;
			const FVector Along = FVector::VectorPlaneProject(FVector(-Out.Y, Out.X, 0.0), Up).GetSafeNormal();
			RangeDashes.Emplace(FRotationMatrix::MakeFromXZ(Along, Up).ToQuat(), Ground + Up * RangeRingLiftCm,
				FVector(DashCm, RangeRingWidthCm, RangeRingHeightCm) / 100.0);
		}
	}

	// One engine box per dash, in the ring's own space (its vertices are world offsets from the pad): the
	// engine's box keeps the winding and normals every basic shape has.
	static const struct FUnitBox
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		FUnitBox() { UKismetProceduralMeshLibrary::GenerateBoxMesh(FVector(50.0), Vertices, Triangles, Normals, UVs, Tangents); }
	} Box;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	Vertices.Reserve(RangeDashes.Num() * Box.Vertices.Num());
	Triangles.Reserve(RangeDashes.Num() * Box.Triangles.Num());
	Normals.Reserve(Vertices.Max());
	UVs.Reserve(Vertices.Max());
	Tangents.Reserve(Vertices.Max());
	const FTransform& Ring = RangeRing->GetComponentTransform();
	for (const FTransform& Dash : RangeDashes)
	{
		const int32 First = Vertices.Num();
		for (int32 v = 0; v < Box.Vertices.Num(); ++v)
		{
			Vertices.Add(Ring.InverseTransformPosition(Dash.TransformPosition(Box.Vertices[v])));
			Normals.Add(Ring.InverseTransformVectorNoScale(Dash.TransformVectorNoScale(Box.Normals[v])));
			UVs.Add(Box.UVs[v]);
			Tangents.Emplace(Ring.InverseTransformVectorNoScale(Dash.TransformVectorNoScale(Box.Tangents[v].TangentX)), Box.Tangents[v].bFlipTangentY);
		}
		for (const int32 Index : Box.Triangles)
		{
			Triangles.Add(First + Index);
		}
	}
	RangeRing->ClearAllMeshSections();
	if (!Vertices.IsEmpty())
	{
		RangeRing->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, /*VertexColors*/ TArray<FColor>(), Tangents, /*bCreateCollision*/ false);
	}
	RangeRingRadiusCm = RadiusCm;
	RangeRingMinRadiusCm = MinRadiusCm;
	++RangeRingLays;
}

FGameplayTag ADFSocket::GetSocketTag() const
{
	switch (Tag)
	{
	case EDFSocketTag::Wall:      return DFTags::Socket_Wall;
	case EDFSocketTag::Trap:      return DFTags::Socket_Trap;
	case EDFSocketTag::Barricade: return DFTags::Socket_Barricade;
	case EDFSocketTag::Ground:
	default:                      return DFTags::Socket_Ground;
	}
}

FVector ADFSocket::GetPadTop() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 2.f * PadHalfHeightCm);
}
