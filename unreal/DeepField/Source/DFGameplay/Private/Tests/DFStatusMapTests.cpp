#include "Attributes/DFHealthSet.h"
#include "DFGameplayLocalTags.h"
#include "Dev/DFStatusTestRig.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

// The in-world half of the WS-02 DoD: L_Test_Status shows chill + burn -> thermalShock 12 % via the
// cue. Runs in a game process (-game -nullrhi): the map boots, the rig chills and burns itself, and
// the test reads what GameplayCue.DF.Reaction.ThermalShock reported through the rig.
//   editor-lock.sh UnrealEditor-Cmd DeepField.uproject /Game/DF/Gameplay/L_Test_Status -game -nullrhi -unattended -nop4 -nosplash -NoSound
//       -ExecCmds="Automation RunTests DF.Func.Status; Quit" -TestExit="Automation Test Queue Empty" -log
namespace
{
	const TCHAR* TestStatusMap = TEXT("/Game/DF/Gameplay/L_Test_Status");

	/** Polls the game world for the rig until it finished a cycle, then checks the cue's report. */
	class FDFWaitForStatusRigCommand : public IAutomationLatentCommand
	{
	public:
		FDFWaitForStatusRigCommand(FAutomationTestBase* InTest, double InTimeoutSeconds) : Test(InTest), TimeoutSeconds(InTimeoutSeconds) {}
		virtual bool Update() override;

	private:
		FAutomationTestBase* Test;
		double TimeoutSeconds;
		double StartedAt = -1.0;
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
		}
		if (Rig && Rig->CompletedCycles >= 1)
		{
			const FGameplayTag Leaf = DFGameplayLocalTags::ReactionCue(TEXT("thermalShock"));
			Test->TestEqual(TEXT("the component reported thermalShock"), Rig->LastReactionId, FName(TEXT("thermalShock")));
			Test->TestTrue(TEXT("the reaction cue's notify ran"), Rig->bLastCueSeen);
			Test->TestEqual(TEXT("cue fired: GameplayCue.DF.Reaction.ThermalShock"), Rig->LastCueTag, Leaf);
			Test->TestEqual(TEXT("handled by the leaf asset GC_DF_Reaction_ThermalShock, not only the GC_DF_Reaction fallback"), Rig->LastCueHandlerTag, Leaf);
			Test->TestTrue(*FString::Printf(TEXT("cue carried the 12%% burst: %.2f of %.0f"), Rig->LastCueBurst, Rig->MaxHealth),
				FMath::IsNearlyEqual(Rig->LastCueBurst, 0.12f * Rig->MaxHealth, 1e-3f));
			Test->TestTrue(TEXT("cue carried the burst fraction 0.12"), FMath::IsNearlyEqual(Rig->LastCueFraction, 0.12f, 1e-4f));
			Test->TestTrue(*FString::Printf(TEXT("health after the burst is 88%% (got %.2f)"), Rig->LastHealthAfterBurst),
				FMath::IsNearlyEqual(Rig->LastHealthAfterBurst, 0.88f * Rig->MaxHealth, 1e-3f));
			Test->AddInfo(FString::Printf(TEXT("L_Test_Status: %s"), *Rig->StateText));
			return true;
		}
		if (Now - StartedAt > TimeoutSeconds)
		{
			Test->AddError(Rig ? FString::Printf(TEXT("the rig never completed a cycle in %.0f s (state: %s)"), TimeoutSeconds, *Rig->StateText)
			                   : FString::Printf(TEXT("no ADFStatusTestRig in the game world after %.0f s (is %s loaded?)"), TimeoutSeconds, TestStatusMap));
			return true;
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusThermalShockViaCueTest, "DF.Func.Status.ThermalShockViaCue", EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusThermalShockViaCueTest::RunTest(const FString& Parameters)
{
	// In a game process this travels to the map when it is not already up, then waits for it.
	if (!AutomationOpenMap(TestStatusMap))
	{
		AddError(FString::Printf(TEXT("could not open %s"), TestStatusMap));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FDFWaitForStatusRigCommand(this, 20.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
