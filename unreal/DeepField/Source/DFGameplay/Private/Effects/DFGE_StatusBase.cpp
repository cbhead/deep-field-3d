#include "Effects/DFGE_StatusBase.h"

#include "DFGameplayTags.h"
#include "Damage/DFDamageExecution.h"
#include "Effects/DFGE_Status_Control.h"
#include "Effects/DFGE_Status_Defense.h"
#include "Effects/DFGE_Status_Detection.h"
#include "Effects/DFGE_Status_Movement.h"
#include "Effects/DFGE_Status_Tether.h"
#include "Effects/DFGE_Status_Thermal.h"
#include "Effects/DFGE_Status_Toxin.h"
#include "Effects/DFGE_Status_Vulnerability.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

FGameplayTag UDFGE_StatusBase::ChannelTag(EDFStatusChannel InChannel)
{
	switch (InChannel)
	{
	case EDFStatusChannel::Movement:      return DFTags::Status_Channel_Movement;
	case EDFStatusChannel::Thermal:       return DFTags::Status_Channel_Thermal;
	case EDFStatusChannel::Toxin:         return DFTags::Status_Channel_Toxin;
	case EDFStatusChannel::Defense:       return DFTags::Status_Channel_Defense;
	case EDFStatusChannel::Vulnerability: return DFTags::Status_Channel_Vulnerability;
	case EDFStatusChannel::Control:       return DFTags::Status_Channel_Control;
	case EDFStatusChannel::Tether:        return DFTags::Status_Channel_Tether;
	case EDFStatusChannel::Detection:     return DFTags::Status_Channel_Detection;
	}
	return FGameplayTag();
}

TSubclassOf<UDFGE_StatusBase> UDFGE_StatusBase::ClassForChannel(EDFStatusChannel InChannel)
{
	switch (InChannel)
	{
	case EDFStatusChannel::Movement:      return UDFGE_Status_Movement::StaticClass();
	case EDFStatusChannel::Thermal:       return UDFGE_Status_Thermal::StaticClass();
	case EDFStatusChannel::Toxin:         return UDFGE_Status_Toxin::StaticClass();
	case EDFStatusChannel::Defense:       return UDFGE_Status_Defense::StaticClass();
	case EDFStatusChannel::Vulnerability: return UDFGE_Status_Vulnerability::StaticClass();
	case EDFStatusChannel::Control:       return UDFGE_Status_Control::StaticClass();
	case EDFStatusChannel::Tether:        return UDFGE_Status_Tether::StaticClass();
	case EDFStatusChannel::Detection:     return UDFGE_Status_Detection::StaticClass();
	}
	return UDFGE_Status_Movement::StaticClass();
}

void UDFGE_StatusBase::ConfigureChannel(EDFStatusChannel InChannel)
{
	Channel = InChannel;
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// A default subobject (not FindOrAddComponent's NewObject) so the CDO owns it the way a
	// loaded asset's component would be owned; GEComponents is protected for exactly this.
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DFChannelTags"));
	GEComponents.Add(TargetTags);
	FInheritedTagContainer Granted;
	Granted.AddTag(ChannelTag(InChannel));
	TargetTags->SetAndApplyTargetTagChanges(Granted);
}

void UDFGE_StatusBase::AddMagnitudeModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op)
{
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = DFTags::SetByCaller_Magnitude;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = Op;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);
}

void UDFGE_StatusBase::ConfigurePeriodicDamage()
{
	Period = FScalableFloat(DotPeriodSeconds);
	// The sim damaged on the first tick after application; ticking at t=0 is the nearest match.
	bExecutePeriodicEffectOnApplication = true;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UDFDamageExecution::StaticClass();
	Executions.Add(Execution);
}
