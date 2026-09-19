#include "Damage/DFDamageContext.h"

#include "Content/DFContentRows.h"
#include "Content/DFContentSubsystem.h"

UDFDamageContext* UDFDamageContext::Make(UObject* Outer, const FGameplayTag& InDamageType, const FGameplayTag& InDamageSource)
{
	UDFDamageContext* Context = NewObject<UDFDamageContext>(Outer ? Outer : GetTransientPackage());
	Context->DamageType = InDamageType;
	Context->DamageSource = InDamageSource;
	Context->ReadBalance(Outer);
	return Context;
}

const UDFDamageContext* UDFDamageContext::FromContext(const FGameplayEffectContextHandle& Handle)
{
	return Handle.IsValid() ? Cast<UDFDamageContext>(Handle.GetSourceObject()) : nullptr;
}

void UDFDamageContext::AttachTo(FGameplayEffectContextHandle& Handle) const
{
	if (Handle.IsValid())
	{
		Handle.AddSourceObject(this);
	}
}

void UDFDamageContext::SetSourceLocation(const FVector& InSourceLocation)
{
	bHasSourceLocation = true;
	SourceLocation = InSourceLocation;
}

void UDFDamageContext::SetTargetArmor(const FDFEnemyRow& Row, const FVector& TargetForward)
{
	bHasTargetArmor = true;
	TargetArmor.FrontArmorArcDegrees = Row.FrontArmorArcDegrees;
	TargetArmor.FrontArmorFactor = Row.FrontArmorFactor;
	TargetArmor.RearWeakFactor = Row.RearWeakFactor;
	TargetArmor.Forward = TargetForward.GetSafeNormal();
}

void UDFDamageContext::SetAmmo(const FDFAmmoRow& Row)
{
	AmmoFactor = Row.DamageFactor;
	UnarmoredBonusFactor = Row.UnarmoredBonusFactor;
	bIgnoresFlatArmor = Row.bIgnoresFlatArmor;
}

void UDFDamageContext::SetStatus(const FDFStatusRow& Row, const FGameplayTag& InStatusTag)
{
	StatusTag = InStatusTag;
	bIgnoresFlatArmor = Row.bIgnoresArmor;
	bIgnoresShield = Row.bIgnoresShield;
}

void UDFDamageContext::ReadBalance(const UObject* WorldContext)
{
	// Only when every table is loaded: an un-imported project is a normal state, and the defaults
	// are the sim's own numbers (ContentTypes.cs:64, Step.cs Damage()). Balance() reports a dial
	// the table lacks once per process and returns the default — the RFC asks for the three dials.
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(WorldContext);
	if (Content && Content->IsReady())
	{
		RearThresholdDegrees = Content->Balance(TEXT("rearThresholdDegrees"), RearThresholdDegrees);
		ShredFrontArcLeakFactor = Content->Balance(TEXT("shredFrontArcLeakFactor"), ShredFrontArcLeakFactor);
		PostArmorDamageFloor = Content->Balance(TEXT("postArmorDamageFloor"), PostArmorDamageFloor);
	}
}

void UDFDamageContext::FillInput(FDFDamageInput& In, const FVector& TargetLocation, const FDFArmorProfile* TargetProfile) const
{
	In.AmmoFactor = AmmoFactor;
	In.UnarmoredBonusFactor = UnarmoredBonusFactor;
	In.WeakPointFactor = WeakPointFactor;
	In.PackAPunchFactor = PackAPunchFactor;
	In.bIgnoresFlatArmor = bIgnoresFlatArmor;
	In.bIgnoresShield = bIgnoresShield;
	In.RearThresholdDegrees = RearThresholdDegrees;
	In.ShredFrontArcLeakFactor = ShredFrontArcLeakFactor;
	In.PostArmorDamageFloor = PostArmorDamageFloor;

	const FDFArmorProfile* Profile = bHasTargetArmor ? &TargetArmor : TargetProfile;
	if (Profile)
	{
		In.FrontArmorArcDegrees = Profile->FrontArmorArcDegrees;
		In.FrontArmorFactor = Profile->FrontArmorFactor;
		In.RearWeakFactor = Profile->RearWeakFactor;
		if (bHasSourceLocation)
		{
			In.bHasHitDirection = true;
			In.HitDot = FDFDamageMath::HitDot(SourceLocation, TargetLocation, Profile->Forward);
		}
	}
}
