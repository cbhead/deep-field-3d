#include "Damage/DFDamageExecution.h"

#include "AbilitySystemComponent.h"
#include "Attributes/DFCombatSet.h"
#include "Attributes/DFHealthSet.h"
#include "DFGameplayLocalTags.h"
#include "DFGameplayTags.h"
#include "Damage/DFArmorProfile.h"
#include "Damage/DFDamageContext.h"
#include "Damage/DFDamageMath.h"

namespace
{
	struct FDFDamageStatics
	{
		FGameplayEffectAttributeCaptureDefinition DamageFactorDef;
		FGameplayEffectAttributeCaptureDefinition FlatArmorDef;
		FGameplayEffectAttributeCaptureDefinition DamageTakenFactorDef;
		FGameplayEffectAttributeCaptureDefinition ShieldDef;

		FDFDamageStatics()
			// Source factor snapshots at spec creation (a buff that ends mid-flight still counts);
			// target values are read at execution so a shred landing a frame earlier is seen.
			: DamageFactorDef(UDFCombatSet::GetDamageFactorAttribute(), EGameplayEffectAttributeCaptureSource::Source, true)
			, FlatArmorDef(UDFHealthSet::GetFlatArmorAttribute(), EGameplayEffectAttributeCaptureSource::Target, false)
			, DamageTakenFactorDef(UDFHealthSet::GetDamageTakenFactorAttribute(), EGameplayEffectAttributeCaptureSource::Target, false)
			, ShieldDef(UDFHealthSet::GetShieldAttribute(), EGameplayEffectAttributeCaptureSource::Target, false)
		{
		}
	};

	const FDFDamageStatics& DamageStatics()
	{
		static FDFDamageStatics Statics;
		return Statics;
	}

	float CapturedOr(const FGameplayEffectCustomExecutionParameters& Params, const FGameplayEffectAttributeCaptureDefinition& Def, const FAggregatorEvaluateParameters& Eval, float Default)
	{
		float Value = Default;
		if (!Params.AttemptCalculateCapturedAttributeMagnitude(Def, Eval, Value))
		{
			return Default;
		}
		return Value;
	}

	/** A set-by-caller factor when the spec carries it, else the context's, else 1. */
	float FactorOr(const FGameplayEffectSpec& Spec, const FGameplayTag& Tag, float ContextValue)
	{
		if (Tag.IsValid() && Spec.SetByCallerTagMagnitudes.Contains(Tag))
		{
			return Spec.GetSetByCallerMagnitude(Tag, /*WarnIfNotFound*/ false, ContextValue);
		}
		return ContextValue;
	}
}

UDFDamageExecution::UDFDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().DamageFactorDef);
	RelevantAttributesToCapture.Add(DamageStatics().FlatArmorDef);
	RelevantAttributesToCapture.Add(DamageStatics().DamageTakenFactorDef);
	RelevantAttributesToCapture.Add(DamageStatics().ShieldDef);
}

void UDFDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

	FAggregatorEvaluateParameters Eval;
	Eval.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Eval.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	const UDFDamageContext* Context = UDFDamageContext::FromContext(Spec.GetEffectContext());

	FDFDamageInput In;
	In.BaseDamage = Spec.GetSetByCallerMagnitude(DFTags::SetByCaller_Damage, /*WarnIfNotFound*/ false, 0.f);
	In.DamageFactor = CapturedOr(ExecutionParams, DamageStatics().DamageFactorDef, Eval, 1.f);
	In.FlatArmor = CapturedOr(ExecutionParams, DamageStatics().FlatArmorDef, Eval, 0.f);
	In.DamageTakenFactor = CapturedOr(ExecutionParams, DamageStatics().DamageTakenFactorDef, Eval, 1.f);
	In.Shield = CapturedOr(ExecutionParams, DamageStatics().ShieldDef, Eval, 0.f);
	In.bTargetShredded = Eval.TargetTags && Eval.TargetTags->HasTag(DFTags::Status_Channel_Defense);

	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	FDFArmorProfile TargetProfile;
	const FDFArmorProfile* TargetProfilePtr = nullptr;
	if (const IDFArmorProfileSource* Armored = Cast<IDFArmorProfileSource>(TargetActor))
	{
		TargetProfile = Armored->GetArmorProfile();
		TargetProfilePtr = &TargetProfile;
	}
	if (Context)
	{
		Context->FillInput(In, TargetActor ? TargetActor->GetActorLocation() : FVector::ZeroVector, TargetProfilePtr);
	}
	else if (TargetProfilePtr)
	{
		// No context: the target still has an arc, but nobody said where the hit came from.
		In.FrontArmorArcDegrees = TargetProfile.FrontArmorArcDegrees;
		In.FrontArmorFactor = TargetProfile.FrontArmorFactor;
		In.RearWeakFactor = TargetProfile.RearWeakFactor;
	}

	// Spec magnitudes win over the context so an ability can override per shot.
	In.AmmoFactor = FactorOr(Spec, DFGameplayLocalTags::SetByCaller_AmmoFactor(), In.AmmoFactor);
	In.WeakPointFactor = FactorOr(Spec, DFGameplayLocalTags::SetByCaller_WeakPointFactor(), In.WeakPointFactor);
	In.PackAPunchFactor = FactorOr(Spec, DFGameplayLocalTags::SetByCaller_PackAPunchFactor(), In.PackAPunchFactor);

	const FDFDamageResult Result = FDFDamageMath::Compute(In);
	if (Result.Damage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UDFHealthSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Result.Damage));
	}

	// A periodic status effect ticks its cue from here: the id rides on the context.
	if (TargetASC && Context && Context->StatusTag.IsValid() && Spec.GetPeriod() > 0.f)
	{
		const FGameplayTag TickCue = DFGameplayLocalTags::StatusCue(DFGameplayLocalTags::ContentIdFromTag(Context->StatusTag), TEXT("Tick"));
		if (TickCue.IsValid())
		{
			FGameplayCueParameters CueParams(Spec.GetEffectContext());
			CueParams.RawMagnitude = Result.Damage;
			TargetASC->ExecuteGameplayCue(TickCue, CueParams);
		}
	}
}
