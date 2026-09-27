#include "World/DFCore.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "World/DFWorldLook.h"

namespace DFCoreLook
{
	// A pillar on a plinth with an orb on top, 5.85 m in all: tall enough to read over the sockets'
	// towers from across Testlane, thin enough (0.9 m) that a leak is seen to walk into it.
	constexpr float PlinthDiameterCm = 240.f;
	constexpr float PlinthHeightCm = 30.f;
	constexpr float ShaftDiameterCm = 90.f;
	constexpr float ShaftHeightCm = 440.f;
	constexpr float CapDiameterCm = 130.f;
	constexpr float CapSinkCm = 15.f;   // the orb sits into the shaft's top rather than balancing on it
	constexpr float ShaftTopCm = PlinthHeightCm + ShaftHeightCm;
	constexpr float CapCentreCm = ShaftTopCm + CapDiameterCm * 0.5f - CapSinkCm;
	constexpr float TopCm = CapCentreCm + CapDiameterCm * 0.5f;   // 5.85 m

	// The light hangs just above the orb: from inside it, it would light no outward face of it.
	// Unshadowed and short-ranged, it is the cyan pool at the core's foot and the glow on its top.
	constexpr float GlowAboveTopCm = 40.f;
	constexpr float GlowCandelas = 150.f;
	constexpr float GlowRadiusCm = 1500.f;
}

ADFCore::ADFCore()
{
	using namespace DFCoreLook;
	using DFWorldLook::EShape;

	Plinth = DFWorldLook::CreatePart(*this, RootComponent, TEXT("Plinth"), EShape::Cylinder,
		FVector(0.f, 0.f, PlinthHeightCm * 0.5f), FVector(PlinthDiameterCm, PlinthDiameterCm, PlinthHeightCm), EComponentMobility::Static);
	Shaft = DFWorldLook::CreatePart(*this, RootComponent, TEXT("Shaft"), EShape::Cylinder,
		FVector(0.f, 0.f, PlinthHeightCm + ShaftHeightCm * 0.5f), FVector(ShaftDiameterCm, ShaftDiameterCm, ShaftHeightCm), EComponentMobility::Static);
	Cap = DFWorldLook::CreatePart(*this, RootComponent, TEXT("Cap"), EShape::Sphere,
		FVector(0.f, 0.f, CapCentreCm), FVector(CapDiameterCm), EComponentMobility::Static);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetMobility(EComponentMobility::Movable);   // no static lighting in this project (r.AllowStaticLighting=False)
	Glow->SetRelativeLocation(FVector(0.f, 0.f, TopCm + GlowAboveTopCm));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(GlowCandelas);
	Glow->SetAttenuationRadius(GlowRadiusCm);
	Glow->SetCastShadows(false);
	Glow->SetLightColor(DFWorldLook::CoreEnergy());
}

void ADFCore::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLook();
}

void ADFCore::BeginPlay()
{
	Super::BeginPlay();
	// A level loaded into a game is not constructed again, and the tint is transient (never saved).
	RefreshLook();
}

void ADFCore::RefreshLook()
{
	DFWorldLook::Tint(Plinth, DFWorldLook::CoreBase());
	DFWorldLook::Tint(Shaft, DFWorldLook::CoreEnergy());
	DFWorldLook::Tint(Cap, DFWorldLook::CoreCap());
}
