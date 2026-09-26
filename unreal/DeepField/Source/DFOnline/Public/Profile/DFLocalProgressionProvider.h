#pragma once

#include "CoreMinimal.h"
#include "Profile/IDFProgressionProvider.h"
#include "UObject/Object.h"
#include "DFLocalProgressionProvider.generated.h"

class UDFProfileSave;

// Level 1 progression: one SaveGame slot ("profile") on this machine. The level curve is a
// placeholder owned by WS-07 (factions): a flat XpPerLevel read from the "xpPerLevel" balance
// dial when the tables carry one, DefaultXpPerLevel otherwise.
UCLASS()
class DFONLINE_API UDFLocalProgressionProvider : public UObject, public IDFProgressionProvider
{
	GENERATED_BODY()

public:
	static constexpr float DefaultXpPerLevel = 100.f;
	static constexpr int32 MaxLevel = 20;

	/** SaveGame slot; tests point this at a scratch slot or set bInMemoryOnly. */
	UPROPERTY() FString SlotName = TEXT("profile");
	UPROPERTY() int32 UserIndex = 0;
	/** Never touch the disk (tests): Load creates a fresh profile, Save serialises to a buffer. */
	UPROPERTY() bool bInMemoryOnly = false;
	/** XP per level for LevelFor; the balance dial "xpPerLevel" overrides it when present. */
	UPROPERTY() float XpPerLevel = DefaultXpPerLevel;

	virtual bool Load() override;
	virtual bool Save() override;
	virtual void ApplyMatchRecord(const FDFMatchRecord& Record) override;
	virtual int32 LevelFor(const FGameplayTag& Faction) const override;
	virtual UDFProfileSave* GetProfile() const override { return Profile; }

	/** The curve on its own so WS-07 can test against it: 1 + floor(Xp / XpPerLevel), clamped to MaxLevel. */
	static int32 LevelForXp(int32 Xp, float InXpPerLevel);

	/** The bytes of the last in-memory Save (bInMemoryOnly), for round-trip tests. */
	const TArray<uint8>& GetLastSavedBytes() const { return LastSavedBytes; }

	/** Replace the profile with one loaded from bytes (the in-memory counterpart of Load). */
	bool LoadFromBytes(const TArray<uint8>& Bytes);

private:
	UPROPERTY(Transient) TObjectPtr<UDFProfileSave> Profile;
	TArray<uint8> LastSavedBytes;
};
