#include "Towers/DFTower.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/DFTargetable.h"
#include "Content/DFContentSubsystem.h"
#include "DFBalanceDial.h"
#include "DFGameplayTags.h"
#include "Damage/DFDamageContext.h"
#include "Effects/DFGE_Damage.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Net/UnrealNetwork.h"
#include "Status/DFStatusComponent.h"
#include "Towers/DFTargetingComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFTower)

DEFINE_LOG_CATEGORY_STATIC(LogDFTower, Log, All);

namespace
{
	/** Host-assigned structure ids: unique within the host process, carried to clients by replication. */
	int32 GNextStructureId = 1;

	IDFTargetable* AsTargetable(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<IDFTargetable>(Actor) : nullptr;
	}
}

ADFTower::ADFTower()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	SetReplicatingMovement(false);   // a tower does not move
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));   // placed on its socket's pad
	Targeting = CreateDefaultSubobject<UDFTargetingComponent>(TEXT("Targeting"));
}

void ADFTower::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADFTower, DefId);
	DOREPLIFETIME(ADFTower, SocketId);
	DOREPLIFETIME(ADFTower, StructureId);
	DOREPLIFETIME(ADFTower, OwnerSeat);
	DOREPLIFETIME(ADFTower, PathLevels);
	DOREPLIFETIME(ADFTower, Hp);
	DOREPLIFETIME(ADFTower, ActiveConditionId);
	DOREPLIFETIME(ADFTower, CurrentTarget);
}

bool ADFTower::InitializeTower(FName InDefId, FName InSocketId, int32 InOwnerSeat, int32 InSpent)
{
	DefId = InDefId;
	const FDFTowerRow* Row = GetRow();
	if (!Row)
	{
		UE_LOG(LogDFTower, Error, TEXT("%s: no tower row '%s'"), *GetName(), *InDefId.ToString());
		DefId = NAME_None;
		return false;
	}
	SocketId = InSocketId;
	OwnerSeat = InOwnerSeat;
	Spent = InSpent;
	StructureId = GNextStructureId++;
	PathLevels.Init(0, Row->UpgradePaths.Num());   // purchases per path: a fresh tower is L1 everywhere
	Hp = Row->StructureHp;
	Cooldown = 0.f;
	ForceNetUpdate();
	return true;
}

void ADFTower::SetActiveCondition(FName ConditionId)
{
	if (ActiveConditionId != ConditionId)
	{
		ActiveConditionId = ConditionId;
		ForceNetUpdate();
	}
}

void ADFTower::ApplyBuff(float Factor, float Seconds)
{
	BuffFactor = Factor;
	BuffTimer = Seconds;
}

void ADFTower::ApplyUpgrade(int32 PathIndex, int32 MoneyCost)
{
	if (PathLevels.IsValidIndex(PathIndex))
	{
		++PathLevels[PathIndex];
		Spent += MoneyCost;
		ForceNetUpdate();
	}
}

const FDFTowerRow* ADFTower::GetRow() const
{
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	return (Content && !DefId.IsNone()) ? Content->Tower(DefId) : nullptr;
}

const FDFConditionRow* ADFTower::GetActiveCondition() const
{
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	return (Content && !ActiveConditionId.IsNone()) ? Content->Condition(ActiveConditionId) : nullptr;
}

float ADFTower::GetRangeMeters() const
{
	const FDFTowerRow* Row = GetRow();
	return Row ? DFTowerMath::RangeMeters(*Row, DefId, PathLevels, GetActiveCondition()) : 0.f;
}

void ADFTower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		StepTower(DeltaSeconds);
	}
}

void ADFTower::StepTower(float DeltaSeconds)
{
	const FDFTowerRow* Row = GetRow();
	if (!Row)
	{
		return;
	}
	// Step.cs order: the weapon (FireTowers), then every round in flight (StepTowerProjectiles), so a
	// round fired this step also moves this step.
	StepWeapon(*Row, DeltaSeconds);
	StepShots(*Row, DeltaSeconds);
}

