#include "World/DFWorldActor.h"

#include "Components/BillboardComponent.h"

ADFWorldActor::ADFWorldActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	RootComponent = SceneRoot;

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(RootComponent);
		Sprite->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
		Sprite->bIsScreenSizeScaled = true;
		Sprite->SetHiddenInGame(true);
	}
#endif
}

void ADFWorldActor::SetStableId(FName NewId)
{
	AssignStableId(NewId);
#if WITH_EDITOR
	if (!NewId.IsNone() && GetActorLabel(false) != NewId.ToString())
	{
		SetActorLabel(NewId.ToString());
	}
#endif
}
