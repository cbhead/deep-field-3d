#pragma once

#include "Components/ActorComponent.h"
#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Weapons/DFWeaponRules.h"
#include "DFHeroWeaponComponent.generated.h"

class UCameraComponent;
class USceneComponent;

/**
 * The hero's gun (WS-03, B§1.2; PROGRAMME.md §3.2 rule 2 "shots, not hits"): the first cut of the P2
 * slice's hitscan rifle and reload. The gun is a content row (weapons.json, read through
 * UDFContentSubsystem): damage, shots per second, range, magazine, reload seconds, automatic. The
 * rules are DFWeapon (pure, tested); this component runs them on each machine.
 *
 * Prediction and authority:
 * - The owning player's machine runs the trigger, the cooldown, the magazine and the reload itself
 *   (DFWeapon::Step), so a shot leaves, its tracer is drawn and the ammo count moves the frame the
 *   button goes down (C4 LocalPredicted, G2 "fire feels instant"). A shot is a local trace on
 *   DF_Weapon from the first-person camera along the view, to the row's range.
 * - A client never applies damage. It sends the shot (origin and direction, not a hit) to the host
 *   (Server_Fire), and a reload it starts (Server_Reload). The host keeps its own copy of the gun's
 *   state and checks each shot: the hero standing, the origin within 2 m of the host's copy of the
 *   eye, the rate (FDFShotBudget), the magazine and the reload (with a 0.2 s grace at its end). A shot
 *   that passes is traced again on the host, to the range × 1.15, and damages what it hits the way a
 *   tower's round does (UDFGE_Damage, a UDFDamageContext with DF.Damage.Type.Kinetic and
 *   DF.Damage.Source.Hero, the hero as instigator), then applies the row's statuses (the rifle's mark).
 * - The host is the truth for the magazine. When it refuses a shot because its magazine is empty or it
 *   is still reloading, it sends the owner its numbers (Client_CorrectAmmo). It never sends them
 *   otherwise: the owner's prediction and the host agree unless a shot was refused.
 * - The host's own hero (a listen server) is the authority and fires directly, with no RPC and no check.
 * - Every other machine draws the shot from the host's multicast (unreliable: a lost tracer is only a
 *   tracer).
 *
 * A component rather than GAS abilities, for this first cut: C4's plan is LocalPredicted fire and
 * reload abilities spending a predicted UDFAmmoSet.Magazine, which needs the ability set, input tags
 * (DA_InputConfig) and predicted-effect plumbing nothing in the project has yet. The state machine is
 * DFWeapon either way; moving it behind GA_Player_Fire / GA_Player_Reload later changes who calls it,
 * not what it does.
 *
 * Not yet: spread, recoil and bloom (the row's defaults are 0, the Godot feel), ≤200 ms rewind of
 * enemy positions, weapon switching, the gunsmith's factors, hit zones.
 */
