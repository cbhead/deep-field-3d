#include "Tint/DFTintComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DFGameplayLocalTags.h"
#include "DFGameplayTags.h"
#include "GameFramework/Actor.h"
#include "Tint/DFPaletteSettings.h"

UDFTintComponent::UDFTintComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Neutral defaults: full hp, unit factors, no status, energy white at the palette intensity.
	Values[DFTintLayout::HpFrac] = 1.f;
	Values[DFTintLayout::EyeGlow] = 1.f;
	Values[DFTintLayout::Phase] = 1.f;
	Values[DFTintLayout::EnergyHueR] = 1.f;
	Values[DFTintLayout::EnergyHueG] = 1.f;
	Values[DFTintLayout::EnergyHueB] = 1.f;
	Values[DFTintLayout::EnergyIntensity] = GetDefault<UDFPaletteSettings>()->EnergyEmissiveIntensity;
	Values[DFTintLayout::Powered] = 1.f;
}

void UDFTintComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!bTargetsExplicit)
	{
		CollectOwnerTargets();
	}
	Refresh();
}

void UDFTintComponent::CollectOwnerTargets()
{
	Targets.Reset();
	if (const AActor* Owner = GetOwner())
	{
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Owner->GetComponents<UPrimitiveComponent>(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			Targets.Add(Primitive);
		}
	}
}

void UDFTintComponent::SetTargets(const TArray<UPrimitiveComponent*>& InTargets)
{
	bTargetsExplicit = true;
	Targets.Reset();
	for (UPrimitiveComponent* Primitive : InTargets)
	{
		if (Primitive)
		{
			Targets.Add(Primitive);
		}
	}
	Refresh();
}

float UDFTintComponent::GetFloat(int32 Index) const
{
	return (Index >= 0 && Index < DFTintLayout::NumFloats) ? Values[Index] : 0.f;
}

FLinearColor UDFTintComponent::GetColor(int32 IndexR) const
{
	return FLinearColor(GetFloat(IndexR), GetFloat(IndexR + 1), GetFloat(IndexR + 2), 1.f);
}

void UDFTintComponent::Flush(int32 Index)
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Target : Targets)
	{
		if (UPrimitiveComponent* Primitive = Target.Get())
		{
			Primitive->SetCustomPrimitiveDataFloat(Index, Values[Index]);
		}
	}
}

void UDFTintComponent::Write(int32 Index, float Value)
{
	if (Index < 0 || Index >= DFTintLayout::NumFloats)
	{
		return;
	}
	if (Values[Index] == Value)
	{
		return;
	}
	Values[Index] = Value;
	Flush(Index);
}

void UDFTintComponent::WriteColor(int32 IndexR, const FLinearColor& Color)
{
	Write(IndexR, Color.R);
	Write(IndexR + 1, Color.G);
	Write(IndexR + 2, Color.B);
}

void UDFTintComponent::Refresh()
{
	ResolveStatus();
	for (int32 I = 0; I < DFTintLayout::NumFloats; ++I)
	{
		Flush(I);
	}
}

// ---- C8 API ----

void UDFTintComponent::SetHp(float HpFraction)
{
	Write(DFTintLayout::HpFrac, FMath::Clamp(HpFraction, 0.f, 1.f));
}

void UDFTintComponent::SetStatus(EDFStatusChannel Channel, FGameplayTag StatusId, float Magnitude)
{
	const int32 I = static_cast<int32>(Channel);
	ChannelStatus[I] = StatusId;
	ChannelMagnitude[I] = Magnitude;
	ResolveStatus();
}

void UDFTintComponent::ClearStatus(EDFStatusChannel Channel)
{
	const int32 I = static_cast<int32>(Channel);
	ChannelStatus[I] = FGameplayTag();
	ChannelMagnitude[I] = 0.f;
	ResolveStatus();
}

FGameplayTag UDFTintComponent::GetStatusTintSource() const
{
	// TintableView.cs: hard control reads over everything, then thermal, then movement.
	const EDFStatusChannel Order[] = { EDFStatusChannel::Control, EDFStatusChannel::Thermal, EDFStatusChannel::Movement };
	const UDFPaletteSettings* Palette = GetDefault<UDFPaletteSettings>();
	for (EDFStatusChannel Channel : Order)
	{
		const FGameplayTag& Tag = ChannelStatus[static_cast<int32>(Channel)];
		if (Tag.IsValid())
		{
			const FDFPaletteStatus* Swatch = Palette->Status(Tag);
			if (Swatch && Swatch->bHasTint)
			{
				return Tag;
			}
		}
	}
	return FGameplayTag();
}

