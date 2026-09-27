#include "Economy/DFEconomyStateComponent.h"

#include "DFBalanceDial.h"
#include "DFGameplayTags.h"
#include "GameFramework/Actor.h"
#include "Messages/DFMessages.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFEconomyStateComponent)

DEFINE_LOG_CATEGORY_STATIC(LogDFEconomy, Log, All);

UDFEconomyStateComponent::UDFEconomyStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDFEconomyStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDFEconomyStateComponent, Money);
	DOREPLIFETIME(UDFEconomyStateComponent, Lives);
}

void UDFEconomyStateComponent::BeginPlay()
{
	Super::BeginPlay();
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	ResetFromBalance();
	// Clients hear the relay's copies of these too; only the host keeps the books.
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		LeakedHandle = Bus->Subscribe<FDFMsg_Enemy>(DFTags::Message_EnemyLeaked,
			[this](const FGameplayTag&, const FDFMsg_Enemy& Msg) { HandleLeaked(Msg); });
		KilledHandle = Bus->Subscribe<FDFMsg_Kill>(DFTags::Message_EnemyKilled,
			[this](const FGameplayTag&, const FDFMsg_Kill& Msg) { HandleKilled(Msg); });
	}
}

void UDFEconomyStateComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		Bus->Unsubscribe(LeakedHandle);
		Bus->Unsubscribe(KilledHandle);
	}
	LeakedHandle = FDFMessageHandle();
	KilledHandle = FDFMessageHandle();
	Super::EndPlay(Reason);
}

void UDFEconomyStateComponent::ResetFromBalance()
{
	Reset(FMath::RoundToInt(DFBalance::Dial(this, TEXT("startingMoney"), DefaultStartingMoney)),
		FMath::RoundToInt(DFBalance::Dial(this, TEXT("startingLives"), DefaultStartingLives)));
}

void UDFEconomyStateComponent::Reset(int32 InMoney, int32 InLives)
{
	Money = FMath::Max(0, InMoney);
	Lives = FMath::Max(0, InLives);
	TeamScrap.Reset();
	Changed();
}

bool UDFEconomyStateComponent::TrySpend(int32 InMoney, const FDFScrapBundle* Scrap)
{
	if (InMoney < 0 || Money < InMoney)
	{
		return false;
	}
	if (Scrap)
	{
		for (const TPair<EDFScrapType, int32>& Line : Scrap->Amounts)
		{
			if (TeamScrap.FindRef(Line.Key) < Line.Value)
			{
				return false;
			}
		}
		for (const TPair<EDFScrapType, int32>& Line : Scrap->Amounts)
		{
			TeamScrap.FindOrAdd(Line.Key) -= Line.Value;
		}
	}
	Money -= InMoney;
	Changed();
	return true;
}

void UDFEconomyStateComponent::AddMoney(int32 InMoney)
{
	if (InMoney == 0)
	{
		return;
	}
	Money = FMath::Max(0, Money + InMoney);
	Changed();
}

void UDFEconomyStateComponent::TakeLives(int32 Amount)
{
	if (Amount <= 0 || Lives <= 0)
	{
		return;
	}
	Lives = FMath::Max(0, Lives - Amount);
	Changed();
}

void UDFEconomyStateComponent::HandleLeaked(const FDFMsg_Enemy& Msg)
{
	const int32 LeakDamage = FMath::Max(0, Msg.Phase);   // FDFMsg_Enemy.Phase: "leak lives" (C15)
	TakeLives(LeakDamage);
	UE_LOG(LogDFEconomy, Log, TEXT("%s leaked: -%d lives, %d left"), *Msg.DefId.ToString(), LeakDamage, Lives);
}

void UDFEconomyStateComponent::HandleKilled(const FDFMsg_Kill& Msg)
{
	AddMoney(FMath::Max(0, Msg.Bounty));
}

void UDFEconomyStateComponent::OnRep_Economy()
{
	OnEconomyChanged.Broadcast(this);
}

void UDFEconomyStateComponent::Changed()
{
	OnEconomyChanged.Broadcast(this);
}
