#pragma once

#include "Components/ActorComponent.h"
#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tint/DFTintLayout.h"
#include "DFTintComponent.generated.h"

class UPrimitiveComponent;

// C8 — the one writer of material state. Runs on every machine from replicated state (the
// status component feeds it on both sides), resolves precedence exactly as TintableView.cs
// did — Control > Thermal > Movement for StatusTint; mark and reveal are emissive-only so they
// coexist with a tint — reads colours from UDFPaletteSettings and writes Custom Primitive
// Data in the DFTintLayout order to every primitive it targets (all of the owner's by default).
// It keeps a mirror of what it wrote (GetFloat) so DF.Unit.Tint.* can check the contract with
// no mesh in the world.
UCLASS(ClassGroup = (DF), meta = (BlueprintSpawnableComponent))
class DFGAMEPLAY_API UDFTintComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDFTintComponent();

	// ---- C8 API ----
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetHp(float HpFraction);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetStatus(EDFStatusChannel Channel, FGameplayTag StatusId, float Magnitude);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void ClearStatus(EDFStatusChannel Channel);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetElite(FGameplayTag EliteTag);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetStealth(float Stealth);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetMarked(bool bMarked);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetRevealed(bool bRevealed);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetEnergy(FLinearColor Hue, float Intensity);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetDamage(float Damage);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetWear(float Wear);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetPhase(int32 Phase);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetDissolve(float Dissolve);

	// ---- documented extras (the remaining C8 enemy / tower parameters) ----
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetEyeGlow(float EyeGlow);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetEnrage(float Enrage);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetShielded(bool bShielded);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetPowered(bool bPowered);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetHeatRamp(float HeatRamp);
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetGhostPreview(bool bGhost);

	/** Restrict writes to these primitives (default: every primitive on the owner at BeginPlay). */
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void SetTargets(const TArray<UPrimitiveComponent*>& InTargets);

	/** The value last written at a DFTintLayout index. */
	UFUNCTION(BlueprintPure, Category = "DF|Tint") float GetFloat(int32 Index) const;
	FLinearColor GetColor(int32 IndexR) const;

	/** The status currently winning StatusTint (Control > Thermal > Movement), or an invalid tag. */
	UFUNCTION(BlueprintPure, Category = "DF|Tint") FGameplayTag GetStatusTintSource() const;

	/** Re-resolves every derived value and rewrites all targets (after SetTargets, or a palette change). */
	UFUNCTION(BlueprintCallable, Category = "DF|Tint") void Refresh();

protected:
	virtual void BeginPlay() override;

private:
	void Write(int32 Index, float Value);
	void WriteColor(int32 IndexR, const FLinearColor& Color);
	void Flush(int32 Index);
	void ResolveStatus();
	void CollectOwnerTargets();

	/** Per channel: the active status tag and magnitude (invalid = clear). */
	FGameplayTag ChannelStatus[8];
	float ChannelMagnitude[8] = {};

	bool bMarked = false;
	bool bRevealed = false;

	float Values[DFTintLayout::NumFloats] = {};

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Targets;
	bool bTargetsExplicit = false;
};
