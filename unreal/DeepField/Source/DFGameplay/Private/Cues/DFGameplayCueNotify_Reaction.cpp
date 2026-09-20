#include "Cues/DFGameplayCueNotify_Reaction.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Tint/DFPaletteSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFCues, Log, All);

FDFReactionCueEvent& UDFGameplayCueNotify_Reaction::OnReactionCue()
{
	static FDFReactionCueEvent Event;
	return Event;
}

bool UDFGameplayCueNotify_Reaction::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const FGameplayTag Cue = Parameters.OriginalTag.IsValid() ? Parameters.OriginalTag : GameplayCueTag;
	const FString Line = FString::Printf(TEXT("%s on %s: burst %.1f (%.0f%% of max) from %s"), *Cue.ToString(), *GetNameSafe(MyTarget),
		Parameters.RawMagnitude, Parameters.NormalizedMagnitude * 100.f, *GetNameSafe(Parameters.GetInstigator()));
	UE_LOG(LogDFCues, Log, TEXT("%s (handled by %s)"), *Line, *GetClass()->GetName());

	// ADR-0017: no colour literal — the burst is drawn in the palette's Danger swatch.
	const FLinearColor Colour = GetDefault<UDFPaletteSettings>()->Semantics.Danger;
	if (bOnScreenMessage && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, FMath::Max(DebugDrawSeconds, 2.f), Colour.ToFColor(true), Line);
	}
#if ENABLE_DRAW_DEBUG
	if (DebugDrawSeconds > 0.f && MyTarget)
	{
		if (UWorld* World = MyTarget->GetWorld())
		{
			const FVector At = MyTarget->GetActorLocation();
			DrawDebugSphere(World, At, DebugRadiusCm, 16, Colour.ToFColor(true), false, DebugDrawSeconds);
			DrawDebugString(World, At + FVector(0.f, 0.f, DebugRadiusCm), Line, nullptr, Colour.ToFColor(true), DebugDrawSeconds);
		}
	}
#endif
	OnReactionCue().Broadcast(MyTarget, Cue, Parameters);
	return true;
}
