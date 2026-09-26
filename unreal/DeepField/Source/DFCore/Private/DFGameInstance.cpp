#include "DFGameInstance.h"

#include "Misc/App.h"
#include "Misc/EngineVersion.h"

void UDFGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogTemp, Log, TEXT("Deep Field 3D %s (UE %s)"), *GetBuildLabel(), *FEngineVersion::Current().ToString(EVersionComponent::Patch));
}

FString UDFGameInstance::GetBuildLabel()
{
	// Project version from DefaultGame.ini plus the changelist when the build has one.
	FString Version;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
	const uint32 CL = FEngineVersion::Current().GetChangelist();
	return CL ? FString::Printf(TEXT("%s+%u"), *Version, CL) : Version;
}
