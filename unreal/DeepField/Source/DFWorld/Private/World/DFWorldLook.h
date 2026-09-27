#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AActor;
class USceneComponent;
class UStaticMeshComponent;

// Placeholder looks for the importer's world actors, until WS-33/WS-34 bring the art: the engine's
// basic shapes (1 m, pivot at the centre, BasicShapeMaterial) scaled into a frame or a pillar and
// tinted through the material's "Color" vector parameter — ADFEnemy's placeholder body, done the
// same way. A look is set dressing and nothing else: every part is NoCollision, never affects
// navigation and raises no overlaps, so no DF_Sight/DF_Weapon/DF_Build/DF_Interact/DF_LaneSurface
// trace (C16, CONTRACTS/collision-input.md), no validator or importer trace and no hero or enemy
// body ever meets it. The actor's own components (root, editor sprite) and its stable id are left
// as they were.
namespace DFWorldLook
{
	enum class EShape : uint8
	{
		Cube,
		Cylinder,
		Sphere,
	};

	/**
	 * One look part, as a default subobject of Owner (call it from Owner's constructor) attached to
	 * Parent: the shape scaled to SizeCm and centred at CentreCm in Parent's space, with the
	 * collision settings above. Mobility is the parent's, or Movable when a parent is turned at play.
	 */
	UStaticMeshComponent* CreatePart(AActor& Owner, USceneComponent* Parent, FName Name, EShape Shape,
		const FVector& CentreCm, const FVector& SizeCm, EComponentMobility::Type Mobility);

	/**
	 * Tints a part: a dynamic instance of the shape's own material with "Color" set. Idempotent (a
	 * part already on its instance is re-tinted, not given another). The instance is transient: it is
	 * made again at construction and at BeginPlay, and a level save never writes it.
	 */
	void Tint(UStaticMeshComponent* Part, const FLinearColor& Colour);

	// Colours. PROGRAMME.md Appendix C§1 fixes two of them for every map: "core = cyan #22D3EE pool,
	// spawn portal = crimson #FF2E4A" (portals and warp gates are the only crimson). palette.json has
	// no world swatches yet, so they are spelled here, once, and parsed by UDFPaletteSettings::FromHex
	// like every other palette hex; when the palette gains them (WS-31), these ask it instead.
	FLinearColor PortalFrame();   // #FF2E4A
	FLinearColor CoreEnergy();    // #22D3EE
	FLinearColor CoreCap();       // #A8F0F4, the arcane ramp's lightest step: the bright end of the cyan
	FLinearColor CoreBase();      // #242E43, obsidian: grounds the pillar without competing with it
	FLinearColor LaneStrip();     // #5A6880, steel: a path you can read, not a colour you notice
}
