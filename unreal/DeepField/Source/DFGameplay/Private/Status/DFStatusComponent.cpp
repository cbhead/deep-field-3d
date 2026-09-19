#include "Status/DFStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/DFControlSet.h"
#include "Attributes/DFHealthSet.h"
#include "Content/DFContentSubsystem.h"
#include "DFGameplayLocalTags.h"
#include "DFGameplayTags.h"
#include "Damage/DFDamageContext.h"
#include "Effects/DFGE_Damage.h"
#include "Effects/DFGE_StatusBase.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Tint/DFTintComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFStatus, Log, All);

UDFStatusComponent::UDFStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);

	Slots.SetNum(FDFStatusResolver::NumChannels);
	ImmunityTags.AddTag(DFTags::Enemy_State_Phased);
	ImmunityTags.AddTag(DFTags::Enemy_BossFrame01);
}

void UDFStatusComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFStatusComponent, Slots, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFStatusComponent, Mass, Params);
}

void UDFStatusComponent::BeginPlay()
{
	Super::BeginPlay();
	Resolver.FindStatusRow = [this](FName Id) { return FindStatusRow(Id); };
	if (HasAuthority())
	{
		ReadRates();
		LoadReactions();
	}
	UpdateTintFromSlots();
}

void UDFStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		for (int32 I = 0; I < FDFStatusResolver::NumChannels; ++I)
		{
			RemoveChannelEffect(static_cast<EDFStatusChannel>(I));
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool UDFStatusComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

float UDFStatusComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

UAbilitySystemComponent* UDFStatusComponent::GetASC() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner(), /*LookForComponent*/ true);
}

UDFTintComponent* UDFStatusComponent::GetTint() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UDFTintComponent>() : nullptr;
}

void UDFStatusComponent::ReadRates()
{
	// Content first, then the control set (an actor initialised from rows), then Balance.cs defaults.
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	if (Content && Content->IsReady())
	{
		Resolver.CcResistFillPerSecond = Content->Balance(TEXT("ccResistFillPerSecond"), Resolver.CcResistFillPerSecond);
		Resolver.CcResistDecayPerSecond = Content->Balance(TEXT("ccResistDecayPerSecond"), Resolver.CcResistDecayPerSecond);
		Resolver.TetherImmuneMass = Content->Balance(TEXT("tetherImmuneMass"), Resolver.TetherImmuneMass);
		return;
	}
	if (const UAbilitySystemComponent* ASC = GetASC())
	{
		if (ASC->HasAttributeSetForAttribute(UDFControlSet::GetCcResistFillAttribute()))
		{
			const float Fill = ASC->GetNumericAttribute(UDFControlSet::GetCcResistFillAttribute());
			const float Decay = ASC->GetNumericAttribute(UDFControlSet::GetCcResistDecayAttribute());
			if (Fill > 0.f) { Resolver.CcResistFillPerSecond = Fill; }
			if (Decay > 0.f) { Resolver.CcResistDecayPerSecond = Decay; }
		}
	}
}

void UDFStatusComponent::LoadReactions()
{
	if (bReactionsLoaded)
	{
		return;
	}
	bReactionsLoaded = true;
	Resolver.Reactions.Reset();
	// Ids() is silent on a missing table (an un-imported project is a normal state); the
	// overrides win over content so a fixture can shadow a row.
	if (const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this))
	{
		for (const FName& Id : Content->Ids(TEXT("reactions")))
		{
			if (const FDFReactionRow* Row = Content->Reaction(Id))
			{
				Resolver.Reactions.Add(Id, *Row);
			}
		}
	}
	for (const TPair<FName, FDFReactionRow>& Pair : ReactionRowOverrides)
	{
		Resolver.Reactions.Add(Pair.Key, Pair.Value);
	}
}

void UDFStatusComponent::AddStatusRowOverride(FName StatusId, const FDFStatusRow& Row)
{
	StatusRowOverrides.Add(StatusId, Row);
}

void UDFStatusComponent::AddReactionRowOverride(FName ReactionId, const FDFReactionRow& Row)
{
	ReactionRowOverrides.Add(ReactionId, Row);
	if (bReactionsLoaded)
	{
		Resolver.Reactions.Add(ReactionId, Row);
	}
}

