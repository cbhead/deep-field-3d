#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/DFHealthSet.h"
#include "Camera/CameraComponent.h"
#include "Combat/DFTargetable.h"
#include "Components/CapsuleComponent.h"
#include "DFGameplayTags.h"
#include "DFHeroCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Weapons/DFHeroWeaponComponent.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

// DF.Func.Weapon.RifleKillsTestlaneEnemy: the rifle on the real map, through the owner's own path. L_Testlane
// opens (PIE in the editor), the game mode spawns the player's ADFHeroCharacter, the solo match sends its
// first wave. The test stands the hero a few metres ahead of the leading enemy, points the controller at
// it and holds the trigger, as a player holding LMB would: the gun fires at its rate from the camera, the
// shots trace DF_Weapon and hit the enemy's body (DFEnemy.cpp: Body blocks DF_Weapon), the damage lands
// as the hero's, and the enemy dies and is announced with its bounty, credited to no tower.
namespace DFWeaponFunc
{
	const TCHAR* Map = TEXT("/Game/DF/Maps/Testlane/L_Testlane");
	/** How far ahead of the enemy the hero stands (cm): close, so nothing else is in between. */
	constexpr float StandOffCm = 700.f;

	class FShootLeadingEnemy : public IAutomationLatentCommand
	{
	public:
		FShootLeadingEnemy(FAutomationTestBase* InTest, double InTimeoutSeconds) : Test(InTest), TimeoutSeconds(InTimeoutSeconds) {}
		virtual ~FShootLeadingEnemy() override { Stop(); }
		virtual bool Update() override;

	private:
		bool Finish()
		{
			Stop();
			return true;
		}
		void Stop()
		{
			if (ADFHeroCharacter* H = Hero.Get())
			{
				if (UDFHeroWeaponComponent* Weapon = H->GetWeapon())
				{
					Weapon->SetTriggerHeld(false);
				}
			}
			if (UDFMessageBus* B = Bus.Get())
			{
				B->Unsubscribe(KilledHandle);
			}
			Bus.Reset();
		}
		AActor* PickLeader(UWorld& World) const;
		void StandBefore(UWorld& World, ADFHeroCharacter& H, AActor& Enemy) const;

		FAutomationTestBase* Test;
		double TimeoutSeconds;
		double StartedAt = -1.0;
		TWeakObjectPtr<ADFHeroCharacter> Hero;
		TWeakObjectPtr<AActor> Target;
		int32 TargetId = 0;
		float StartHealth = 0.f;
		float LowestHealth = 0.f;
		int32 StartRounds = 0;
		bool bKilled = false;
		FDFMsg_Kill Kill;
		TWeakObjectPtr<UDFMessageBus> Bus;
		FDFMessageHandle KilledHandle;
	};

	AActor* FShootLeadingEnemy::PickLeader(UWorld& World) const
	{
		// The body nearest the core is the front of the wave: nothing walks between it and a hero ahead of it.
		AActor* Best = nullptr;
		float BestRemaining = TNumericLimits<float>::Max();
		UDFTargetRegistry* Registry = UDFTargetRegistry::Get(&World);
		if (!Registry)
		{
			return nullptr;
		}
		for (AActor* Actor : Registry->GetTargets())
		{
			const IDFTargetable* Body = Cast<IDFTargetable>(Actor);
			if (Body && !Body->IsTargetDead() && !Body->IsTargetBurrowed() && Body->GetTargetLayer() == EDFEnemyLayer::Ground
				&& Body->GetRemainingToCore() < BestRemaining)
			{
				Best = Actor;
				BestRemaining = Body->GetRemainingToCore();
			}
		}
		return Best;
	}