void ADFTower::StepWeapon(const FDFTowerRow& Row, float DeltaSeconds)
{
	if (BuffTimer > 0.f)
	{
		BuffTimer -= DeltaSeconds;
		if (BuffTimer <= 0.f)
		{
			BuffFactor = 1.f;
		}
	}

	switch (Row.Kind)
	{
	case EDFTowerKind::Barricade:
	case EDFTowerKind::Support:
		return;   // no weapon: a barricade works by existing; Support (Overclock) is WS-16's

	case EDFTowerKind::Tesla:
	{
		Cooldown = FMath::Max(0.f, Cooldown - DeltaSeconds);
		if (Cooldown > 0.f)
		{
			return;
		}
		AActor* First = Targeting->PickTarget(Row, GetRangeMeters());
		SetCurrentTarget(First);
		if (!First)
		{
			return;
		}
		Cooldown = 1.f / DFTowerMath::EffectiveRate(Row, PathLevels, BuffFactor);
		FireTesla(Row, First);
		return;
	}

	case EDFTowerKind::Beam:
	{
		AActor* Target = Targeting->PickTarget(Row, GetRangeMeters());
		SetCurrentTarget(Target);
		if (!Target)
		{
			// Lost the target: the charge bleeds off rather than persisting, or a beam would be free burst on the next one.
			RampTarget.Reset();
			RampSeconds = 0.f;
			return;
		}
		if (RampTarget.Get() != Target)
		{
			RampTarget = Target;
			RampSeconds = 0.f;   // switching costs the ramp
			OnFired.Broadcast(this, Target);
			Announce(DFTags::Message_BeamHeld, Target, Target->GetActorLocation());
		}
		RampSeconds += DeltaSeconds;
		const float Ramp = DFTowerMath::BeamRamp(Row, PathLevels, RampSeconds,
			DFBalance::Dial(this, TEXT("beamRampPerSecond"), 0.6f), DFBalance::Dial(this, TEXT("beamRampCap"), 3.f));
		DealDamage(Row, Target, DFTowerMath::EffectiveDamage(Row, PathLevels) * Ramp * DeltaSeconds, DFTags::Damage_Type_Thermal);
		return;
	}

	case EDFTowerKind::Aura:
	{
		// Every body on a target layer in range gets the row's statuses, every step (no sight, no stealth test: Step.cs).
		const float RangeCm = GetRangeMeters() * 100.f;
		TArray<AActor*> Bodies;
		const TArray<DFTowerMath::FDFTargetCandidate> Candidates = Targeting->GatherCandidates(Bodies);
		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			const DFTowerMath::FDFTargetCandidate& C = Candidates[i];
			if (C.bDead || !Row.TargetLayers.Contains(C.Layer) || FVector::Dist(GetActorLocation(), C.PositionCm) > RangeCm)
			{
				continue;
			}
			if (UDFStatusComponent* Status = Bodies[i]->FindComponentByClass<UDFStatusComponent>())
			{
				for (const FName& StatusId : Row.Applies)
				{
					Status->ApplyById(StatusId, this);
				}
			}
		}
		return;
	}

	default:   // Bolt, Mortar, Flak: rounds
	{
		Cooldown = FMath::Max(0.f, Cooldown - DeltaSeconds);
		if (Cooldown > 0.f)
		{
			return;
		}
		AActor* Target = Targeting->PickTarget(Row, GetRangeMeters());
		SetCurrentTarget(Target);
		if (!Target)
		{
			Targeting->NoteTarget(nullptr, nullptr);
			return;
		}
		// Night: the beat before a tower settles on something new, charged to the cooldown so it cannot stack with itself.
		const float Delay = Targeting->NoteTarget(Target, GetActiveCondition());
		if (Delay > 0.f)
		{
			Cooldown = Delay;
			return;
		}
		Cooldown = 1.f / DFTowerMath::EffectiveRate(Row, PathLevels, BuffFactor);
		FireRound(Row, Target);
		return;
	}
	}
}

void ADFTower::FireRound(const FDFTowerRow& Row, AActor* Target)
{
	FShotInFlight& Flight = Shots.AddDefaulted_GetRef();
	Flight.Target = Target;
	Flight.Shot.TargetId = AsTargetable(Target) ? AsTargetable(Target)->GetTargetId() : INDEX_NONE;
	Flight.Shot.PositionCm = GetActorLocation() + FVector(0.f, 0.f, DFTowerMath::ShotMuzzleHeightCm);
	Flight.Shot.SpeedMetersPerSecond = Row.ProjectileSpeed;
	Flight.Shot.Damage = DFTowerMath::EffectiveDamage(Row, PathLevels);
	Flight.Shot.SplashRadiusMeters = Row.SplashRadius;
	Flight.Shot.SplashFalloff = Row.SplashFalloff;
	OnFired.Broadcast(this, Target);
	Announce(DFTags::Message_TowerFired, Target, Target->GetActorLocation());
}

void ADFTower::FireTesla(const FDFTowerRow& Row, AActor* First)
{
	OnFired.Broadcast(this, First);
	Announce(DFTags::Message_TowerFired, First, First->GetActorLocation());

	TArray<AActor*> Bodies;
	const TArray<DFTowerMath::FDFTargetCandidate> Candidates = Targeting->GatherCandidates(Bodies);
	int32 Struck = Bodies.IndexOfByKey(First);
	float Damage = DFTowerMath::EffectiveDamage(Row, PathLevels);
	TSet<int32> Hit;
	if (Candidates.IsValidIndex(Struck))
	{
		Hit.Add(Candidates[Struck].Id);
	}
	DealDamage(Row, First, Damage, DFTags::Damage_Type_Shock);

	// Instant arc: hops to the nearest other body within chain range, damage falling off per hop.
	for (int32 Hop = 0; Hop < Row.ChainJumps && Struck != INDEX_NONE; ++Hop)
	{
		const int32 Next = DFTowerMath::NextChainTarget(Row, Struck, Candidates, Hit);
		if (Next == INDEX_NONE)
		{
			break;
		}
		Damage *= Row.ChainFalloff;
		Hit.Add(Candidates[Next].Id);
		DealDamage(Row, Bodies[Next], Damage, DFTags::Damage_Type_Shock);
		Struck = Next;
	}
}