void UDFTintComponent::ResolveStatus()
{
	const UDFPaletteSettings* Palette = GetDefault<UDFPaletteSettings>();

	// StatusTint / StatusWeight / StatusEmissive from the winning channel.
	const FGameplayTag Winner = GetStatusTintSource();
	if (Winner.IsValid())
	{
		const FDFPaletteStatus* Swatch = Palette->Status(Winner);
		WriteColor(DFTintLayout::StatusTintR, Swatch->Tint);
		Write(DFTintLayout::StatusWeight, FMath::Clamp(Palette->StatusTintWeight, 0.f, 0.7f));
		WriteColor(DFTintLayout::StatusEmissiveR, Swatch->bEmissive ? Swatch->Emissive : FLinearColor::Black);
	}
	else
	{
		WriteColor(DFTintLayout::StatusTintR, FLinearColor::Black);
		Write(DFTintLayout::StatusWeight, 0.f);
		WriteColor(DFTintLayout::StatusEmissiveR, FLinearColor::Black);
	}

	// Emissive-only flags: driven by the Vulnerability / Detection channels or the explicit setters.
	const bool bMarkActive = bMarked || ChannelStatus[static_cast<int32>(EDFStatusChannel::Vulnerability)].IsValid();
	const bool bRevealActive = bRevealed || ChannelStatus[static_cast<int32>(EDFStatusChannel::Detection)].IsValid();
	Write(DFTintLayout::MarkEmissive, bMarkActive ? 1.f : 0.f);
	Write(DFTintLayout::RevealEmissive, bRevealActive ? 1.f : 0.f);

	// Per-look amounts by status id, whatever channel holds them.
	auto Has = [this](const FGameplayTag& Id)
	{
		for (const FGameplayTag& Tag : ChannelStatus)
		{
			if (Tag.IsValid() && Tag == Id)
			{
				return true;
			}
		}
		return false;
	};
	Write(DFTintLayout::FreezeAmount, Has(DFTags::Status_Freeze) ? 1.f : 0.f);
	Write(DFTintLayout::TarAmount, Has(DFTags::Status_Tar) ? 1.f : 0.f);
	Write(DFTintLayout::PoisonAmount, ChannelStatus[static_cast<int32>(EDFStatusChannel::Toxin)].IsValid() ? 1.f : 0.f);
}

void UDFTintComponent::SetElite(FGameplayTag EliteTag)
{
	const UDFPaletteSettings* Palette = GetDefault<UDFPaletteSettings>();
	if (EliteTag.IsValid())
	{
		WriteColor(DFTintLayout::EliteRimColorR, Palette->EliteRim(EliteTag));
		Write(DFTintLayout::EliteRimPower, 1.f);
	}
	else
	{
		WriteColor(DFTintLayout::EliteRimColorR, FLinearColor::Black);
		Write(DFTintLayout::EliteRimPower, 0.f);
	}
}

void UDFTintComponent::SetStealth(float Stealth)     { Write(DFTintLayout::Stealth, FMath::Clamp(Stealth, 0.f, 1.f)); }
void UDFTintComponent::SetMarked(bool bInMarked)     { bMarked = bInMarked; ResolveStatus(); }
void UDFTintComponent::SetRevealed(bool bInRevealed) { bRevealed = bInRevealed; ResolveStatus(); }

void UDFTintComponent::SetEnergy(FLinearColor Hue, float Intensity)
{
	WriteColor(DFTintLayout::EnergyHueR, Hue);
	Write(DFTintLayout::EnergyIntensity, FMath::Max(Intensity, 0.f));
}

void UDFTintComponent::SetDamage(float Damage)       { Write(DFTintLayout::Damage, FMath::Clamp(Damage, 0.f, 1.f)); }
void UDFTintComponent::SetWear(float Wear)           { Write(DFTintLayout::Wear, FMath::Clamp(Wear, 0.f, 1.f)); }
void UDFTintComponent::SetPhase(int32 Phase)         { Write(DFTintLayout::Phase, static_cast<float>(FMath::Max(Phase, 1))); }
void UDFTintComponent::SetDissolve(float Dissolve)   { Write(DFTintLayout::Dissolve, FMath::Clamp(Dissolve, 0.f, 1.f)); }
void UDFTintComponent::SetEyeGlow(float EyeGlow)     { Write(DFTintLayout::EyeGlow, FMath::Max(EyeGlow, 0.f)); }
void UDFTintComponent::SetEnrage(float Enrage)       { Write(DFTintLayout::Enrage, FMath::Clamp(Enrage, 0.f, 1.f)); }
void UDFTintComponent::SetShielded(bool bShielded)   { Write(DFTintLayout::Shielded, bShielded ? 1.f : 0.f); }
void UDFTintComponent::SetPowered(bool bPowered)     { Write(DFTintLayout::Powered, bPowered ? 1.f : 0.f); }
void UDFTintComponent::SetHeatRamp(float HeatRamp)   { Write(DFTintLayout::HeatRamp, FMath::Clamp(HeatRamp, 0.f, 1.f)); }
void UDFTintComponent::SetGhostPreview(bool bGhost)  { Write(DFTintLayout::GhostPreview, bGhost ? 1.f : 0.f); }
