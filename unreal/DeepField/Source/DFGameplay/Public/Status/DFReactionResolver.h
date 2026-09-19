#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "DFReactionResolver.generated.h"

// C5 — the closed reaction table (Statuses.cs Reactions.Match): an active status in one
// channel plus an incoming status in another names a reaction, either way round. It is a plain
// struct (C5 spells it UDFReactionResolver) so DF.Unit.Status.* can build it from literals; the
// component fills it from DT_Reactions through UDFContentSubsystem.
USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFReactionEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FDFReactionRow Row;
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFReactionResolver
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FDFReactionEntry> Reactions;

	void Reset() { Reactions.Reset(); }
	void Add(FName Id, const FDFReactionRow& Row);

	/** The reaction (Active, Incoming) or (Incoming, Active) names, or null. */
	const FDFReactionEntry* Match(FName Active, FName Incoming) const;
};