UCLASS(ClassGroup = (DF), meta = (BlueprintSpawnableComponent))
class DFPLAYER_API UDFHeroWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDFHeroWeaponComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Content id of the gun in hand (weapons.json). */
	FName GetWeaponId() const { return WeaponId; }
	const FDFWeaponRules& GetRules() const { return Rules; }
	/** This machine's view of the gun: predicted on the owner, the truth on the host. */
	const FDFWeaponState& GetState() const { return State; }
	/** The row's numbers are in (from content or an override). */
	bool HasWeapon() const { return bRulesLoaded; }

	// ---- the HUD's readout (WS-12). On the owner these are its own predicted numbers. ----------
	UFUNCTION(BlueprintPure, Category = "DF|Weapon")
	int32 GetAmmoInMagazine() const { return State.Rounds; }

	UFUNCTION(BlueprintPure, Category = "DF|Weapon")
	int32 GetMagazineSize() const { return bRulesLoaded ? Rules.MagazineSize : 0; }

	UFUNCTION(BlueprintPure, Category = "DF|Weapon")
	bool IsReloading() const { return DFWeapon::IsReloading(State); }

	/** How far through the reload, 0..1 (0 when not reloading). */
	UFUNCTION(BlueprintPure, Category = "DF|Weapon")
	float GetReloadFraction() const { return DFWeapon::ReloadFraction(State, Rules); }

	/** This machine's numbers changed: a shot, a reload starting or ending, a correction from the host. */
	FSimpleMulticastDelegate OnAmmoChanged;

	// ---- the owner's input ----------------------------------------------------------------------
	/** IA_Fire down / up. A press fires at once when the gun is ready. */
	void SetTriggerHeld(bool bHeld);
	/** IA_Reload: start a reload if one is worth starting (not reloading, magazine not full). */
	void RequestReload();

	/** Where the owner's tracer starts (the placeholder gun's muzzle); the eye when unset. */
	void SetMuzzle(USceneComponent* InMuzzle);

	/** Use this row instead of the content's, and load a full magazine (tests: the Appendix A1 literals). */
	void SetWeaponRowOverride(const FDFWeaponRow& Row);

	/**
	 * Host: judge a shot the owner reported, and if it stands trace it and apply its damage. Server_Fire
	 * calls it; public so the tests can call it in a world with no connection. Accepted spends a round.
	 */
	EDFShotVerdict HostReceiveShot(const FVector& Origin, const FVector& Direction);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION(Server, Reliable)
	void Server_Fire(FVector_NetQuantize10 Origin, FVector_NetQuantizeNormal Direction);

	UFUNCTION(Server, Reliable)
	void Server_Reload();

	UFUNCTION(Client, Reliable)
	void Client_CorrectAmmo(int32 Rounds, float ReloadLeft);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Shot(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, bool bHitTarget);

	struct FShotTrace
	{
		FVector End = FVector::ZeroVector;
		bool bHit = false;
		/** What the shot hit, if it takes damage (an IDFTargetable that is alive, with an ability system). */
		TWeakObjectPtr<AActor> Target;
	};

	/** The row's numbers, from the override or the content (weapons.json). */
	void LoadRules();
	/** Owner: one step of the gun (DFWeapon::Step) and whatever it did. */
	void StepOwner(float DeltaSeconds);
	/** Owner: a round left the barrel: trace, draw, and send it (or apply it, on the host). */
	void FireOwnerShot();
	/** The first-person camera's location and the view's direction; false with no owner. */
	bool GetEye(FVector& OutLocation, FRotator& OutRotation) const;
	/** Where the tracer starts: the muzzle, or the eye. */
	FVector GetMuzzleLocation(const FVector& Eye) const;
	/** Standing (not down, not waiting to respawn): the rifle is not a downed hero's gun. */
	bool CanUseWeapon() const;
	bool IsOwnerLocallyControlled() const;
	bool HasOwnerAuthority() const;

	FShotTrace TraceShot(const FVector& Origin, const FVector& Direction, float RangeCm) const;
	/** Host: the shot's damage and statuses on what it hit. */
	void HostApplyShot(const FShotTrace& Trace, const FVector& Origin);

	/** Owner: DF.Message.ReloadStarted / Reloaded on this machine's bus, and OnAmmoChanged. */
	void NoteReloadStarted();
	void NoteReloaded();

	/** Content id of the gun in hand; the rifle until the gunsmith (WS-06) hands out others. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Weapon")
	FName WeaponId;

	FDFWeaponRules Rules;
	FDFWeaponState State;
	/** Host, for a remote owner: its shot-rate allowance. */
	FDFShotBudget Budget;
	/** Status ids a hit applies (the row's Applies). */
	TArray<FName> Applies;
	TOptional<FDFWeaponRow> RowOverride;
	bool bRulesLoaded = false;
	bool bTriggerHeld = false;

	TWeakObjectPtr<USceneComponent> Muzzle;
	TWeakObjectPtr<UCameraComponent> ViewCamera;
};