const FDFStatusRow* UDFStatusComponent::FindStatusRow(FName StatusId) const
{
	if (const FDFStatusRow* Row = StatusRowOverrides.Find(StatusId))
	{
		return Row;
	}
	// Only ask content when it is loaded: a missing table would log an error per call, and an
	// un-imported project is not a fault of the caller.
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	return (Content && Content->IsReady()) ? Content->Status(StatusId) : nullptr;
}

void UDFStatusComponent::InitFromEnemyRow(const FDFEnemyRow& Row)
{
	Mass = Row.Mass;
	MARK_PROPERTY_DIRTY_FROM_NAME(UDFStatusComponent, Mass, this);
}

FDFStatusTargetState UDFStatusComponent::ReadTargetState() const
{
	FDFStatusTargetState State;
	State.Mass = Mass;
	State.bTetherImmune = bTetherImmune;
	if (const UAbilitySystemComponent* ASC = GetASC())
	{
		if (ASC->HasAttributeSetForAttribute(UDFHealthSet::GetShieldAttribute()))
		{
			State.Shield = ASC->GetNumericAttribute(UDFHealthSet::GetShieldAttribute());
		}
		if (!State.bTetherImmune && ImmunityTags.Num() > 0)
		{
			State.bTetherImmune = ASC->HasAnyMatchingGameplayTags(ImmunityTags);
		}
	}
	return State;
}

// ---- C5 API ----

EDFStatusApplyResult UDFStatusComponent::Apply(FGameplayTag StatusTag, AActor* Source, float MagnitudeOverride)
{
	return ApplyById(DFGameplayLocalTags::ContentIdFromTag(StatusTag), Source, MagnitudeOverride, 1.f);
}

EDFStatusApplyResult UDFStatusComponent::ApplyById(FName StatusId, AActor* Source, float MagnitudeOverride, float DurationFactor)
{
	LastOutcome = FDFStatusApplyOutcome();
	LastOutcome.StatusId = StatusId;
	if (!HasAuthority())
	{
		UE_LOG(LogDFStatus, Warning, TEXT("%s: Apply(%s) called without authority; statuses are server-only"), *GetNameSafe(GetOwner()), *StatusId.ToString());
		return EDFStatusApplyResult::NoRow;
	}
	const FDFStatusRow* Row = FindStatusRow(StatusId);
	if (!Row)
	{
		UE_LOG(LogDFStatus, Warning, TEXT("%s: no status row '%s'"), *GetNameSafe(GetOwner()), *StatusId.ToString());
		return EDFStatusApplyResult::NoRow;
	}
	if (!Resolver.FindStatusRow)
	{
		Resolver.FindStatusRow = [this](FName Id) { return FindStatusRow(Id); };
	}
	LoadReactions();

	float Factor = DurationFactor;
	if (const float* ConditionFactor = ChannelDurationFactors.Find(Row->Channel))
	{
		Factor *= *ConditionFactor;
	}

	const int32 SourceId = Source ? static_cast<int32>(Source->GetUniqueID()) : 0;
	LastOutcome = Resolver.Apply(StatusId, *Row, Now(), SourceId, ReadTargetState(), MagnitudeOverride, Factor);

	switch (LastOutcome.Result)
	{
	case EDFStatusApplyResult::Applied:
		if (!LastOutcome.ReplacedStatusId.IsNone())
		{
			OnSlotLeft(LastOutcome.Channel, LastOutcome.ReplacedStatusId, /*bExpired*/ false);
		}
		OnSlotWritten(LastOutcome.Channel, Source, /*bFromReaction*/ false);
		break;

	case EDFStatusApplyResult::Refreshed:
		ChannelSources[static_cast<int32>(LastOutcome.Channel)] = Source;
		SyncSlots();
		break;

	case EDFStatusApplyResult::Reacted:
		OnSlotLeft(LastOutcome.ConsumedChannel, LastOutcome.ConsumedStatusId, /*bExpired*/ false);
		ApplyBurst(LastOutcome, Source);
		if (!LastOutcome.EmittedStatusId.IsNone())
		{
			if (!LastOutcome.EmitReplacedStatusId.IsNone())
			{
				OnSlotLeft(LastOutcome.EmittedChannel, LastOutcome.EmitReplacedStatusId, /*bExpired*/ false);
			}
			OnSlotWritten(LastOutcome.EmittedChannel, Source, /*bFromReaction*/ true);
		}
		SyncSlots();
		break;

	default:
		break;
	}
	return LastOutcome.Result;
}

