#include "World/DFSocket.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DFGameplayTags.h"
#include "DFWorldCollision.h"
#include "Engine/World.h"
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
	// The range ring hangs off the root for its position only: a circle drawn level and to scale in the
	// world, not turned or tilted with the pad.
	RangeRing = DFWorldLook::CreateInstancedPart(*this, RootComponent, TEXT("RangeRing"), EShape::Cube, EComponentMobility::Movable);
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
			RangeRing->SetVisibility(false);   // the segments stay: aiming back at the pad shows them again
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

void ADFSocket::LayRangeRing(float RadiusCm, float MinRadiusCm)
{
	TArray<FTransform> Segments;
	Segments.Reserve(RangeSegmentsFor(RadiusCm) + (MinRadiusCm > 0.f ? RangeSegmentsFor(MinRadiusCm) : 0));
	for (const float Radius : { RadiusCm, MinRadiusCm })
	{
		if (Radius <= 0.f)
		{
			continue;
		}
		// Each segment is a flat bar along the circle's tangent, centred on it; the dash is a share of the
		// chord its step spans, so the gaps are even all the way round.
		const int32 Count = RangeSegmentsFor(Radius);
		const double Step = UE_DOUBLE_TWO_PI / Count;
		const double DashCm = 2.0 * Radius * FMath::Sin(Step * 0.5) * RangeDashFraction;
		const FVector Size(DashCm / 100.0, RangeRingWidthCm / 100.0, RangeRingHeightCm / 100.0);
		for (int32 i = 0; i < Count; ++i)
		{
			const double Angle = Step * i;
			const FVector At(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), RangeRingLiftCm);
			Segments.Emplace(FRotator(0.0, FMath::RadiansToDegrees(Angle) + 90.0, 0.0), At, Size);
		}
	}
	RangeRing->ClearInstances();
	RangeRing->AddInstances(Segments, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false, /*bUpdateNavigation*/ false);
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