	void FShootLeadingEnemy::StandBefore(UWorld& World, ADFHeroCharacter& H, AActor& Enemy) const
	{
		// On the lane ahead of it, on the ground there, facing its aim point.
		const IDFTargetable* Body = Cast<IDFTargetable>(&Enemy);
		FVector Ahead = Enemy.GetActorForwardVector().GetSafeNormal2D();
		if (Ahead.IsNearlyZero())
		{
			Ahead = FVector::ForwardVector;
		}
		const FVector Spot = Body->GetTargetPosition() + Ahead * StandOffCm;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DFWeaponFuncGround), false, &H);
		Params.AddIgnoredActor(&Enemy);
		FHitResult Ground;
		FVector Feet = Spot;
		if (World.LineTraceSingleByChannel(Ground, Spot + FVector(0.f, 0.f, 400.f), Spot - FVector(0.f, 0.f, 800.f), ECC_Visibility, Params))
		{
			Feet = Ground.ImpactPoint;
		}
		const float HalfHeight = H.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		H.SetActorLocation(Feet + FVector(0.f, 0.f, HalfHeight + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
		H.GetCharacterMovement()->StopMovementImmediately();
		if (AController* Controller = H.GetController())
		{
			const FVector Eye = H.GetFirstPersonCamera()->GetComponentLocation();
			Controller->SetControlRotation((Body->GetAimPoint() - Eye).Rotation());
		}
	}

	bool FShootLeadingEnemy::Update()
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt < 0.0)
		{
			StartedAt = Now;
		}
		if (Now - StartedAt > TimeoutSeconds)
		{
			Test->AddError(FString::Printf(TEXT("no kill in %.0f s: hero %s, target %s, health %.1f of %.1f, rounds %d"), TimeoutSeconds,
				Hero.IsValid() ? TEXT("found") : TEXT("missing"), Target.IsValid() ? TEXT("alive") : TEXT("none"),
				LowestHealth, StartHealth, Hero.IsValid() ? Hero->GetAmmoInMagazine() : -1));
			return Finish();
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || !World->HasBegunPlay())
		{
			return false;
		}
		if (!Hero.IsValid())
		{
			for (TActorIterator<ADFHeroCharacter> It(World); It; ++It)
			{
				if (It->IsLocallyControlled() && It->GetWeapon() && It->GetWeapon()->HasWeapon())
				{
					Hero = *It;
					break;
				}
			}
			if (!Hero.IsValid())
			{
				return false;   // the game mode has not spawned the player's hero yet
			}
			StartRounds = Hero->GetAmmoInMagazine();
			Bus = UDFMessageBus::Get(World);
			if (UDFMessageBus* B = Bus.Get())
			{
				KilledHandle = B->Subscribe<FDFMsg_Kill>(DFTags::Message_EnemyKilled, [this](const FGameplayTag&, const FDFMsg_Kill& Msg)
				{
					if (Msg.EnemyId == TargetId && TargetId != 0)
					{
						bKilled = true;
						Kill = Msg;
					}
				});
			}
		}
		ADFHeroCharacter& H = *Hero.Get();

		if (bKilled)
		{
			Test->TestTrue(*FString::Printf(TEXT("the enemy was hurt before it died (%.1f of %.1f)"), LowestHealth, StartHealth), LowestHealth < StartHealth);
			Test->TestEqual(TEXT("no tower killed it"), Kill.KillerStructureId, 0);
			Test->TestTrue(*FString::Printf(TEXT("its bounty was announced (%d)"), Kill.Bounty), Kill.Bounty > 0);
			Test->TestTrue(*FString::Printf(TEXT("the magazine moved (%d of %d left)"), H.GetAmmoInMagazine(), StartRounds),
				H.GetAmmoInMagazine() < StartRounds || H.IsReloading());
			Test->AddInfo(FString::Printf(TEXT("the rifle killed enemy %d in %.1f s"), TargetId, Now - StartedAt));
			return Finish();
		}

		if (!Target.IsValid())
		{
			AActor* Leader = PickLeader(*World);
			if (!Leader)
			{
				return false;   // the first wave has not spawned yet
			}
			const IDFTargetable* Body = Cast<IDFTargetable>(Leader);
			const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Leader);
			if (!ASC || !ASC->HasAttributeSetForAttribute(UDFHealthSet::GetHealthAttribute()))
			{
				Test->AddError(TEXT("the leading enemy has no ability system with UDFHealthSet"));
				return Finish();
			}
			Target = Leader;
			TargetId = Body->GetTargetId();
			StartHealth = ASC->GetNumericAttribute(UDFHealthSet::GetHealthAttribute());
			LowestHealth = StartHealth;
		}
		AActor& Enemy = *Target.Get();
		if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Enemy))
		{
			LowestHealth = FMath::Min(LowestHealth, ASC->GetNumericAttribute(UDFHealthSet::GetHealthAttribute()));
		}
		StandBefore(*World, H, Enemy);
		H.GetWeapon()->SetTriggerHeld(true);
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFRifleKillsTestlaneEnemyTest, "DF.Func.Weapon.RifleKillsTestlaneEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDFRifleKillsTestlaneEnemyTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(DFWeaponFunc::Map))
	{
		AddError(FString::Printf(TEXT("could not open %s"), DFWeaponFunc::Map));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(DFWeaponFunc::FShootLeadingEnemy(this, 60.0));
#if WITH_EDITOR
	if (GIsEditor)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
