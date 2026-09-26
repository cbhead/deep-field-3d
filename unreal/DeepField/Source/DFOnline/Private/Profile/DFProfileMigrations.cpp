#include "Profile/DFProfileMigrations.h"

#include "Profile/DFProfileSave.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFProfile, Log, All);

bool UDFProfileMigrations::Migrate(UDFProfileSave* Profile)
{
	if (!Profile)
	{
		return false;
	}
	if (Profile->Version > UDFProfileSave::CurrentVersion)
	{
		UE_LOG(LogDFProfile, Warning, TEXT("profile is version %d, this build knows %d: leaving it untouched"), Profile->Version, UDFProfileSave::CurrentVersion);
		return false;
	}
	while (Profile->Version < UDFProfileSave::CurrentVersion)
	{
		const int32 From = Profile->Version;
		switch (From)
		{
		case 0:
			MigrateV0ToV1(*Profile);
			break;
		default:
			// A gap in the chain is a programming error: refuse rather than guess.
			UE_LOG(LogDFProfile, Error, TEXT("no migration from profile version %d"), From);
			return false;
		}
		checkf(Profile->Version == From + 1, TEXT("migration from %d did not advance the version"), From);
		UE_LOG(LogDFProfile, Log, TEXT("profile migrated %d -> %d"), From, Profile->Version);
	}
	return true;
}

int32 UDFProfileMigrations::CurrentVersion()
{
	return UDFProfileSave::CurrentVersion;
}

void UDFProfileMigrations::MigrateV0ToV1(UDFProfileSave& Profile)
{
	Profile.Version = 1;
}