void UDFStatusComponent::ClearChannel(EDFStatusChannel Channel)
{
	if (!HasAuthority() || !Resolver.IsActive(Channel))
	{
		return;
	}
	const FName Id = Resolver.Slot(Channel).StatusId;
	Resolver.ClearChannel(Channel);
	OnSlotLeft(Channel, Id, /*bExpired*/ false);
	SyncSlots();
}

void UDFStatusComponent::ClearAll()
{
	for (int32 I = 0; I < FDFStatusResolver::NumChannels; ++I)
	{
		ClearChannel(static_cast<EDFStatusChannel>(I));
	}
}

bool UDFStatusComponent::IsChannelActive(EDFStatusChannel Channel) const
{
	return Slots.IsValidIndex(static_cast<int32>(Channel)) && Slots[static_cast<int32>(Channel)].IsActive();
}

FGameplayTag UDFStatusComponent::ActiveStatus(EDFStatusChannel Channel) const
{
	return Slots.IsValidIndex(static_cast<int32>(Channel)) ? Slots[static_cast<int32>(Channel)].StatusTag : FGameplayTag();
}

float UDFStatusComponent::ActiveMagnitude(EDFStatusChannel Channel) const
{
	return Slots.IsValidIndex(static_cast<int32>(Channel)) ? Slots[static_cast<int32>(Channel)].Magnitude : 0.f;
}

float UDFStatusComponent::TimeRemaining(EDFStatusChannel Channel) const
{
	if (!IsChannelActive(Channel))
	{
		return 0.f;
	}
	return FMath::Max(0.f, Slots[static_cast<int32>(Channel)].EndTimeServer - Now());
}

bool UDFStatusComponent::IsControlled() const
{
	return IsChannelActive(EDFStatusChannel::Control);
}

// ---- server mirroring ----

void UDFStatusComponent::OnSlotWritten(EDFStatusChannel Channel, AActor* Source, bool bFromReaction)
{
	const FDFStatusSlot& Slot = Resolver.Slot(Channel);
	const FDFStatusRow* Row = FindStatusRow(Slot.StatusId);
	ChannelSources[static_cast<int32>(Channel)] = Source;
	if (Row)
	{
		ApplyChannelEffect(Channel, Slot, *Row, Source);
	}
	FireCue(Slot.StatusId, TEXT("Applied"), Slot.Magnitude, Source);
	BroadcastStatus(DFTags::Message_StatusApplied, DFTags::ForContentId(TEXT("DF.Status"), Slot.StatusId), Channel, Slot.Magnitude, Slot.EndTime - Now());
	UpdateTintFromResolver(Channel);
	OnStatusApplied.Broadcast(Slot);
	if (!bFromReaction)
	{
		SyncSlots();
	}
}

void UDFStatusComponent::OnSlotLeft(EDFStatusChannel Channel, FName StatusId, bool bExpired)
{
	RemoveChannelEffect(Channel);
	AActor* Source = ChannelSources[static_cast<int32>(Channel)].Get();
	ChannelSources[static_cast<int32>(Channel)] = nullptr;
	FireCue(StatusId, TEXT("Removed"), 0.f, Source);
	if (bExpired)
	{
		BroadcastStatus(DFTags::Message_StatusExpired, DFTags::ForContentId(TEXT("DF.Status"), StatusId), Channel, 0.f, 0.f);
	}
	UpdateTintFromResolver(Channel);
	FDFStatusSlot Gone;
	Gone.StatusId = StatusId;
	Gone.Channel = Channel;
	OnStatusRemoved.Broadcast(Gone);
}

