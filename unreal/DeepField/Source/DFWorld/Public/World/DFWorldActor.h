#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DFWorldActor.generated.h"

class UBillboardComponent;

// Base of every actor the level importer places into L_<Map>_Gameplay (C6). Each carries the
// stable id from the level file — which is also its editor label — so a re-import can find it by
// id and move it in place rather than recreate it (actor GUIDs, references and overrides survive).
// Subclasses own the id property under the contract's name (ADFSocket::SocketId, ADFCore::Id, ...);
// the base only knows how to ask for it and how to stamp it on the label.
UCLASS(Abstract)
class DFWORLD_API ADFWorldActor : public AActor
{
	GENERATED_BODY()

public:
	ADFWorldActor();

	/** The permanent id from the level file (never renamed, only added). */
	virtual FName GetStableId() const PURE_VIRTUAL(ADFWorldActor::GetStableId, return NAME_None;);

	/** Sets the id and, in the editor, the actor label to match. */
	void SetStableId(FName NewId);

protected:
	/** Store the id in the subclass's property. */
	virtual void AssignStableId(FName NewId) PURE_VIRTUAL(ADFWorldActor::AssignStableId, );

	UPROPERTY(VisibleAnywhere, Category = "DF")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
