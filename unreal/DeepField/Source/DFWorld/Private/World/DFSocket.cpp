#include "World/DFSocket.h"

#include "Components/BoxComponent.h"
#include "DFGameplayTags.h"
#include "DFWorldCollision.h"

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
