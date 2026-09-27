#include "Weapons/DFHeroWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Combat/DFTargetable.h"
#include "Content/DFContentSubsystem.h"
#include "DFGameplayTags.h"
#include "DFPlayerModule.h"
#include "DFWorldCollision.h"
#include "Damage/DFDamageContext.h"
#include "Effects/DFGE_Damage.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Hero/DFHeroStateComponent.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Status/DFStatusComponent.h"
#include "Weapons/DFShotFx.h"

UDFHeroWeaponComponent::UDFHeroWeaponComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);   // for its RPCs; it replicates no properties
	WeaponId = TEXT("rifle");
}

void UDFHeroWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const AActor* OwnerActor = GetOwner())
	{
		ViewCamera = OwnerActor->FindComponentByClass<UCameraComponent>();
	}
	if (!bRulesLoaded)
	{
		LoadRules();
	}
}

void UDFHeroWeaponComponent::LoadRules()
{
	const FDFWeaponRow* Row = RowOverride.IsSet() ? &RowOverride.GetValue() : nullptr;
	if (Row == nullptr)
	{
		if (const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this))
		{
			Row = Content->Weapon(WeaponId);   // logs the missing id itself
		}
	}
	if (Row == nullptr)
	{
		UE_LOG(LogDFPlayer, Warning, TEXT("%s: no weapons row '%s', so the hero has no gun"), *GetNameSafe(GetOwner()), *WeaponId.ToString());
		bRulesLoaded = false;
		Rules = FDFWeaponRules();
		State = FDFWeaponState();
		return;
	}
	Rules = DFWeapon::RulesFromRow(*Row);
	Applies = Row->Applies;
	State = DFWeapon::Loaded(Rules);
	Budget = DFWeapon::FullBudget();
	bRulesLoaded = true;
	OnAmmoChanged.Broadcast();
}

void UDFHeroWeaponComponent::SetWeaponRowOverride(const FDFWeaponRow& Row)
{
	RowOverride = Row;
	LoadRules();
}

void UDFHeroWeaponComponent::SetMuzzle(USceneComponent* InMuzzle)
{
	Muzzle = InMuzzle;
}

bool UDFHeroWeaponComponent::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn != nullptr && Pawn->IsLocallyControlled();
}

bool UDFHeroWeaponComponent::HasOwnerAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor != nullptr && OwnerActor->HasAuthority();
}

bool UDFHeroWeaponComponent::CanUseWeapon() const
{
	// No state component yet (a test, a map without WS-28's player state): standing.
	const UDFHeroStateComponent* Life = UDFHeroStateComponent::Find(GetOwner());
	return Life == nullptr || Life->IsUp();
}

bool UDFHeroWeaponComponent::GetEye(FVector& OutLocation, FRotator& OutRotation) const
{
	const AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return false;
	}
	const APawn* Pawn = Cast<APawn>(OwnerActor);
	if (Pawn == nullptr)
	{
		OwnerActor->GetActorEyesViewPoint(OutLocation, OutRotation);
		return true;
	}
	// The first-person camera, where the player sees from (crouch easing included on the owner); the
	// control rotation, which the camera follows.
	const UCameraComponent* Camera = ViewCamera.IsValid() ? ViewCamera.Get() : Pawn->FindComponentByClass<UCameraComponent>();
	OutLocation = Camera != nullptr ? Camera->GetComponentLocation() : Pawn->GetPawnViewLocation();
	OutRotation = Pawn->GetViewRotation();
	return true;
}

FVector UDFHeroWeaponComponent::GetMuzzleLocation(const FVector& Eye) const
{
	const USceneComponent* MuzzlePoint = Muzzle.Get();
	return MuzzlePoint != nullptr ? MuzzlePoint->GetComponentLocation() : Eye;
}

void UDFHeroWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bRulesLoaded)
	{
		return;
	}
	if (IsOwnerLocallyControlled())
	{
		StepOwner(DeltaTime);
		return;
	}
	if (HasOwnerAuthority())
	{
		// The host's copy of a remote owner's gun: the reload runs on the host's clock, and the owner's
		// shot allowance refills.
		if (DFWeapon::AdvanceReload(State, Rules, DeltaTime))
		{
			OnAmmoChanged.Broadcast();
		}
		DFWeapon::RefillBudget(Budget, Rules, DeltaTime);
	}
}

void UDFHeroWeaponComponent::SetTriggerHeld(bool bHeld)
{
	if (bTriggerHeld == bHeld)
	{
		return;
	}
	bTriggerHeld = bHeld;
	if (bRulesLoaded && IsOwnerLocallyControlled())
	{
		StepOwner(0.f);   // a press fires this frame, not at the next tick; a release is seen before the next press
	}
}

