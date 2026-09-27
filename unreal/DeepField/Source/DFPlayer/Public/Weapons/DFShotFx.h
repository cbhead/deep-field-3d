#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DFShotFx.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * One hero shot drawn with engine basic shapes, until WS-14's VFX: a thin streak from the muzzle to
 * where the shot stopped, a flash at the muzzle, and a flash where it hit. Every piece shrinks away
 * within about a tenth of a second and the actor removes itself. Local only (never replicated): the
 * owner spawns it the frame it fires, and the host's multicast has every other machine spawn one.
 * Nothing on it collides, so it never stops a trace.
 *
 * Godot drew the same thing (`Vfx.GunShot`): a streak and an impact, "instant feel, nothing waits".
 */
UCLASS(NotBlueprintable, NotPlaceable)
class DFPLAYER_API ADFShotFx : public AActor
{
	GENERATED_BODY()

public:
	ADFShotFx();

	/** Draw a shot from Start to End. bHit: it stopped on something; bHitTarget: on something that takes damage (an enemy). */
	static ADFShotFx* Spawn(UWorld* World, const FVector& Start, const FVector& End, bool bHit, bool bHitTarget);

	virtual void Tick(float DeltaSeconds) override;

private:
	void Configure(const FVector& Start, const FVector& End, bool bHit, bool bHitTarget);
	void UpdateLook();
	static void Tint(UStaticMeshComponent* Mesh, TObjectPtr<UMaterialInstanceDynamic>& Material, const FLinearColor& Colour);

	UPROPERTY(VisibleAnywhere, Category = "DF|Shot")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "DF|Shot")
	TObjectPtr<UStaticMeshComponent> Tracer;

	UPROPERTY(VisibleAnywhere, Category = "DF|Shot")
	TObjectPtr<UStaticMeshComponent> MuzzleFlash;

	UPROPERTY(VisibleAnywhere, Category = "DF|Shot")
	TObjectPtr<UStaticMeshComponent> Impact;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TracerMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MuzzleMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ImpactMaterial;

	float Age = 0.f;
	float LengthCm = 0.f;
	bool bShowImpact = false;
	bool bImpactOnTarget = false;
};
