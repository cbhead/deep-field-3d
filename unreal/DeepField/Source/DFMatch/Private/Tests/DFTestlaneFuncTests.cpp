#include "Components/InstancedStaticMeshComponent.h"
#include "DFGameplayTags.h"
#include "DFMatchState.h"
#include "DFPlayerController.h"
#include "Economy/DFEconomyStateComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Towers/DFBuildSubsystem.h"
#include "Towers/DFTower.h"
#include "World/DFSocket.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

// DF.Func.Tower.KillsTestlaneEnemies: the first playable's loop on the real map, with nothing stood in.
// L_Testlane opens (PIE in the editor, travel in -game); the solo match starts its waves by itself;
// two Lances go up through UDFBuildSubsystem, paid from WS-06's economy on the match state; then the
// test waits for the wave's drifters to walk into range and be killed by the towers, and checks the
// books: the builds were charged, every tower kill paid its bounty, and the towers credit the kills.
// This is what WS-04's DoD calls "fires at a test enemy", with the enemy WS-05's real ADFEnemy.
namespace DFTestlaneFunc
{
	const TCHAR* Map = TEXT("/Game/DF/Maps/Testlane/L_Testlane");
	const FName Lance(TEXT("lance"));
	constexpr int32 TowersToBuild = 2;
	constexpr int32 KillsWanted = 3;

	class FWaitForTowerKills : public IAutomationLatentCommand
	{
	public:
		FWaitForTowerKills(FAutomationTestBase* InTest, double InTimeoutSeconds) : Test(InTest), TimeoutSeconds(InTimeoutSeconds) {}
		virtual ~FWaitForTowerKills() override { Unsubscribe(); }
		virtual bool Update() override;

	private:
		bool Build(UWorld& World);
		void Unsubscribe()
		{
			if (UDFMessageBus* B = Bus.Get())
			{
				B->Unsubscribe(KilledHandle);
			}
			Bus.Reset();
		}
		bool Finish()
		{
			Unsubscribe();
			return true;
		}

		FAutomationTestBase* Test;
		double TimeoutSeconds;
		double StartedAt = -1.0;
		bool bBuilt = false;
		int32 MoneyAfterBuild = 0;
		int32 TowerKills = 0;
		int32 BountyPaid = 0;
		TWeakObjectPtr<UDFMessageBus> Bus;
		FDFMessageHandle KilledHandle;
	};

	bool FWaitForTowerKills::Build(UWorld& World)
	{
		ADFMatchState* Match = World.GetGameState<ADFMatchState>();
		UDFBuildSubsystem* Builder = UDFBuildSubsystem::Get(&World);
		if (!Match || !Match->GetEconomy() || !Builder)
		{
			return false;   // not up yet; try next frame
		}
		TArray<ADFSocket*> Sockets;
		for (TActorIterator<ADFSocket> It(&World); It; ++It)
		{
			if (It->Tag == EDFSocketTag::Ground)
			{
				Sockets.Add(*It);
			}
		}
		if (Sockets.Num() < TowersToBuild)
		{
			Test->AddError(FString::Printf(TEXT("%s has %d ground sockets; the test builds on %d"), Map, Sockets.Num(), TowersToBuild));
			return true;
		}
		Sockets.Sort([](const ADFSocket& A, const ADFSocket& B) { return A.SocketId.LexicalLess(B.SocketId); });

		UDFEconomyStateComponent* Economy = Match->GetEconomy();
		const int32 MoneyBefore = Economy->GetMoney();
		int32 Spent = 0;
		for (int32 i = 0; i < TowersToBuild; ++i)
		{
			const FDFBuildResult Result = Builder->PlaceTower(/*PlayerId*/ 1, Lance, Sockets[i]->SocketId);
			Test->TestTrue(*FString::Printf(TEXT("a Lance goes up on %s (%s)"), *Sockets[i]->SocketId.ToString(), *Result.Reason.ToString()), Result.Succeeded());
			if (Result.Succeeded() && Result.Tower)
			{
				Spent += Result.Tower->GetSpent();
				Test->TestTrue(TEXT("the tower stands on its pad"), FVector::Dist2D(Result.Tower->GetActorLocation(), Sockets[i]->GetPadTop()) < 1.f);
			}
		}
		MoneyAfterBuild = Economy->GetMoney();
		Test->TestTrue(TEXT("building cost money"), Spent > 0);
		Test->TestEqual(TEXT("the economy was charged what the towers cost"), MoneyAfterBuild, MoneyBefore - Spent);

		Bus = UDFMessageBus::Get(&World);
		if (UDFMessageBus* B = Bus.Get())
		{
			KilledHandle = B->Subscribe<FDFMsg_Kill>(DFTags::Message_EnemyKilled, [this](const FGameplayTag&, const FDFMsg_Kill& Msg)
			{
				if (Msg.KillerStructureId != 0)
				{
					++TowerKills;
				}
				BountyPaid += Msg.Bounty;
			});
		}
		return true;
	}

