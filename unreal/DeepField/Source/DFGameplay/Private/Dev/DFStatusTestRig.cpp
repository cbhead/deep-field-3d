#include "Dev/DFStatusTestRig.h"

#include "Abilities/DFAbilitySystemComponent.h"
#include "Attributes/DFHealthSet.h"
#include "Attributes/DFMovementSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Content/DFContentSubsystem.h"
#include "Cues/DFGameplayCueNotify_Reaction.h"
#include "DFGameplayTags.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameplayEffectTypes.h"
#include "Status/DFStatusComponent.h"
#include "Tint/DFTintComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFStatusRig, Log, All);

ADFStatusTestRig::ADFStatusTestRig()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Body->SetStaticMesh(Sphere.Object);
	}
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Body);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(18.f);
	Label->SetText(FText::FromString(TEXT("L_Test_Status")));

	AbilitySystem = CreateDefaultSubobject<UDFAbilitySystemComponent>(TEXT("AbilitySystem"));
	// Attribute sets as subobjects of the actor: the ASC registers them in InitializeComponent.
	HealthSet = CreateDefaultSubobject<UDFHealthSet>(TEXT("HealthSet"));
	MovementSet = CreateDefaultSubobject<UDFMovementSet>(TEXT("MovementSet"));
	Status = CreateDefaultSubobject<UDFStatusComponent>(TEXT("Status"));
	Tint = CreateDefaultSubobject<UDFTintComponent>(TEXT("Tint"));
}

void ADFStatusTestRig::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem->InitAbilityActorInfo(this, this);

	// Statuses.cs / Appendix A1 literals unless WS-01's tables are loaded (the component prefers an
	// override, so only add them when content is absent).
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	if (!(Content && Content->IsReady()))
	{
		FDFStatusRow Chill;
		Chill.Channel = EDFStatusChannel::Movement;
		Chill.SpeedFactor = 0.65f;
		Chill.MaxDurationSeconds = 1.5f;
		FDFStatusRow Burn;
		Burn.Channel = EDFStatusChannel::Thermal;
		Burn.DamagePerSecond = 6.f;
		Burn.MaxDurationSeconds = 3.f;
		FDFReactionRow ThermalShock;
		ThermalShock.StatusA = TEXT("chill");
		ThermalShock.StatusB = TEXT("burn");
		ThermalShock.BurstFraction = 0.12f;
		Status->AddStatusRowOverride(TEXT("chill"), Chill);
		Status->AddStatusRowOverride(TEXT("burn"), Burn);
		Status->AddReactionRowOverride(TEXT("thermalShock"), ThermalShock);
	}

	CueHandle = UDFGameplayCueNotify_Reaction::OnReactionCue().AddUObject(this, &ADFStatusTestRig::OnReactionCue);
	if (HasAuthority())
	{
		StartCycle();
	}
	else
	{
		SetState(TEXT("client: showing the replicated slots"));
	}
}

void ADFStatusTestRig::EndPlay(const EEndPlayReason::Type Reason)
{
	UDFGameplayCueNotify_Reaction::OnReactionCue().Remove(CueHandle);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChillTimer);
		World->GetTimerManager().ClearTimer(BurnTimer);
		World->GetTimerManager().ClearTimer(CycleTimer);
	}
	Super::EndPlay(Reason);
}

void ADFStatusTestRig::StartCycle()
{
	Status->ClearAll();
	HealthSet->InitMaxHealth(MaxHealth);
	HealthSet->InitHealth(MaxHealth);
	HealthSet->InitFlatArmor(FlatArmor);
	HealthSet->InitMaxShield(0.f);
	HealthSet->InitShield(0.f);
	MovementSet->InitBaseSpeed(250.f);
	MovementSet->InitSpeedFactor(1.f);
	Tint->SetHp(1.f);

	bLastCueSeen = false;
	LastCueTag = FGameplayTag();
	LastCueHandlerTag = FGameplayTag();
	LastCueBurst = 0.f;
	LastCueFraction = 0.f;
	LastReactionId = NAME_None;
	LastHealthAfterBurst = 0.f;
	SetState(FString::Printf(TEXT("cycle %d: %.0f hp; chill at +%.1f s, burn at +%.1f s"), CompletedCycles + 1, MaxHealth, ChillAtSeconds, BurnAtSeconds));

	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(ChillTimer, this, &ADFStatusTestRig::ApplyChill, ChillAtSeconds, false);
	Timers.SetTimer(BurnTimer, this, &ADFStatusTestRig::ApplyBurn, BurnAtSeconds, false);
	Timers.SetTimer(CycleTimer, this, &ADFStatusTestRig::StartCycle, CycleSeconds, false);
}

void ADFStatusTestRig::ApplyChill()
{
	const EDFStatusApplyResult Result = Status->Apply(DFTags::Status_Chill, this);
	SetState(FString::Printf(TEXT("chill: %s (SpeedFactor %.2f)"), *UEnum::GetValueAsString(Result), MovementSet->GetSpeedFactor()));
}

void ADFStatusTestRig::ApplyBurn()
{
	const EDFStatusApplyResult Result = Status->Apply(DFTags::Status_Burn, this);
	const FDFStatusApplyOutcome& Outcome = Status->GetLastOutcome();
	LastReactionId = Outcome.ReactionId;
	LastHealthAfterBurst = HealthSet->GetHealth();
	++CompletedCycles;

	// The DoD line. The cue executed synchronously inside Apply (a local multicast), so
	// bLastCueSeen already says whether GC_DF_Reaction_ThermalShock ran.
	const FString Line = FString::Printf(TEXT("chill + burn -> %s (%s) %.0f%% = %.1f of %.0f hp, health %.1f, %s"),
		*Outcome.ReactionId.ToString(), *UEnum::GetValueAsString(Result), Outcome.BurstFraction * 100.f, LastCueBurst, MaxHealth,
		LastHealthAfterBurst, bLastCueSeen ? *FString::Printf(TEXT("via %s"), *LastCueTag.ToString()) : TEXT("cue NOT seen"));
	UE_LOG(LogDFStatusRig, Display, TEXT("L_Test_Status: %s"), *Line);
	SetState(Line);
}

void ADFStatusTestRig::OnReactionCue(AActor* Target, const FGameplayTag& CueTag, const FGameplayCueParameters& Params)
{
	if (Target != this)
	{
		return;
	}
	bLastCueSeen = true;
	LastCueTag = CueTag;
	LastCueHandlerTag = Params.MatchedTagName;
	LastCueBurst = Params.RawMagnitude;
	LastCueFraction = Params.NormalizedMagnitude;
}

void ADFStatusTestRig::SetState(const FString& Text)
{
	StateText = Text;
	if (Label)
	{
		Label->SetText(FText::FromString(Text));
	}
}
