#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "IDFProgressionProvider.generated.h"

struct FDFMatchRecord;
class UDFProfileSave;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDFProgressionProvider : public UInterface
{
	GENERATED_BODY()
};

// C13's L3 seam. Level 1 is UDFLocalProgressionProvider (a SaveGame slot); a server-backed
// provider later consumes the same FDFMatchRecord and answers the same questions, so nothing
// above this interface changes when progression moves off the machine.
class DFONLINE_API IDFProgressionProvider
{
	GENERATED_BODY()

public:
	/** Read the profile (creating a fresh one if none exists) and migrate it. */
	virtual bool Load() = 0;

	virtual bool Save() = 0;

	/** Fold a host-computed record into the profile (XP capped per source) and persist. */
	virtual void ApplyMatchRecord(const FDFMatchRecord& Record) = 0;

	/** The faction level implied by the stored XP (1-based). */
	virtual int32 LevelFor(const FGameplayTag& Faction) const = 0;

	/** The loaded profile, or null before Load(). */
	virtual UDFProfileSave* GetProfile() const = 0;
};
