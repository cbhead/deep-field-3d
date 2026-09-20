#include "Cues/DFGameplayCueNotify.h"

#include "GameplayTagsManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFCues, Log, All);

bool UDFGameplayCueNotify::ResolveCueTagName()
{
	if (CueTagName.IsNone())
	{
		return true;
	}
	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(CueTagName, /*ErrorIfNotFound*/ false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogDFCues, Error, TEXT("%s: CueTagName '%s' is not a registered gameplay tag (Config/Tags/DF_Gameplay.ini?); GameplayCueTag left as '%s'"),
			*GetPathName(), *CueTagName.ToString(), *GameplayCueTag.ToString());
		return false;
	}
	if (GameplayCueTag != Tag)
	{
		GameplayCueTag = Tag;
	}
	GameplayCueName = Tag.GetTagName();
	return true;
}

void UDFGameplayCueNotify::PostInitProperties()
{
	Super::PostInitProperties();
	ResolveCueTagName();
}

void UDFGameplayCueNotify::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		ResolveCueTagName();   // before the engine derives / mirrors the name
	}
	Super::Serialize(Ar);
	if (Ar.IsLoading())
	{
		ResolveCueTagName();   // the name is authoritative once loaded
	}
}

#if WITH_EDITOR
void UDFGameplayCueNotify::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Changed = PropertyChangedEvent.Property;
	if (Changed && Changed->GetFName() == GET_MEMBER_NAME_CHECKED(UDFGameplayCueNotify, CueTagName))
	{
		ResolveCueTagName();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