	bool FWaitForTowerKills::Update()
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt < 0.0)
		{
			StartedAt = Now;
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || !World->HasBegunPlay())
		{
			if (Now - StartedAt > TimeoutSeconds)
			{
				Test->AddError(FString::Printf(TEXT("%s never began play"), Map));
				return Finish();
			}
			return false;
		}
		if (!bBuilt)
		{
			bBuilt = Build(*World);
			if (Test->HasAnyErrors())
			{
				return Finish();
			}
			return false;
		}
		if (TowerKills >= KillsWanted)
		{
			const ADFMatchState* Match = World->GetGameState<ADFMatchState>();
			const UDFEconomyStateComponent* Economy = Match ? Match->GetEconomy() : nullptr;
			int32 Credited = 0;
			for (TActorIterator<ADFTower> It(World); It; ++It)
			{
				Credited += It->GetKills();
			}
			Test->TestTrue(*FString::Printf(TEXT("the towers credit their kills (%d)"), Credited), Credited >= KillsWanted);
			if (Economy)
			{
				// Leaks cost lives, not money, and nothing else spends: money is the build's change plus bounty.
				Test->TestEqual(TEXT("every kill's bounty was paid"), Economy->GetMoney(), MoneyAfterBuild + BountyPaid);
				Test->TestTrue(*FString::Printf(TEXT("bounty was paid (%d)"), BountyPaid), BountyPaid >= KillsWanted);
			}
			Test->AddInfo(FString::Printf(TEXT("%d tower kills in %.1f s, %d bounty"), TowerKills, Now - StartedAt, BountyPaid));
			return Finish();
		}
		if (Now - StartedAt > TimeoutSeconds)
		{
			const ADFMatchState* Match = World->GetGameState<ADFMatchState>();
			Test->AddError(FString::Printf(TEXT("only %d tower kills in %.0f s (wave %d, phase %d, %d enemies left)"), TowerKills, TimeoutSeconds,
				Match ? Match->GetWaveIndex() : -2, Match ? static_cast<int32>(Match->GetPhase()) : -1, Match ? Match->GetEnemiesRemaining() : -1));
			return Finish();
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerKillsTestlaneEnemiesTest, "DF.Func.Tower.KillsTestlaneEnemies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDFTowerKillsTestlaneEnemiesTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(DFTestlaneFunc::Map))
	{
		AddError(FString::Printf(TEXT("could not open %s"), DFTestlaneFunc::Map));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(DFTestlaneFunc::FWaitForTowerKills(this, 90.0));
#if WITH_EDITOR
	if (GIsEditor)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
#endif
	return true;
}

// DF.Func.Build.PadHighlightFollowsAim: the player can see where hold E builds, and how far it would
// reach. On L_Testlane the local controller aims its hero at a pad: the pad's ring turns green (free)
// with a range ring at the chosen Lance's 12 m (towers.json; Testlane's waves carry no weather); the
// wheel picks a Nova: 16 m with its 5 m dead zone inside; a Lance goes up on it: gold, and the range ring
// is the standing Lance's reach, not the choice's; a Range purchase grows it; the hero looks at the sky:
// both rings go away. PlayerTick drives it, as in play; the test only turns the view and the wheel.
namespace DFTestlaneFunc
{
	class FWaitForPadHighlight : public IAutomationLatentCommand
	{
	public:
		FWaitForPadHighlight(FAutomationTestBase* InTest, double InTimeoutSeconds) : Test(InTest), TimeoutSeconds(InTimeoutSeconds) {}
		virtual bool Update() override;

	private:
		enum class EStep : uint8 { Aim, ExpectFree, ExpectNovaReach, ExpectOccupied, ExpectUpgradedReach, ExpectNone };
		bool Fail(const FString& Why) { Test->AddError(Why); return true; }

		FAutomationTestBase* Test;
		double TimeoutSeconds;
		double StartedAt = -1.0;
		EStep Step = EStep::Aim;
		int32 FramesInStep = 0;
		TWeakObjectPtr<ADFSocket> Target;
		TWeakObjectPtr<ADFTower> Built;
	};

	/** The range ring shows RadiusCm (and MinRadiusCm inside it) in Tone, drawn: what the player sees. */
	bool ShowsReach(const ADFSocket& Pad, float RadiusCm, float MinRadiusCm, EDFPadHighlight Tone)
	{
		const UInstancedStaticMeshComponent* Ring = Pad.GetRangeRing();
		const int32 Segments = ADFSocket::RangeSegmentsFor(RadiusCm) + (MinRadiusCm > 0.f ? ADFSocket::RangeSegmentsFor(MinRadiusCm) : 0);
		return Ring && Ring->IsVisible() && Ring->GetInstanceCount() == Segments && Pad.GetRangePreviewTone() == Tone
			&& FMath::IsNearlyEqual(Pad.GetRangePreviewRadius(), RadiusCm, 1.f) && FMath::IsNearlyEqual(Pad.GetRangePreviewMinRadius(), MinRadiusCm, 1.f);
	}

	FString DescribeReach(const ADFSocket& Pad)
	{
		const UInstancedStaticMeshComponent* Ring = Pad.GetRangeRing();
		return FString::Printf(TEXT("%.0f cm, inner %.0f cm, tone %d, %s, %d segments"), Pad.GetRangePreviewRadius(), Pad.GetRangePreviewMinRadius(),
			static_cast<int32>(Pad.GetRangePreviewTone()), Ring && Ring->IsVisible() ? TEXT("shown") : TEXT("hidden"), Ring ? Ring->GetInstanceCount() : -1);
	}

	bool FWaitForPadHighlight::Update()
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt < 0.0)
		{
			StartedAt = Now;
		}
		if (Now - StartedAt > TimeoutSeconds)
		{
			return Fail(FString::Printf(TEXT("the pad highlight did not follow the aim within %.0f s (step %d)"), TimeoutSeconds, static_cast<int32>(Step)));
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		ADFPlayerController* Controller = World ? Cast<ADFPlayerController>(World->GetFirstPlayerController()) : nullptr;
		APawn* Hero = Controller ? Controller->GetPawn() : nullptr;
		if (!World || !World->HasBegunPlay() || !Hero)
		{
			return false;
		}
		++FramesInStep;
		switch (Step)
		{
		case EStep::Aim:
		{
			// The nearest ground pad, looked at from the hero's eye.
			ADFSocket* Nearest = nullptr;
			for (TActorIterator<ADFSocket> It(World); It; ++It)
			{
				if (It->Tag == EDFSocketTag::Ground && (!Nearest || FVector::DistSquared(It->GetActorLocation(), Hero->GetActorLocation()) < FVector::DistSquared(Nearest->GetActorLocation(), Hero->GetActorLocation())))
				{
					Nearest = *It;
				}
			}
			if (!Nearest)
			{
				return Fail(FString::Printf(TEXT("%s has no ground pad"), Map));
			}
			Target = Nearest;
			FVector Eye;
			FRotator Unused;
			Controller->GetPlayerViewPoint(Eye, Unused);
			Controller->SetControlRotation((Nearest->GetPadTop() - Eye).Rotation());
			Step = EStep::ExpectFree;
			FramesInStep = 0;
			return false;
		}
		case EStep::ExpectFree:
		{
			ADFSocket* Pad = Target.Get();
			if (Pad && Pad->GetAimHighlight() == EDFPadHighlight::Free)
			{
				Test->TestTrue(TEXT("the controller's build target is the pad it looks at"), Controller->FindAimedSocket() == Pad);
				Test->TestEqual(TEXT("hold E builds a Lance"), Controller->GetQuickBuildTowerId(), Lance);
				// Set in the same frame as the ring: the chosen Lance's reach, in clear weather.
				Test->TestTrue(*FString::Printf(TEXT("free: the range ring is the Lance's 12 m (towers.json), green (%s)"), *DescribeReach(*Pad)),
					ShowsReach(*Pad, 1200.f, 0.f, EDFPadHighlight::Free));
				Controller->CycleQuickBuild(1);   // the wheel: Nova
				Step = EStep::ExpectNovaReach;
				FramesInStep = 0;
			}
			return false;
		}
		case EStep::ExpectNovaReach:
		{
			ADFSocket* Pad = Target.Get();
			if (Pad && ShowsReach(*Pad, 1600.f, 500.f, EDFPadHighlight::Free))
			{
				Test->AddInfo(TEXT("the wheel's Nova: 16 m, with its 5 m dead zone as an inner ring"));
				const FDFBuildResult Result = UDFBuildSubsystem::Get(World)->PlaceTower(/*PlayerId*/ 1, Lance, Pad->SocketId);
				Test->TestTrue(*FString::Printf(TEXT("a Lance goes up on the aimed pad (%s)"), *Result.Reason.ToString()), Result.Succeeded());
				Built = Result.Tower;
				Step = EStep::ExpectOccupied;
				FramesInStep = 0;
			}
			else if (FramesInStep > 30)
			{
				return Fail(FString::Printf(TEXT("the range ring did not follow the wheel to the Nova's 16 m / 5 m (%s)"), Pad ? *DescribeReach(*Pad) : TEXT("no pad")));
			}
			return false;
		}
		case EStep::ExpectOccupied:
		{
			ADFSocket* Pad = Target.Get();
			ADFTower* Tower = Built.Get();
			if (Pad && Tower && Pad->GetAimHighlight() == EDFPadHighlight::Occupied)
			{
				// The standing Lance's reach, although hold E would now build a Nova.
				Test->TestEqual(TEXT("the standing Lance reaches 12 m"), Tower->GetRangeMeters(), 12.f, 1e-3f);
				Test->TestTrue(*FString::Printf(TEXT("occupied: the range ring is the standing tower's reach, gold, no inner ring (%s)"), *DescribeReach(*Pad)),
					ShowsReach(*Pad, Tower->GetRangeMeters() * 100.f, 0.f, EDFPadHighlight::Occupied));
				const FDFTowerRow* Row = Tower->GetRow();
				const int32 RangePath = Row ? DFTowerMath::RangePathIndex(*Row) : INDEX_NONE;
				if (!Test->TestTrue(TEXT("a Lance has a Range path"), RangePath != INDEX_NONE))
				{
					return true;
				}
				Tower->ApplyUpgrade(RangePath, /*MoneyCost*/ 0);   // as hold U would, once paid for
				Step = EStep::ExpectUpgradedReach;
				FramesInStep = 0;
			}
			else if (Pad && FramesInStep > 30)
			{
				return Fail(FString::Printf(TEXT("the pad never read occupied after the build (%s)"), *DescribeReach(*Pad)));
			}
			return false;
		}
		case EStep::ExpectUpgradedReach:
		{
			ADFSocket* Pad = Target.Get();
			const ADFTower* Tower = Built.Get();
			if (Pad && Tower && Tower->GetRangeMeters() > 12.f && ShowsReach(*Pad, Tower->GetRangeMeters() * 100.f, 0.f, EDFPadHighlight::Occupied))
			{
				Test->AddInfo(FString::Printf(TEXT("pad %s: free (green, 12 m) when aimed, occupied (gold) once built on, %.2f m after a Range purchase"),
					*Pad->SocketId.ToString(), Tower->GetRangeMeters()));
				Controller->SetControlRotation(FRotator(80.f, 0.f, 0.f));   // at the sky
				Step = EStep::ExpectNone;
				FramesInStep = 0;
			}
			else if (FramesInStep > 30)
			{
				return Fail(FString::Printf(TEXT("the range ring did not grow with the Range purchase (%.2f m; %s)"),
					Tower ? Tower->GetRangeMeters() : -1.f, Pad ? *DescribeReach(*Pad) : TEXT("no pad")));
			}
			return false;
		}
		case EStep::ExpectNone:
		{
			ADFSocket* Pad = Target.Get();
			if (Pad && Pad->GetAimHighlight() == EDFPadHighlight::None && FramesInStep > 1)
			{
				Test->TestNull(TEXT("looking at the sky there is no build target"), Controller->FindAimedSocket());
				Test->TestEqual(TEXT("aimed away: no reach shown"), Pad->GetRangePreviewRadius(), 0.f);
				Test->TestFalse(TEXT("aimed away: the range ring is hidden"), Pad->GetRangeRing() && Pad->GetRangeRing()->IsVisible());
				return true;
			}
			return false;
		}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFPadHighlightFollowsAimTest, "DF.Func.Build.PadHighlightFollowsAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDFPadHighlightFollowsAimTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(DFTestlaneFunc::Map))
	{
		AddError(FString::Printf(TEXT("could not open %s"), DFTestlaneFunc::Map));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(DFTestlaneFunc::FWaitForPadHighlight(this, 60.0));
#if WITH_EDITOR
	if (GIsEditor)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
