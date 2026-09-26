#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/DFHeroMoveRules.h"
#include "DFHeroMovementComponent.generated.h"

/**
 * The hero's CMC (WS-03, B§1.1): server-authoritative with client prediction, replacing Godot's
 * PlayerSync. Walk, crouch and jump use the engine's own properties (MaxWalkSpeed,
 * MaxWalkSpeedCrouched, JumpZVelocity); sprint and ADS are added here, and GetMaxSpeed asks
 * DFHeroMove::MaxSpeed which of the four applies.
 *
 * Sprint and ADS are predicted wants, carried in each saved move's custom flags
 * (DFHeroMove::PackWants), so a client at 150 ms sprints the frame it presses the key and the server
 * replays the same move at the same speed instead of correcting it.
 *
 * The hero's life state (UDFHeroStateComponent, set by ADFHeroCharacter on host and clients) caps
 * the rest (B§1.14): a downed hero crawls at 1 m/s and cannot sprint, jump or crouch; a hero waiting
 * for the solo respawn does not move. It is server state, not a predicted want: a client learns it
 * from replication, as it learns it went down.
 *
 * Mantle, vault, slide, ladder, zipline and lift (B§1.1, B§1.13) are custom movement modes and arrive
 * in later PRs.
 */
UCLASS()
class DFPLAYER_API UDFHeroMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UDFHeroMovementComponent();

	/** Top speed while sprinting (B§1.1: 10 m/s, Player.cs SprintSpeed). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF|Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxSprintSpeed = DFHeroMove::SprintCmPerSec;

	/** Top speed while aiming down sights (B§1.1: 3.5 m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF|Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxAimSpeed = DFHeroMove::AimCmPerSec;

	/** Top speed while downed (B§1.14: crawl 1 m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF|Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxDownedSpeed = DFHeroMove::DownedCrawlCmPerSec;

	/** Called by the owning client's input. The want is predicted; see the class comment. */
	void SetWantsToSprint(bool bWants) { bWantsToSprint = bWants; }
	void SetWantsToAim(bool bWants) { bWantsToAim = bWants; }

	bool WantsToSprint() const { return bWantsToSprint; }
	bool WantsToAim() const { return bWantsToAim; }

	/** Sprinting as the rules see it: standing, wanted, and neither crouched nor aiming. */
	bool IsSprinting() const;

	/** The hero's life state, from UDFHeroStateComponent. Up unless told otherwise. */
	void SetHeroLife(EDFHeroLife InLife) { HeroLife = InLife; }
	EDFHeroLife GetHeroLife() const { return HeroLife; }

	/** The four speeds this component moves at, as DFHeroMove reads them. */
	FDFHeroMoveSpeeds GetHeroSpeeds() const;

	virtual float GetMaxSpeed() const override;
	virtual bool CanAttemptJump() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

private:
	bool bWantsToSprint = false;
	bool bWantsToAim = false;
	EDFHeroLife HeroLife = EDFHeroLife::Up;

	friend class FDFSavedMove_Hero;
};