void UDFStatusComponent::ApplyChannelEffect(EDFStatusChannel Channel, const FDFStatusSlot& Slot, const FDFStatusRow& Row, AActor* Source)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}
	RemoveChannelEffect(Channel);

	const TSubclassOf<UDFGE_StatusBase> EffectClass = UDFGE_StatusBase::ClassForChannel(Channel);
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Source, Source);

	const FGameplayTag StatusTag = DFTags::ForContentId(TEXT("DF.Status"), Slot.StatusId);

	// The slot's magnitude is the number strongest-wins compared; the effect gets it back in
	// the attribute's own terms so a MagnitudeOverride (Swift: Movement x0.5) really halves the slow.
	float EffectMagnitude = Slot.Magnitude;
	switch (Channel)
	{
	case EDFStatusChannel::Movement:      EffectMagnitude = FMath::Clamp(1.f - Slot.Magnitude, 0.f, 1.f); break;   // SpeedFactor
	case EDFStatusChannel::Defense:       EffectMagnitude = -Slot.Magnitude; break;                                // ArmorDelta
	case EDFStatusChannel::Vulnerability: EffectMagnitude = 1.f + Slot.Magnitude; break;                           // DamageTakenFactor
	case EDFStatusChannel::Thermal:
	case EDFStatusChannel::Toxin:
	{
		UDFDamageContext* Damage = UDFDamageContext::Make(this,
			Channel == EDFStatusChannel::Thermal ? DFTags::Damage_Type_Thermal : DFTags::Damage_Type_Toxin, FGameplayTag());
		Damage->SetStatus(Row, StatusTag);
		Damage->AttachTo(Context);
		break;
	}
	default:
		break;
	}

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(DFTags::SetByCaller_Magnitude, EffectMagnitude);
	if (Channel == EDFStatusChannel::Thermal || Channel == EDFStatusChannel::Toxin)
	{
		// dps x tick period: the execution reads DF.SetByCaller.Damage as the base of each tick.
		Spec.Data->SetSetByCallerMagnitude(DFTags::SetByCaller_Damage, Slot.Magnitude * UDFGE_StatusBase::DotPeriodSeconds);
	}
	if (StatusTag.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AddTag(StatusTag);
	}
	ChannelEffects[static_cast<int32>(Channel)] = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}

void UDFStatusComponent::RemoveChannelEffect(EDFStatusChannel Channel)
{
	FActiveGameplayEffectHandle& Handle = ChannelEffects[static_cast<int32>(Channel)];
	if (Handle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetASC())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
		Handle.Invalidate();
	}
}

void UDFStatusComponent::ApplyBurst(const FDFStatusApplyOutcome& Outcome, AActor* Source)
{
	UAbilitySystemComponent* ASC = GetASC();
	float Burst = 0.f;
	if (ASC && Outcome.BurstFraction > 0.f && ASC->HasAttributeSetForAttribute(UDFHealthSet::GetMaxHealthAttribute()))
	{
		Burst = ASC->GetNumericAttribute(UDFHealthSet::GetMaxHealthAttribute()) * Outcome.BurstFraction;
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(Source, Source);
		// A burst has no aspect (it happens on the target) and goes through armor and shield
		// like any other hit, as Step.cs Damage() did for reactions.
		UDFDamageContext* Damage = UDFDamageContext::Make(this, FGameplayTag(), DFTags::Damage_Source_Reaction);
		Damage->ReactionTag = DFTags::ForContentId(TEXT("DF.Reaction"), Outcome.ReactionId);
		Damage->AttachTo(Context);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UDFGE_Damage::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(DFTags::SetByCaller_Damage, Burst);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}

	if (ASC)
	{
		const FGameplayTag Cue = DFGameplayLocalTags::ReactionCue(Outcome.ReactionId);
		if (Cue.IsValid())
		{
			FGameplayCueParameters Params;
			Params.Instigator = Source;
			Params.RawMagnitude = Burst;
			Params.NormalizedMagnitude = Outcome.BurstFraction;
			ASC->ExecuteGameplayCue(Cue, Params);
		}
	}
	BroadcastStatus(DFTags::Message_ReactionTriggered, DFTags::ForContentId(TEXT("DF.Reaction"), Outcome.ReactionId), Outcome.ConsumedChannel, Burst, 0.f);
	OnReactionTriggered.Broadcast(Outcome.ReactionId, Burst, Outcome);
}

