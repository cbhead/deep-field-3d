#include "Attributes/DFHealthSet.h"
#include "DFGameplayLocalTags.h"
#include "DFGameplayTags.h"
#include "Dev/DFStatusTestRig.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Tests/AutomationCommon.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

// The in-level half of the WS-02 DoD: "L_Test_Status shows chill + burn -> thermalShock 12 % via cue".
// In the editor (unreal/Build/test.sh) AutomationOpenMap loads the map and starts PIE; in a -game
// process it travels there. The rig chills and burns itself in a real world, and the test reads
// what the component, the reaction cue (through UDFMessageBus) and DF.Message.ReactionTriggered
// reported.
//
// WHICH MAP. /Game/DF/Dev/L_Test_Status is the DoD level, built by
// Source/DFGameplay/Dev/make-l-test-status.py (floor, sun, player start, one placed rig). It is NOT
// committed: Content/DF/Dev/** is WS-15's glob, so a .umap from a [WS-02] PR is an ownership-check
// violation until INT adds the row this workstream's file asks for. So the test opens it when it is
// there (an agent that ran the generator, or the day the row lands) and otherwise falls back to
// WS-00's committed /Game/DF/Dev/L_Dev_Empty and spawns the rig itself — the same actor, the same
// world, the same cue path, no new binary. What is asserted does not change either way.
namespace
{
	const TCHAR* TestStatusMap = TEXT("/Game/DF/Dev/L_Test_Status");
	const TCHAR* FallbackMap = TEXT("/Game/DF/Dev/L_Dev_Empty");

	/** The DoD level when it exists on disk, else the empty dev level (the rig is then spawned). */
	const TCHAR* MapToOpen()
	{
		return FPackageName::DoesPackageExist(TestStatusMap) ? TestStatusMap : FallbackMap;
	}

	/** Polls the game world for the rig — spawning one if the level carries none — until it finished
	 *  a cycle, then checks what the cycle showed. */
	class FDFWaitForStatusRigCommand : public IAutomationLatentCommand
	{
	public:
		FDFWaitForStatusRigCommand(FAutomationTestBase* InTest, double InTimeoutSeconds) : Test(InTest), TimeoutSeconds(InTimeoutSeconds) {}
		virtual bool Update() override;

	private:
		FAutomationTestBase* Test;
		double TimeoutSeconds;
		double StartedAt = -1.0;
		bool bSpawned = false;
	};

	bool FDFWaitForStatusRigCommand::Update()
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt < 0.0)
		{
			StartedAt = Now;
		}
		ADFStatusTestRig* Rig = nullptr;
		if (UWorld* World = AutomationCommon::GetAnyGameWorld())
		{
			TActorIterator<ADFStatusTestRig> It(World);
			Rig = It ? *It : nullptr;
			if (!Rig && !bSpawned && World->HasBegunPlay())
			{
				// The fallback map carries no rig: spawn one into the running world (BeginPlay runs
				// at once, so the cycle starts on this tick).
				bSpawned = true;
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Rig = World->SpawnActor<ADFStatusTestRig>(ADFStatusTestRig::StaticClass(), FTransform(FVector(0.f, 0.f, 100.f)), Params);
				Test->AddInfo(FString::Printf(TEXT("%s carries no rig: spawned one (%s)"), MapToOpen(), *GetNameSafe(Rig)));
			}
		}
		if (Rig && Rig->CompletedCycles >= 1)
		{
			const FGameplayTag Cue = DFGameplayLocalTags::ReactionCue(TEXT("thermalShock"));
			Test->TestEqual(TEXT("the component reported thermalShock"), Rig->LastReactionId, FName(TEXT("thermalShock")));
			Test->TestTrue(*FString::Printf(TEXT("UDFHealthSet took the 12%% burst: health %.2f of %.0f"), Rig->LastHealthAfterBurst, Rig->MaxHealth),
				FMath::IsNearlyEqual(Rig->LastHealthAfterBurst, 0.88f * Rig->MaxHealth, 1e-3f));
			Test->TestTrue(TEXT("GameplayCue.DF.Reaction.ThermalShock was fired (the notify ran and reached the bus)"), Rig->bLastCueSeen);
			Test->TestEqual(TEXT("cue fired: GameplayCue.DF.Reaction.ThermalShock"), Rig->LastCueTag, Cue);
			Test->TestTrue(*FString::Printf(TEXT("cue carried the 12%% burst: %.2f of %.0f"), Rig->LastCueBurst, Rig->MaxHealth),
				FMath::IsNearlyEqual(Rig->LastCueBurst, 0.12f * Rig->MaxHealth, 1e-3f));
			Test->TestTrue(TEXT("cue carried the burst fraction 0.12"), FMath::IsNearlyEqual(Rig->LastCueFraction, 0.12f, 1e-4f));
			Test->TestTrue(TEXT("DF.Message.ReactionTriggered reached the bus"), Rig->bLastMessageSeen);
			Test->TestEqual(TEXT("the message named DF.Reaction.ThermalShock"), Rig->LastMessageReaction, FGameplayTag(DFTags::Reaction_ThermalShock));
			Test->AddInfo(FString::Printf(TEXT("%s (%s handled the cue): %s"), MapToOpen(), *Rig->LastCueHandlerTag.ToString(), *Rig->StateText));
			return true;
		}
		if (Now - StartedAt > TimeoutSeconds)
		{
			Test->AddError(Rig ? FString::Printf(TEXT("the rig never completed a cycle in %.0f s (state: %s)"), TimeoutSeconds, *Rig->StateText)
			                   : FString::Printf(TEXT("no ADFStatusTestRig in the game world after %.0f s (is %s loaded?)"), TimeoutSeconds, MapToOpen()));
			return true;
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusThermalShockInLevelTest, "DF.Func.Status.ThermalShockInLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusThermalShockInLevelTest::RunTest(const FString& Parameters)
{
	// Editor: load the map and start PIE (UEditorEngine::AutomationLoadMap). Game: travel to it.
	const TCHAR* Map = MapToOpen();
	if (!AutomationOpenMap(Map))
	{
		AddError(FString::Printf(TEXT("could not open %s"), Map));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FDFWaitForStatusRigCommand(this, 30.0));
#if WITH_EDITOR
	if (GIsEditor)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