void UDFHeroWeaponComponent::RequestReload()
{
	if (!bRulesLoaded || !DFWeapon::BeginReload(State, Rules))
	{
		return;
	}
	NoteReloadStarted();
	if (!HasOwnerAuthority())
	{
		Server_Reload();
	}
}

void UDFHeroWeaponComponent::StepOwner(float DeltaSeconds)
{
	const FDFWeaponStep Result = DFWeapon::Step(State, Rules, DeltaSeconds, bTriggerHeld && CanUseWeapon());
	if (Result.bReloaded)
	{
		NoteReloaded();
	}
	if (Result.bReloadStarted)
	{
		NoteReloadStarted();
		if (!HasOwnerAuthority())
		{
			Server_Reload();
		}
	}
	if (Result.bFired)
	{
		FireOwnerShot();
		OnAmmoChanged.Broadcast();
	}
}

void UDFHeroWeaponComponent::FireOwnerShot()
{
	FVector Eye = FVector::ZeroVector;
	FRotator View = FRotator::ZeroRotator;
	if (!GetEye(Eye, View))
	{
		return;
	}
	const FVector Direction = View.Vector();
	const FShotTrace Trace = TraceShot(Eye, Direction, Rules.RangeCm);
	const FVector Start = GetMuzzleLocation(Eye);
	ADFShotFx::Spawn(GetWorld(), Start, Trace.End, Trace.bHit, Trace.Target.IsValid());

	if (HasOwnerAuthority())
	{
		// The host's own hero: its gun is the truth already.
		HostApplyShot(Trace, Eye);
		Multicast_Shot(Start, Trace.End, Trace.bHit, Trace.Target.IsValid());
	}
	else
	{
		Server_Fire(Eye, Direction);
	}
}

UDFHeroWeaponComponent::FShotTrace UDFHeroWeaponComponent::TraceShot(const FVector& Origin, const FVector& Direction, float RangeCm) const
{
	FShotTrace Result;
	Result.End = Origin + Direction * RangeCm;
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return Result;
	}
	// DF_Weapon (C16): the world, terrain, structures, vehicles and enemies block it. The hero's own
	// capsule ignores the channel already; the owner is ignored as well, so nothing on the hero can
	// ever stop its own shot.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DFHeroShot), /*bTraceComplex*/ false, GetOwner());
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Origin, Result.End, DFCollision::Weapon, Params))
	{
		return Result;
	}
	Result.bHit = true;
	Result.End = Hit.ImpactPoint;
	AActor* HitActor = Hit.GetActor();
	const IDFTargetable* Targetable = Cast<IDFTargetable>(HitActor);
	if (Targetable != nullptr && !Targetable->IsTargetDead() && !Targetable->IsTargetBurrowed()
		&& UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor) != nullptr)
	{
		Result.Target = HitActor;
	}
	return Result;
}

void UDFHeroWeaponComponent::HostApplyShot(const FShotTrace& Trace, const FVector& Origin)
{
	AActor* Target = Trace.Target.Get();
	AActor* Shooter = GetOwner();
	if (Target == nullptr || Shooter == nullptr || Rules.Damage <= 0.f)
	{
		return;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (ASC == nullptr)
	{
		return;
	}
	// As a tower's round lands (DFTowerDamage::ApplyDamage): UDFGE_Damage with the damage set by caller,
	// and a context saying what hit and from where. The source location is the eye the shot left, so the
	// front-arc armor sees the side the hero shot from.
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Shooter, Shooter);
	UDFDamageContext* Damage = UDFDamageContext::Make(Shooter, DFTags::Damage_Type_Kinetic, DFTags::Damage_Source_Hero);
	Damage->SetSourceLocation(Origin);
	Damage->AttachTo(Context);
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UDFGE_Damage::StaticClass(), 1.f, Context);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(DFTags::SetByCaller_Damage, Rules.Damage);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}

	// Step.cs ApplyPlayerHit: the weapon's Applies ride on the hit (the rifle's mark), after the damage.
	if (!Applies.IsEmpty())
	{
		if (UDFStatusComponent* Status = Target->FindComponentByClass<UDFStatusComponent>())
		{
			for (const FName& StatusId : Applies)
			{
				if (!StatusId.IsNone())
				{
					Status->ApplyById(StatusId, Shooter);
				}
			}
		}
	}
}

