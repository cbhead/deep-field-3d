#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"
#include "Messages/DFMessageBus.h"
#include "Misc/CoreMisc.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/SubclassOf.h"
#include "UObject/Package.h"

// Header-only helpers for automation tests (PROGRAMME.md §7; WS-15 contract-append). Tests live
// in each module's Private/Tests and are compiled into that module only under
// WITH_AUTOMATION_TESTS, so the helpers they share cannot live in DFTests (UncookedOnly: a
// runtime module may not link it). They live here, in the lowest layer, and carry no automation
// framework types of their own — any module that depends on DFCore can include this file.
//
//   IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMyTest, "DF.Unit.Area.Name", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
//   bool FMyTest::RunTest(const FString&)
//   {
//       FDFTestWorld World;                                   // a Game world with a game instance and its subsystems
//       FDFMessageCapture Seen(World.MessageBus(), DFTags::Message);
//       World.MessageBus()->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure{});
//       World.Tick();                                         // one 60 Hz frame
//       return TestEqual(TEXT("one message"), Seen.Num(), 1);
//   }

/**
 * A minimal world for an automation test, torn down when the object goes out of scope.
 *
 * Game (default): a UGameInstance (any subclass) is initialised standalone — that creates the
 * FWorldContext and the world via UWorld::CreateWorld(EWorldType::Game) and initialises every
 * UGameInstanceSubsystem (UDFMessageBus, UDFContentSubsystem, ...) — then the world's actors are
 * initialised for play and BeginPlay runs, so spawned actors tick and timers fire under Tick().
 * Editor: a bare EWorldType::Editor world with its own context and no game instance, for tests of
 * editor-only code paths (validators, importers) that need a UWorld but no gameplay.
 *
 * Teardown follows the engine's own test spawner (CQTest): EndPlay on every actor, net driver down,
 * DestroyWorld, context removed, game instance shut down. Nothing is left rooted.
 */
class FDFTestWorld
{
public:
	explicit FDFTestWorld(EWorldType::Type InWorldType = EWorldType::Game, TSubclassOf<UGameInstance> GameInstanceClass = nullptr)
		: WorldType(InWorldType)
	{
		check(GEngine);
		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DFTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		if (WorldType == EWorldType::Game)
		{
			// InitializeStandalone is the engine's own "a game instance with no map yet" path: it
			// creates the context, a dummy Game world, and calls Init() (subsystems included).
			GameInstance = NewObject<UGameInstance>(GEngine, GameInstanceClass ? *GameInstanceClass : UGameInstance::StaticClass());
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone(WorldName, GetTransientPackage());
			World = GameInstance->GetWorld();
		}
		else
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(WorldType);
			World = UWorld::CreateWorld(WorldType, /*bInformEngineOfWorld*/ false, WorldName, GetTransientPackage());
			Context.SetCurrentWorld(World);
		}
		check(World);
		World->InitializeActorsForPlay(FURL());
		if (WorldType == EWorldType::Game)
		{
			World->BeginPlay();
		}
	}

	~FDFTestWorld()
	{
		if (World)
		{
			if (World->AreActorsInitialized())
			{
				for (AActor* Actor : FActorRange(World))
				{
					if (Actor)
					{
						Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
					}
				}
			}
			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(/*bInformEngineOfWorld*/ true);
			World->SetPhysicsScene(nullptr);
			GEngine->DestroyWorldContext(World);
			World = nullptr;
		}
		if (GameInstance)
		{
			GameInstance->Shutdown();
			GameInstance->RemoveFromRoot();
			GameInstance = nullptr;
		}
	}

	FDFTestWorld(const FDFTestWorld&) = delete;
	FDFTestWorld& operator=(const FDFTestWorld&) = delete;

	UWorld* GetWorld() const { return World; }
	UGameInstance* GetGameInstance() const { return GameInstance; }
	operator UWorld*() const { return World; }

	/** A game-instance subsystem of this world (null for Editor worlds, which have no game instance). */
	template <typename TSubsystem>
	TSubsystem* GetSubsystem() const
	{
		return GameInstance ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
	}

	UDFMessageBus* MessageBus() const { return GetSubsystem<UDFMessageBus>(); }

	template <typename TActor>
	TActor* SpawnActor(const FTransform& Transform = FTransform::Identity) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<TActor>(TActor::StaticClass(), Transform, Params);
	}

	/**
	 * Advance the world by Frames ticks of DeltaSeconds each (default one 60 Hz frame). Actors,
	 * components and the world's timer manager tick; GFrameCounter advances so once-per-frame
	 * guards (e.g. in movement and replication) see a new frame.
	 */
	void Tick(float DeltaSeconds = 1.f / 60.f, int32 Frames = 1) const
	{
		for (int32 i = 0; i < Frames; ++i)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, DeltaSeconds);
		}
	}

	/** Tick until Seconds of world time have elapsed (at DeltaSeconds per frame). */
	void TickFor(float Seconds, float DeltaSeconds = 1.f / 60.f) const
	{
		Tick(DeltaSeconds, FMath::Max(1, FMath::CeilToInt(Seconds / DeltaSeconds)));
	}

