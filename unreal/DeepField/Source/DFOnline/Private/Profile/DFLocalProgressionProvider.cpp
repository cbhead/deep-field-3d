#include "Profile/DFLocalProgressionProvider.h"

#include "Content/DFContentSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Profile/DFProfileMigrations.h"
#include "Profile/DFProfileSave.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFProgression, Log, All);

bool UDFLocalProgressionProvider::Load()
{
	UDFProfileSave* Loaded = nullptr;
	if (!bInMemoryOnly && UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		Loaded = Cast<UDFProfileSave>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
		if (!Loaded)
		{
			// A save of another class or a corrupt file: start fresh but keep the file until the next Save overwrites it.
			UE_LOG(LogDFProgression, Warning, TEXT("profile slot '%s' did not load as UDFProfileSave; starting a new profile"), *SlotName);
		}
	}
	if (!Loaded)
	{
		Loaded = NewObject<UDFProfileSave>(this);
	}
	if (!UDFProfileMigrations::Migrate(Loaded))
	{
		UE_LOG(LogDFProgression, Warning, TEXT("profile slot '%s' could not be migrated; it will be used read-only"), *SlotName);
	}
	Profile = Loaded;

	if (const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this); Content && Content->IsReady())
	{
		const float Dial = Content->Balance(TEXT("xpPerLevel"), 0.f);
		if (Dial > 0.f)
		{
			XpPerLevel = Dial;
		}
	}
	return true;
}

bool UDFLocalProgressionProvider::Save()
{
	if (!Profile)
	{
		return false;
	}
	if (Profile->Version > UDFProfileSave::CurrentVersion)
	{
		// Never overwrite a newer build's profile with this build's layout.
		return false;
	}
	if (bInMemoryOnly)
	{
		LastSavedBytes.Reset();
		return UGameplayStatics::SaveGameToMemory(Profile, LastSavedBytes);
	}
	return UGameplayStatics::SaveGameToSlot(Profile, SlotName, UserIndex);
}

void UDFLocalProgressionProvider::ApplyMatchRecord(const FDFMatchRecord& Record)
{
	if (!Profile && !Load())
	{
		return;
	}
	Profile->ApplyMatchRecord(Record);
	Save();
}

int32 UDFLocalProgressionProvider::LevelFor(const FGameplayTag& Faction) const
{
	const int32* Xp = Profile ? Profile->FactionXp.Find(Faction) : nullptr;
	return LevelForXp(Xp ? *Xp : 0, XpPerLevel);
}

int32 UDFLocalProgressionProvider::LevelForXp(int32 Xp, float InXpPerLevel)
{
	const float PerLevel = InXpPerLevel > 0.f ? InXpPerLevel : DefaultXpPerLevel;
	return FMath::Clamp(1 + FMath::FloorToInt32(FMath::Max(0, Xp) / PerLevel), 1, MaxLevel);
}

bool UDFLocalProgressionProvider::LoadFromBytes(const TArray<uint8>& Bytes)
{
	UDFProfileSave* Loaded = Cast<UDFProfileSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!Loaded)
	{
		return false;
	}
	UDFProfileMigrations::Migrate(Loaded);
	Profile = Loaded;
	return true;
}