EDFShotVerdict UDFHeroWeaponComponent::HostReceiveShot(const FVector& Origin, const FVector& Direction)
{
	if (!HasOwnerAuthority() || !bRulesLoaded || Rules.RangeCm <= 0.f || Rules.Damage <= 0.f)
	{
		return EDFShotVerdict::NoWeapon;   // only the host judges shots, and only with a gun
	}
	if (!CanUseWeapon())
	{
		return EDFShotVerdict::NotUp;
	}
	const FVector ShotDirection = Direction.GetSafeNormal();
	FVector HostEye = FVector::ZeroVector;
	FRotator Unused = FRotator::ZeroRotator;
	if (ShotDirection.IsNearlyZero() || !GetEye(HostEye, Unused) || !DFWeapon::IsOriginPlausible(Origin, HostEye))
	{
		UE_LOG(LogDFPlayer, Log, TEXT("%s: refused a shot from %s, %.0f cm from where the host has the eye"),
			*GetNameSafe(GetOwner()), *Origin.ToCompactString(), FVector::Dist(Origin, HostEye));
		return EDFShotVerdict::BadShot;
	}

	const EDFShotVerdict Verdict = DFWeapon::HostAcceptShot(State, Budget, Rules);
	switch (Verdict)
	{
	case EDFShotVerdict::Accepted:
	{
		const FShotTrace Trace = TraceShot(Origin, ShotDirection, Rules.RangeCm * DFWeapon::RangeSlack);
		HostApplyShot(Trace, Origin);
		Multicast_Shot(GetMuzzleLocation(Origin), Trace.End, Trace.bHit, Trace.Target.IsValid());
		OnAmmoChanged.Broadcast();
		break;
	}
	case EDFShotVerdict::Empty:
		// Step.cs ApplyPlayerHit: a pull on an empty magazine starts the reload. The owner thought it had
		// a round, so it is told otherwise.
		DFWeapon::BeginReload(State, Rules);
		Client_CorrectAmmo(State.Rounds, State.ReloadLeft);
		OnAmmoChanged.Broadcast();
		break;
	case EDFShotVerdict::Reloading:
		Client_CorrectAmmo(State.Rounds, State.ReloadLeft);
		break;
	default:
		UE_LOG(LogDFPlayer, Verbose, TEXT("%s: refused a shot (too fast)"), *GetNameSafe(GetOwner()));
		break;
	}
	return Verdict;
}

void UDFHeroWeaponComponent::Server_Fire_Implementation(FVector_NetQuantize10 Origin, FVector_NetQuantizeNormal Direction)
{
	HostReceiveShot(Origin, Direction);
}

void UDFHeroWeaponComponent::Server_Reload_Implementation()
{
	if (bRulesLoaded && DFWeapon::BeginReload(State, Rules))
	{
		OnAmmoChanged.Broadcast();
	}
}

void UDFHeroWeaponComponent::Client_CorrectAmmo_Implementation(int32 Rounds, float ReloadLeft)
{
	const bool bWasReloading = DFWeapon::IsReloading(State);
	State.Rounds = FMath::Clamp(Rounds, 0, FMath::Max(0, Rules.MagazineSize));
	State.ReloadLeft = FMath::Max(0.f, ReloadLeft);
	if (!bWasReloading && DFWeapon::IsReloading(State))
	{
		NoteReloadStarted();
		return;
	}
	OnAmmoChanged.Broadcast();
}

void UDFHeroWeaponComponent::Multicast_Shot_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, bool bHitTarget)
{
	if (IsOwnerLocallyControlled())
	{
		return;   // the owner drew this shot the frame it fired
	}
	ADFShotFx::Spawn(GetWorld(), Start, End, bHit, bHitTarget);
}

void UDFHeroWeaponComponent::NoteReloadStarted()
{
	// The owner's own, predicted message, for its HUD and sounds; the host's copy of a remote owner's
	// reload says nothing. PlayerId stays 0 (unknown): the seat is ADFPlayerState's (DFMatch), which
	// DFPlayer cannot read, so a team-wide relay with the seat belongs there, off OnAmmoChanged.
	if (IsOwnerLocallyControlled())
	{
		if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
		{
			FDFMsg_Amount Msg;
			Msg.Amount = Rules.ReloadSeconds;   // C15: ReloadStarted's Amount is the reload's seconds
			Msg.Subject = WeaponId;
			Bus->Broadcast(DFTags::Message_ReloadStarted, Msg);
		}
	}
	OnAmmoChanged.Broadcast();
}

void UDFHeroWeaponComponent::NoteReloaded()
{
	if (IsOwnerLocallyControlled())
	{
		if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
		{
			FDFMsg_Player Msg;
			if (const AActor* OwnerActor = GetOwner())
			{
				Msg.Location = OwnerActor->GetActorLocation();
			}
			Bus->Broadcast(DFTags::Message_Reloaded, Msg);
		}
	}
	OnAmmoChanged.Broadcast();
}