private:
	EWorldType::Type WorldType;
	UWorld* World = nullptr;
	UGameInstance* GameInstance = nullptr;
};

/**
 * Subscribes to a tag on the message bus and records every (tag, payload) delivered, in order,
 * until it goes out of scope. bIncludeChildren = true (the default) records every message under
 * the tag (subscribe to DFTags::Message to hear everything); false records exact matches only.
 */
class FDFMessageCapture
{
public:
	struct FRecord
	{
		FGameplayTag Tag;
		FInstancedStruct Payload;
	};

	FDFMessageCapture(UDFMessageBus* InBus, const FGameplayTag& Tag, bool bIncludeChildren = true)
		: Bus(InBus)
	{
		if (InBus)
		{
			Handle = InBus->SubscribeRaw(Tag, FDFMessageDelegate::CreateLambda(
				[this](const FGameplayTag& InTag, const FInstancedStruct& Payload)
				{
					Records.Add(FRecord{ InTag, Payload });
				}), bIncludeChildren);
		}
	}

	~FDFMessageCapture()
	{
		Stop();
	}

	FDFMessageCapture(const FDFMessageCapture&) = delete;
	FDFMessageCapture& operator=(const FDFMessageCapture&) = delete;

	/** Unsubscribe early (safe to call from inside a delivery, and more than once). */
	void Stop()
	{
		if (Handle.IsValid())
		{
			if (UDFMessageBus* B = Bus.Get())
			{
				B->Unsubscribe(Handle);
			}
			Handle = FDFMessageHandle();
		}
	}

	int32 Num() const { return Records.Num(); }
	bool IsEmpty() const { return Records.IsEmpty(); }
	const TArray<FRecord>& GetRecords() const { return Records; }
	const FRecord& Last() const { return Records.Last(); }
	const FRecord& operator[](int32 Index) const { return Records[Index]; }

	/** How many of the recorded messages carried exactly this tag. */
	int32 CountOf(const FGameplayTag& Tag) const
	{
		int32 N = 0;
		for (const FRecord& R : Records)
		{
			N += (R.Tag == Tag) ? 1 : 0;
		}
		return N;
	}

	/** The payload of record Index as T, or null if there is no such record or it is another type. */
	template <typename T>
	const T* PayloadAt(int32 Index) const
	{
		return Records.IsValidIndex(Index) ? Records[Index].Payload.GetPtr<T>() : nullptr;
	}

	template <typename T>
	const T* LastPayload() const
	{
		return PayloadAt<T>(Records.Num() - 1);
	}

	void Reset() { Records.Reset(); }

private:
	TWeakObjectPtr<UDFMessageBus> Bus;
	FDFMessageHandle Handle;
	TArray<FRecord> Records;
};