void ADFTower::StepShots(const FDFTowerRow& Row, float DeltaSeconds)
{
	if (Shots.Num() == 0)
	{
		return;
	}
	const float HitRadius = DFBalance::Dial(this, TEXT("projectileHitRadius"), 0.4f);
	// Iterate a copy's worth of indices: landing a round applies damage, and a listener may fire more.
	for (int32 i = 0; i < Shots.Num();)
	{
		AActor* Target = Shots[i].Target.Get();
		IDFTargetable* Body = AsTargetable(Target);
		if (!Body || Body->IsTargetDead())
		{
			Shots.RemoveAtSwap(i);   // the sim kills a round whose target is gone; it lands nowhere
			continue;
		}
		if (!DFTowerMath::AdvanceShot(Shots[i].Shot, Body->GetAimPoint(), DeltaSeconds, HitRadius))
		{
			++i;
			continue;
		}
		const DFTowerMath::FDFTowerShot Landed = Shots[i].Shot;
		Shots.RemoveAtSwap(i);

		if (Landed.SplashRadiusMeters > 0.f)
		{
			// Splash: every live body within the radius of the struck target, any layer (Step.cs), falling off to the rim.
			const FVector Centre = Body->GetTargetPosition();
			TArray<AActor*> Bodies;
			const TArray<DFTowerMath::FDFTargetCandidate> Candidates = Targeting->GatherCandidates(Bodies);
			for (int32 j = 0; j < Candidates.Num(); ++j)
			{
				if (Candidates[j].bDead)
				{
					continue;
				}
				const float Distance = static_cast<float>(FVector::Dist(Candidates[j].PositionCm, Centre)) / 100.f;
				const float Factor = DFTowerMath::SplashFactor(Distance, Landed.SplashRadiusMeters, Landed.SplashFalloff);
				if (Distance <= Landed.SplashRadiusMeters)
				{
					DealDamage(Row, Bodies[j], Landed.Damage * Factor, DFTags::Damage_Type_Splash);
				}
			}
		}
		else
		{
			DealDamage(Row, Target, Landed.Damage, DFTags::Damage_Type_Kinetic);
		}
		OnShotLanded.Broadcast(this, Target);
		Announce(DFTags::Message_ProjectileLanded, Target, Landed.PositionCm);
	}
}

void ADFTower::DealDamage(const FDFTowerRow& Row, AActor* Target, float Amount, const FGameplayTag& DamageType)
{
	IDFTargetable* Body = AsTargetable(Target);
	if (!Body || Body->IsTargetDead())
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, /*LookForComponent*/ true); ASC && Amount > 0.f)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(this, this);
		UDFDamageContext* Damage = UDFDamageContext::Make(this, DamageType, DFTags::Damage_Source_Tower);
		Damage->SetSourceLocation(GetActorLocation());
		Damage->AttachTo(Context);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UDFGE_Damage::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(DFTags::SetByCaller_Damage, Amount);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
	DamageDealt += Amount;

	// Step.cs Damage(): statuses after the damage. The sim flags death only after its Applies loop,
	// so a lethal hit still applies them; WS-05's IsTargetDead must likewise not turn true mid-hit
	// for this to match (it reads the death the health set reports, which happens in the same frame).
	if (UDFStatusComponent* Status = Target->FindComponentByClass<UDFStatusComponent>())
	{
		for (const FName& StatusId : Row.Applies)
		{
			Status->ApplyById(StatusId, this);
		}
	}
	if (Body->IsTargetDead())
	{
		++Kills;
	}
}

void ADFTower::Announce(const FGameplayTag& Tag, AActor* Target, const FVector& Impact) const
{
	UDFMessageBus* Bus = UDFMessageBus::Get(this);
	if (!Bus)
	{
		return;
	}
	FDFMsg_Shot Message;
	Message.StructureId = StructureId;
	Message.TargetId = AsTargetable(Target) ? AsTargetable(Target)->GetTargetId() : 0;
	Message.DefId = DefId;
	Message.Origin = GetActorLocation() + FVector(0.f, 0.f, DFTowerMath::ShotMuzzleHeightCm);
	Message.Impact = Impact;
	if (const FDFTowerRow* Row = GetRow())
	{
		Message.bIndirect = Row->bIndirect;
	}
	Bus->BroadcastTeam(Tag, Message);
}

void ADFTower::SetCurrentTarget(AActor* Target)
{
	if (CurrentTarget != Target)
	{
		CurrentTarget = Target;
		ForceNetUpdate();
	}
}
