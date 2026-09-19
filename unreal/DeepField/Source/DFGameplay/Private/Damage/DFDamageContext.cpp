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
	// Only when every table is loaded: a partially imported project would log a missing-dial
	// error per hit, and the defaults are the sim's own numbers.
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(WorldContext);
	if (Content && Content->IsReady())
	{
		RearArcDegrees = Content->Balance(TEXT("rearArcDegrees"), RearArcDegrees);
		ShreddedFrontArcFactor = Content->Balance(TEXT("shreddedFrontArcFactor"), ShreddedFrontArcFactor);
		MinDamageAfterArmor = Content->Balance(TEXT("minDamageAfterArmor"), MinDamageAfterArmor);
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
	In.RearArcDegrees = RearArcDegrees;
	In.ShreddedFrontArcFactor = ShreddedFrontArcFactor;
	In.MinDamageAfterArmor = MinDamageAfterArmor;

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