void UDFStatusComponent::FireCue(FName StatusId, const TCHAR* Verb, float Magnitude, AActor* Source)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}
	const FGameplayTag Cue = DFGameplayLocalTags::StatusCue(StatusId, Verb);
	if (!Cue.IsValid())
	{
		return;
	}
	FGameplayCueParameters Params;
	Params.Instigator = Source;
	Params.RawMagnitude = Magnitude;
	ASC->ExecuteGameplayCue(Cue, Params);
}

void UDFStatusComponent::BroadcastStatus(const FGameplayTag& MessageTag, const FGameplayTag& StatusTag, EDFStatusChannel Channel, float Magnitude, float Duration)
{
	UDFMessageBus* Bus = UDFMessageBus::Get(this);
	if (!Bus)
	{
		return;
	}
	FDFMsg_Status Msg;
	Msg.TargetId = TargetId;
	Msg.Status = StatusTag;
	Msg.Channel = UDFGE_StatusBase::ChannelTag(Channel);
	Msg.Magnitude = Magnitude;
	Msg.Duration = Duration;
	Bus->Broadcast(MessageTag, Msg);
}

void UDFStatusComponent::UpdateTintFromResolver(EDFStatusChannel Channel)
{
	UDFTintComponent* Tint = GetTint();
	if (!Tint)
	{
		return;
	}
	const FDFStatusSlot& Slot = Resolver.Slot(Channel);
	if (Slot.IsActive())
	{
		Tint->SetStatus(Channel, DFTags::ForContentId(TEXT("DF.Status"), Slot.StatusId), Slot.Magnitude);
	}
	else
	{
		Tint->ClearStatus(Channel);
	}
}

void UDFStatusComponent::UpdateTintFromSlots()
{
	UDFTintComponent* Tint = GetTint();
	if (!Tint)
	{
		return;
	}
	for (int32 I = 0; I < Slots.Num() && I < FDFStatusResolver::NumChannels; ++I)
	{
		const EDFStatusChannel Channel = static_cast<EDFStatusChannel>(I);
		if (Slots[I].IsActive())
		{
			Tint->SetStatus(Channel, Slots[I].StatusTag, Slots[I].Magnitude);
		}
		else
		{
			Tint->ClearStatus(Channel);
		}
	}
}

void UDFStatusComponent::SyncSlots()
{
	bool bChanged = false;
	for (int32 I = 0; I < FDFStatusResolver::NumChannels; ++I)
	{
		const FDFStatusSlot& S = Resolver.Slots[I];
		FDFStatusSlotRep Rep;
		if (S.IsActive())
		{
			Rep.StatusTag = DFTags::ForContentId(TEXT("DF.Status"), S.StatusId);
			Rep.EndTimeServer = S.EndTime;
			Rep.Magnitude = S.Magnitude;
		}
		if (!(Slots[I] == Rep))
		{
			Slots[I] = Rep;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UDFStatusComponent, Slots, this);
		OnSlotsChanged.Broadcast();
	}
}

void UDFStatusComponent::OnRep_Slots()
{
	UpdateTintFromSlots();
	OnSlotsChanged.Broadcast();
}

void UDFStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!HasAuthority())
	{
		return;
	}

	TArray<FDFStatusSlot> Expired;
	Resolver.Tick(Now(), DeltaTime, &Expired);
	for (const FDFStatusSlot& Gone : Expired)
	{
		OnSlotLeft(Gone.Channel, Gone.StatusId, /*bExpired*/ true);
	}
	if (Expired.Num() > 0)
	{
		SyncSlots();
	}

	if (UAbilitySystemComponent* ASC = GetASC())
	{
		if (ASC->HasAttributeSetForAttribute(UDFControlSet::GetCcResistAttribute()))
		{
			ASC->SetNumericAttributeBase(UDFControlSet::GetCcResistAttribute(), Resolver.CcResist);
		}
	}
}
