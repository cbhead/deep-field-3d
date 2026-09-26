#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/DFWorldActor.h"
#include "DFWorldSubsystem.generated.h"

class ADFLaneGraphInfo;
class ADFSocket;
class UDFLaneGraphAsset;

// The level's lane graph and placed actors, for gameplay. The asset is found through the
// ADFLaneGraphInfo the importer placed in L_<Map>_Gameplay; failing that, by the naming
// convention /Game/DF/Data/Defs/LaneGraphs/DA_LaneGraph_<Map> for the persistent level L_<Map>
// (the sector row carries no asset path: the level and its graph are one import).
UCLASS()
class DFWORLD_API UDFWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDFWorldSubsystem* Get(const UObject* WorldContext);

	/** The level's lane graph, or null (and one error) when the level has none. */
	UFUNCTION(BlueprintCallable, Category = "DF|World")
	UDFLaneGraphAsset* GetLaneGraph();

	UFUNCTION(BlueprintCallable, Category = "DF|World")
	ADFSocket* FindSocket(FName SocketId);

	UFUNCTION(BlueprintCallable, Category = "DF|World")
	TArray<ADFSocket*> GetSockets();

	/** Any importer-placed actor by class and stable id. */
	template <typename T>
	T* FindActorById(FName Id)
	{
		static_assert(TIsDerivedFrom<T, ADFWorldActor>::IsDerived, "FindActorById needs an ADFWorldActor subclass");
		for (TActorIterator<T> It(GetWorld()); It; ++It)
		{
			if (It->GetStableId() == Id)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Forget what was found (levels streamed in or out, the importer re-ran). */
	void Invalidate();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Resolve();
	void OnLevelChanged(ULevel* Level, UWorld* World);

	UPROPERTY(Transient)
	TObjectPtr<UDFLaneGraphAsset> LaneGraph;

	UPROPERTY(Transient)
	TObjectPtr<ADFLaneGraphInfo> Info;

	TMap<FName, TWeakObjectPtr<ADFSocket>> SocketsById;
	bool bResolved = false;
	bool bReportedMissing = false;
	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;
};
