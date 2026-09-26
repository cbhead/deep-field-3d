#include "Boss/DFBoss.h"
#include "DFGameplayTags.h"
#include "Damage/DFDamageMath.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Boss.* — FDFBoss against B§2.4. The sim has no boss, so these pin the spec and the readings
// DFBoss.h states where the spec is silent; each assertion is chosen so that removing the rule it names
// changes the answer. Event sequences compare as one string (Describe) so a failure prints what happened.

namespace DFBossTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Tol = 1e-4f;

	FDFBossAttackRow Attack(const TCHAR* Id, float IntervalSeconds, float Radius, float Damage, float KnockbackMeters, float StaggerSeconds, float TelegraphSeconds)
	{
		FDFBossAttackRow A;
		A.Id = Id;
		A.IntervalSeconds = IntervalSeconds;
		A.Radius = Radius;
		A.Damage = Damage;
		A.KnockbackMeters = KnockbackMeters;
		A.StaggerSeconds = StaggerSeconds;
		A.TelegraphSeconds = TelegraphSeconds;
		return A;
	}

	FDFBossSiegeRow Siege(float Dps, float Reach)
	{
		FDFBossSiegeRow S;
		S.Dps = Dps;
		S.Reach = Reach;
		return S;
	}

	/**
	 * Frame01 with B§2.4's numbers where it gives them: 1200 hp, 6 plates x 40, shred x2 on plates; P1 at
	 * 100-66 % with a 180 deg front arc at x0.2, vents x2.0, Sweep every 12 s (6 m, knock 5 m, stagger
	 * 0.5 s, 1 s telegraph); P2 at 66-33 %, plates off, flat armour 0, siege 30 dps r8, 4 motes every
	 * 15 s up to 12, Slam r5 for 25; P3 at 33-0 %, speed x1.4, Stomp every 10 s (r5, 30, knock 6 m,
	 * hero stagger 0.25 s per B§1.6), vents x2.5. Where it gives none the fixture picks one, and no test
	 * depends on the pick being right, only on it being what is written here: P1's flat armour 3, Slam
	 * every 9 s, Slam's and Stomp's 1 s telegraphs, Sweep's damage 0.
	 */
	FDFBossTables Frame01()
	{
		FDFBossTables T;
		T.PhaseIds = { TEXT("armoured"), TEXT("exposed"), TEXT("enraged") };

		FDFBossPhaseRow Armoured;
		Armoured.HpFrom = 1.f;
		Armoured.HpTo = 0.66f;
		Armoured.FrontArcDeg = 180.f;
		Armoured.FrontArcFactor = 0.2f;
		Armoured.FlatArmor = 3.f;
		Armoured.Attack = Attack(TEXT("sweep"), 12.f, 6.f, 0.f, 5.f, 0.5f, 1.f);
		Armoured.Siege = Siege(30.f, 0.f);
		Armoured.WeakPointFactor = 2.f;
		Armoured.bPlatesHeld = true;

		FDFBossPhaseRow Exposed;
		Exposed.HpFrom = 0.66f;
		Exposed.HpTo = 0.33f;
		Exposed.Attack = Attack(TEXT("slam"), 9.f, 5.f, 25.f, 0.f, 0.f, 1.f);
		Exposed.Spawns.EnemyId = TEXT("mote");
		Exposed.Spawns.Count = 4;
		Exposed.Spawns.MaxAlive = 12;
		Exposed.Spawns.IntervalSeconds = 15.f;
		Exposed.Siege = Siege(30.f, 8.f);
		Exposed.WeakPointFactor = 2.f;

		FDFBossPhaseRow Enraged;
		Enraged.HpFrom = 0.33f;
		Enraged.HpTo = 0.f;
		Enraged.SpeedFactor = 1.4f;
		Enraged.Attack = Attack(TEXT("stomp"), 10.f, 5.f, 30.f, 6.f, 0.25f, 1.f);
		Enraged.Siege = Siege(30.f, 0.f);
		Enraged.WeakPointFactor = 2.5f;

		T.Phases = { Armoured, Exposed, Enraged };
		T.Body.RawHp = 1200.f;
		T.Body.PlateCount = 6;
		T.Body.PlateHp = 40.f;
		T.Body.PlateShredFactor = 2.f;
		return T;
	}

	const TCHAR* KindName(EDFBossEventKind Kind)
	{
		switch (Kind)
		{
		case EDFBossEventKind::PhaseEntered:      return TEXT("PhaseEntered");
		case EDFBossEventKind::PlatesDropped:     return TEXT("PlatesDropped");
		case EDFBossEventKind::PlateRemoved:      return TEXT("PlateRemoved");
		case EDFBossEventKind::AttackTelegraphed: return TEXT("AttackTelegraphed");
		case EDFBossEventKind::AttackCancelled:   return TEXT("AttackCancelled");
		case EDFBossEventKind::AttackLanded:      return TEXT("AttackLanded");
		case EDFBossEventKind::BroodVented:       return TEXT("BroodVented");
		case EDFBossEventKind::Died:              return TEXT("Died");
		}
		return TEXT("?");
	}

	/** "Kind(phase)#index xcount :detail", space-separated. Details are lower-cased: an FName keeps the casing
	 *  it was first registered with, and the tag DF.Enemy.Mote registers "Mote" before any test says "mote". */
	FString Describe(TConstArrayView<FDFBossEvent> Events)
	{
		TArray<FString> Parts;
		for (const FDFBossEvent& E : Events)
		{
			FString Part = FString::Printf(TEXT("%s(%d)"), KindName(E.Kind), E.Phase);
			if (E.Index != INDEX_NONE)
			{
				Part += FString::Printf(TEXT("#%d"), E.Index);
			}
			if (E.Count > 0)
			{
				Part += FString::Printf(TEXT("x%d"), E.Count);
			}
			if (!E.Detail.IsNone())
			{
				Part += TEXT(":") + E.Detail.ToString().ToLower();
			}
			Parts.Add(Part);
		}
		return FString::Join(Parts, TEXT(" "));
	}

	TArray<FDFBossEvent> OfKind(TConstArrayView<FDFBossEvent> Events, EDFBossEventKind Kind)
	{
		TArray<FDFBossEvent> Out;
		for (const FDFBossEvent& E : Events)
		{
			if (E.Kind == Kind)
			{
				Out.Add(E);
			}
		}
		return Out;
	}

	FDFBossHitTarget Plate(int32 Index)
	{
		FDFBossHitTarget T;
		T.PlateIndex = Index;
		return T;
	}

	/** Straight into the body's pool, without the damage math: the pools and phases are what most tests are about. */
	float HitBody(FDFBossState& S, const FDFBossTables& T, float Amount, TArray<FDFBossEvent>& Out)
	{
		return FDFBoss::ApplyDamage(S, T, FDFBossHitTarget(), Amount, /*bTargetShredded*/ false, Out);
	}

	/** A fresh boss already in Exposed (1200 - 408 = 792 hp = 66 %), its events discarded. */
	FDFBossState Exposed(const FDFBossTables& T)
	{
		FDFBossState S;
		FDFBoss::Begin(S, T, 1.f);
		TArray<FDFBossEvent> Ignored;
		HitBody(S, T, 408.f, Ignored);
		return S;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossValidateTest, "DF.Unit.Boss.ValidateRejectsBadTables", DFBossTest::Flags)
bool FDFBossValidateTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;

	FString Error;
	const bool bFixtureValid = Frame01().Validate(Error);
	TestTrue(*FString::Printf(TEXT("the Frame01 fixture validates (%s)"), *Error), bFixtureValid);

	auto Refuses = [this](const TCHAR* What, TFunctionRef<void(FDFBossTables&)> Break, const TCHAR* Says)
	{
		FDFBossTables T = Frame01();
		Break(T);
		FString Why;
		TestFalse(*FString::Printf(TEXT("%s is refused"), What), T.Validate(Why));
		TestTrue(*FString::Printf(TEXT("%s: the error says so (\"%s\")"), What, *Why), Why.Contains(Says));
	};

	Refuses(TEXT("a first phase below full hp"), [](FDFBossTables& T) { T.Phases[0].HpFrom = 0.9f; }, TEXT("must start at full hp"));
	Refuses(TEXT("a gap between phases"), [](FDFBossTables& T) { T.Phases[1].HpFrom = 0.65f; }, TEXT("but the next phase starts at"));
	Refuses(TEXT("a band that runs upward"), [](FDFBossTables& T) { T.Phases[2].HpTo = 0.5f; }, TEXT("must run downward"));
	Refuses(TEXT("a last phase that stops short of 0"), [](FDFBossTables& T) { T.Phases[2].HpTo = 0.1f; }, TEXT("must end at 0"));
	Refuses(TEXT("fewer ids than phases"), [](FDFBossTables& T) { T.PhaseIds.SetNum(2); }, TEXT("phase ids for"));
	Refuses(TEXT("a duplicate id"), [](FDFBossTables& T) { T.PhaseIds[2] = TEXT("Armoured"); }, TEXT("duplicate id"));
	Refuses(TEXT("a telegraph longer than its interval"), [](FDFBossTables& T) { T.Phases[0].Attack.TelegraphSeconds = 13.f; }, TEXT("does not fit"));
	Refuses(TEXT("an attack with no id"), [](FDFBossTables& T) { T.Phases[1].Attack.Id = NAME_None; }, TEXT("an attack with no id"));
	Refuses(TEXT("a vent cap below one vent"), [](FDFBossTables& T) { T.Phases[1].Spawns.MaxAlive = 3; }, TEXT("less than one vent"));
	Refuses(TEXT("a vent with no enemy"), [](FDFBossTables& T) { T.Phases[1].Spawns.EnemyId = NAME_None; }, TEXT("no enemy id"));
	Refuses(TEXT("a negative vent count"), [](FDFBossTables& T) { T.Phases[0].Spawns.Count = -1; }, TEXT("Spawns.Count"));
	Refuses(TEXT("plates that come back"), [](FDFBossTables& T) { T.Phases[2].bPlatesHeld = true; }, TEXT("shed them"));
	Refuses(TEXT("a NaN speed"), [](FDFBossTables& T) { T.Phases[2].SpeedFactor = std::numeric_limits<float>::quiet_NaN(); }, TEXT("must be positive"));
	Refuses(TEXT("a body with no hp"), [](FDFBossTables& T) { T.Body.RawHp = 0.f; }, TEXT("RawHp"));
	Refuses(TEXT("plates with no hp"), [](FDFBossTables& T) { T.Body.PlateHp = 0.f; }, TEXT("PlateHp"));

	FDFBossTables NoPlates = Frame01();
	NoPlates.Body.PlateCount = 0;
	NoPlates.Body.PlateHp = 0.f;
	TestTrue(TEXT("a boss with no plates is a valid boss"), NoPlates.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossBeginTest, "DF.Unit.Boss.BeginScalesHpAndPlates", DFBossTest::Flags)
bool FDFBossBeginTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;
	const FDFBossTables T = Frame01();

	FDFBossState Unbegun;
	TestFalse(TEXT("not active before Begin"), Unbegun.IsActive());
	TestNull(TEXT("no phase before Begin"), FDFBoss::CurrentPhase(Unbegun, T));
	TestEqual(TEXT("speed factor 1 before Begin"), FDFBoss::SpeedFactor(Unbegun, T), 1.f, Tol);

	FDFBossState S;
	FDFBoss::Begin(S, T, 1.5f);
	TestEqual(TEXT("body = 1200 x the wave's hp scale"), S.MaxHp, 1800.f, Tol);
	TestEqual(TEXT("at full hp"), S.Hp, 1800.f, Tol);
	TestEqual(TEXT("plates ride the same curve: 40 x 1.5"), S.PlateMaxHp, 60.f, Tol);
	TestEqual(TEXT("six plates"), S.PlateHp.Num(), 6);
	TestEqual(TEXT("every plate on"), S.PlatesHeld(), 6);
	for (const float EachPlateHp : S.PlateHp)
	{
		TestEqual(TEXT("each plate at full"), EachPlateHp, 60.f, Tol);
	}
	TestEqual(TEXT("first phase"), S.PhaseIndex, 0);
	TestTrue(TEXT("active"), S.IsActive());
	TestEqual(TEXT("the boss bar is full"), S.HpFrac(), 1.f, Tol);

	TestEqual(TEXT("phase 0 is DF.Boss.Phase.Armoured"), FDFBoss::PhaseTag(T, 0), FGameplayTag(DFTags::Boss_Phase_Armoured));
	TestEqual(TEXT("phase 1 is DF.Boss.Phase.Exposed"), FDFBoss::PhaseTag(T, 1), FGameplayTag(DFTags::Boss_Phase_Exposed));
	TestEqual(TEXT("phase 2 is DF.Boss.Phase.Enraged"), FDFBoss::PhaseTag(T, 2), FGameplayTag(DFTags::Boss_Phase_Enraged));
	TestFalse(TEXT("no tag past the last phase"), FDFBoss::PhaseTag(T, 3).IsValid());

	// A state reused for the next boss (an endless lap) starts clean: a dead boss is alive again, plates back on.
	TArray<FDFBossEvent> Events;
	HitBody(S, T, 5000.f, Events);
	TestFalse(TEXT("killed"), S.IsActive());
	FDFBoss::Begin(S, T, 1.f);
	TestTrue(TEXT("Begin revives a used state"), S.IsActive());
	TestEqual(TEXT("at the new scale's full hp"), S.Hp, 1200.f, Tol);
	TestEqual(TEXT("plates back on"), S.PlatesHeld(), 6);
	TestEqual(TEXT("in the first phase again"), S.PhaseIndex, 0);

	// A table whose first phase holds no plates starts with them off, and the vents open.
	FDFBossTables Bare = Frame01();
	Bare.Phases[0].bPlatesHeld = false;
	FString Error;
	TestTrue(TEXT("a plateless first phase validates"), Bare.Validate(Error));
	FDFBoss::Begin(S, Bare, 1.f);
	TestEqual(TEXT("no plates on"), S.PlatesHeld(), 0);
	TestEqual(TEXT("so a plate hit reaches the vent at x2.0"), FDFBoss::ResolveHit(S, Bare, EDFBossZone::Plate, 0).WeakPointFactor, 2.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossPhasesTest, "DF.Unit.Boss.PhasesFollowBodyHpOnly", DFBossTest::Flags)
bool FDFBossPhasesTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;
	const FDFBossTables T = Frame01();

	// Strip every plate first: 240 hp of damage, and the phase must not move.
	FDFBossState S;
	FDFBoss::Begin(S, T, 1.f);
	TArray<FDFBossEvent> Events;
	for (int32 i = 0; i < 6; ++i)
	{
		TestEqual(TEXT("a plate takes its 40"), FDFBoss::ApplyDamage(S, T, Plate(i), 40.f, false, Events), 40.f, Tol);
	}
	TestEqual(TEXT("each plate says it broke"), Describe(Events),
		FString(TEXT("PlateRemoved(0)#0 PlateRemoved(0)#1 PlateRemoved(0)#2 PlateRemoved(0)#3 PlateRemoved(0)#4 PlateRemoved(0)#5")));
	TestEqual(TEXT("plate damage never reaches the body"), S.Hp, 1200.f, Tol);
	TestEqual(TEXT("so the phase has not moved"), S.PhaseIndex, 0);

	// 793 hp is 66.08 %: still Armoured. 792 is exactly 66 %: Exposed — reaching HpFrom enters the phase.
	Events.Reset();
	TestEqual(TEXT("the body takes 407"), HitBody(S, T, 407.f, Events), 407.f, Tol);
	TestEqual(TEXT("one hp above the line is still Armoured"), S.PhaseIndex, 0);
	TestEqual(TEXT("and says nothing"), Events.Num(), 0);
	HitBody(S, T, 1.f, Events);
	TestEqual(TEXT("exactly 66 % enters Exposed"), S.PhaseIndex, 1);
	TestEqual(TEXT("with no plates left, nothing drops"), Describe(Events), FString(TEXT("PhaseEntered(1)")));

	// With every plate still on, the same line sheds all six.
	FDFBossState Plated;
	FDFBoss::Begin(Plated, T, 1.f);
	Events.Reset();
	HitBody(Plated, T, 408.f, Events);
	TestEqual(TEXT("Exposed sheds the plates"), Describe(Events), FString(TEXT("PhaseEntered(1) PlatesDropped(1)x6")));
	TestEqual(TEXT("none held"), Plated.PlatesHeld(), 0);
	for (const float PlateHp : Plated.PlateHp)
	{
		TestEqual(TEXT("each plate at 0"), PlateHp, 0.f, Tol);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossCrossingTest, "DF.Unit.Boss.OneHitCrossesPhasesInOrder", DFBossTest::Flags)
bool FDFBossCrossingTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;
	const FDFBossTables T = Frame01();

	FDFBossState S;
	FDFBoss::Begin(S, T, 1.f);
	TArray<FDFBossEvent> Events;
	TestEqual(TEXT("1000 lands"), HitBody(S, T, 1000.f, Events), 1000.f, Tol);
	TestEqual(TEXT("both lines crossed, in order, the plates dropping on the way"), Describe(Events),
		FString(TEXT("PhaseEntered(1) PlatesDropped(1)x6 PhaseEntered(2)")));
	TestEqual(TEXT("Enraged"), S.PhaseIndex, 2);
	TestTrue(TEXT("still alive at 200"), S.IsActive());

	// The killing blow still passes through every phase it crosses (B§1.7 pays per phase), then dies.
	FDFBoss::Begin(S, T, 1.f);
	Events.Reset();
	TestEqual(TEXT("an overkill takes only what was there"), HitBody(S, T, 5000.f, Events), 1200.f, Tol);
	TestEqual(TEXT("every phase, then death"), Describe(Events),
		FString(TEXT("PhaseEntered(1) PlatesDropped(1)x6 PhaseEntered(2) Died(2)")));
	TestTrue(TEXT("dead"), S.bDead);
	TestEqual(TEXT("at 0"), S.Hp, 0.f, Tol);

	// A dead boss is done: nothing lands, nothing heals, nothing ticks.
	Events.Reset();
	TestEqual(TEXT("no body damage after death"), HitBody(S, T, 10.f, Events), 0.f, Tol);
	TestEqual(TEXT("no heal after death"), FDFBoss::Heal(S, 100.f), 0.f, Tol);
	FDFBoss::Advance(S, T, 30.f, 0, Events);
	TestEqual(TEXT("and no events"), Events.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossPlatesTest, "DF.Unit.Boss.PlatesCoverVentsUntilTheyBreak", DFBossTest::Flags)
bool FDFBossPlatesTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;
	const FDFBossTables T = Frame01();
	FDFBossState S;
	FDFBoss::Begin(S, T, 1.f);
	TArray<FDFBossEvent> Events;

	const FDFBossHitTarget Body = FDFBoss::ResolveHit(S, T, EDFBossZone::Body, INDEX_NONE);
	TestFalse(TEXT("a body hit is the body"), Body.IsPlate());
	TestEqual(TEXT("at no weak-point factor"), Body.WeakPointFactor, 1.f, Tol);
	TestEqual(TEXT("a plate that is on takes its own hit"), FDFBoss::ResolveHit(S, T, EDFBossZone::Plate, 2).PlateIndex, 2);
	TestEqual(TEXT("and covers the vent under it"), FDFBoss::ResolveHit(S, T, EDFBossZone::Vent, 2).PlateIndex, 2);
	TestFalse(TEXT("a zone past the plates is the body"), FDFBoss::ResolveHit(S, T, EDFBossZone::Plate, 6).IsPlate());
	TestEqual(TEXT("and earns no bonus"), FDFBoss::ResolveHit(S, T, EDFBossZone::Vent, 6).WeakPointFactor, 1.f, Tol);
	TestEqual(TEXT("nor does a negative one"), FDFBoss::ResolveHit(S, T, EDFBossZone::Vent, -1).WeakPointFactor, 1.f, Tol);

	// Break plate 2: the hit that breaks it is spent on it.
	TestEqual(TEXT("25 into plate 2"), FDFBoss::ApplyDamage(S, T, Plate(2), 25.f, false, Events), 25.f, Tol);
	TestEqual(TEXT("15 left"), S.PlateHp[2], 15.f, Tol);
	TestEqual(TEXT("nothing broke yet"), Events.Num(), 0);
	TestEqual(TEXT("an overkill on a plate takes only the plate's 15"), FDFBoss::ApplyDamage(S, T, Plate(2), 100.f, false, Events), 15.f, Tol);
	TestEqual(TEXT("it broke"), Describe(Events), FString(TEXT("PlateRemoved(0)#2")));
	TestEqual(TEXT("and nothing carried through to the body"), S.Hp, 1200.f, Tol);
	Events.Reset();
	TestEqual(TEXT("a plate already off takes nothing"), FDFBoss::ApplyDamage(S, T, Plate(2), 10.f, false, Events), 0.f, Tol);
	TestEqual(TEXT("and does not break twice"), Events.Num(), 0);

	const FDFBossHitTarget Vent = FDFBoss::ResolveHit(S, T, EDFBossZone::Vent, 2);
	TestFalse(TEXT("the open vent reaches the body"), Vent.IsPlate());
	TestEqual(TEXT("at Armoured's x2.0"), Vent.WeakPointFactor, 2.f, Tol);
	TestEqual(TEXT("so does a shot at where the plate was"), FDFBoss::ResolveHit(S, T, EDFBossZone::Plate, 2).WeakPointFactor, 2.f, Tol);
	TestEqual(TEXT("the plate beside it is still a plate"), FDFBoss::ResolveHit(S, T, EDFBossZone::Plate, 3).PlateIndex, 3);

	// Shred doubles what a plate takes.
	TestEqual(TEXT("15 under shred is 30 to a plate"), FDFBoss::ApplyDamage(S, T, Plate(5), 15.f, true, Events), 30.f, Tol);
	TestEqual(TEXT("10 left"), S.PlateHp[5], 10.f, Tol);
	TestEqual(TEXT("unshredded 15 takes the last 10"), FDFBoss::ApplyDamage(S, T, Plate(5), 15.f, false, Events), 10.f, Tol);
	TestEqual(TEXT("plate 5 broke"), Describe(Events), FString(TEXT("PlateRemoved(0)#5")));

	// Down to 33 % in one hit: the four plates still on drop with Exposed, and Enraged's vents are x2.5.
	Events.Reset();
	HitBody(S, T, 804.f, Events);
	TestEqual(TEXT("exactly 33 % enters Enraged"), Describe(Events), FString(TEXT("PhaseEntered(1) PlatesDropped(1)x4 PhaseEntered(2)")));
	TestEqual(TEXT("an Enraged vent is x2.5"), FDFBoss::ResolveHit(S, T, EDFBossZone::Vent, 0).WeakPointFactor, 2.5f, Tol);
	TestEqual(TEXT("a dropped plate's place is a vent too"), FDFBoss::ResolveHit(S, T, EDFBossZone::Plate, 3).WeakPointFactor, 2.5f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossDamageMathTest, "DF.Unit.Boss.HitsPricedThroughDamageMath", DFBossTest::Flags)
bool FDFBossDamageMathTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;
	const FDFBossTables T = Frame01();

	/** A 10-damage hit priced the way the execution will: the caller's source half, ProfileHit's target half,
	 *  FDFDamageMath's order. FlatArmorAttribute stands in for the FlatArmor attribute the body keeps. */
	auto Price = [](const FDFBossTables& Tables, const FDFBossState& S, const FDFBossHitTarget& Target, float HitDot, float FlatArmorAttribute, float UnarmoredBonusFactor = 1.f)
	{
		FDFDamageInput In;
		In.BaseDamage = 10.f;
		In.bHasHitDirection = true;
		In.HitDot = HitDot;
		In.FlatArmor = FlatArmorAttribute;
		In.UnarmoredBonusFactor = UnarmoredBonusFactor;
		FDFBoss::ProfileHit(S, Tables, Target, In);
		return FDFDamageMath::Compute(In).Damage;
	};
	constexpr float Front = 1.f;
	constexpr float Rear = -1.f;

	FDFBossState S;
	FDFBoss::Begin(S, T, 1.f);
	const FDFArmorProfile Armoured = FDFBoss::ArmorProfile(S, T);
	TestEqual(TEXT("Armoured: a 180 deg front arc"), Armoured.FrontArmorArcDegrees, 180.f, Tol);
	TestEqual(TEXT("at x0.2"), Armoured.FrontArmorFactor, 0.2f, Tol);
	TestEqual(TEXT("and no rear factor"), Armoured.RearWeakFactor, 1.f, Tol);
	const float Flat = FDFBoss::FlatArmor(S, T);
	TestEqual(TEXT("Armoured's flat armour"), Flat, 3.f, Tol);

	const FDFBossHitTarget Body;
	TestEqual(TEXT("body from the front: 10 x 0.2 - 3, floored at 0.5"), Price(T, S, Body, Front, Flat), 0.5f, Tol);
	TestEqual(TEXT("body from behind: 10 - 3"), Price(T, S, Body, Rear, Flat), 7.f, Tol);
	TestEqual(TEXT("hollow point gets nothing from an armoured body"), Price(T, S, Body, Rear, Flat, 1.5f), 7.f, Tol);

	// A plate is armour: no arc, no flat armour (ProfileHit clears the attribute's 3), and still armoured.
	const FDFBossHitTarget PlateOne = FDFBoss::ResolveHit(S, T, EDFBossZone::Plate, 1);
	TestEqual(TEXT("a plate from the front takes the full 10"), Price(T, S, PlateOne, Front, Flat), 10.f, Tol);
	TestEqual(TEXT("hollow point gets nothing from a plate"), Price(T, S, PlateOne, Front, Flat, 1.5f), 10.f, Tol);

	// Break plate 0: its vent is x2.0 and the arc still applies to it, because both are true of the hit.
	TArray<FDFBossEvent> Events;
	FDFBoss::ApplyDamage(S, T, Plate(0), 40.f, false, Events);
	const FDFBossHitTarget Vent = FDFBoss::ResolveHit(S, T, EDFBossZone::Vent, 0);
	TestEqual(TEXT("open vent from behind: 10 x 2.0 - 3"), Price(T, S, Vent, Rear, Flat), 17.f, Tol);
	TestEqual(TEXT("open vent from the front: 10 x 2.0 x 0.2 - 3"), Price(T, S, Vent, Front, Flat), 1.f, Tol);

	// Exposed: the arc and the flat armour are gone, and the body is now unarmoured.
	HitBody(S, T, 408.f, Events);
	TestEqual(TEXT("Exposed"), S.PhaseIndex, 1);
	TestEqual(TEXT("Exposed has no arc"), FDFBoss::ArmorProfile(S, T).FrontArmorArcDegrees, 0.f, Tol);
	const float ExposedFlat = FDFBoss::FlatArmor(S, T);
	TestEqual(TEXT("and no flat armour"), ExposedFlat, 0.f, Tol);
	TestEqual(TEXT("so a front hit is the full 10"), Price(T, S, Body, Front, ExposedFlat), 10.f, Tol);
	TestEqual(TEXT("hollow point's x1.5 applies to the unarmoured body"), Price(T, S, Body, Front, ExposedFlat, 1.5f), 15.f, Tol);
	TestEqual(TEXT("a vent from the front: 10 x 2.0"), Price(T, S, FDFBoss::ResolveHit(S, T, EDFBossZone::Vent, 4), Front, ExposedFlat), 20.f, Tol);

	// Enraged walks faster.
	HitBody(S, T, 396.f, Events);
	TestEqual(TEXT("Enraged"), S.PhaseIndex, 2);
	TestEqual(TEXT("Enraged walks at x1.4"), FDFBoss::SpeedFactor(S, T), 1.4f, Tol);
	TestEqual(TEXT("Enraged vents: 10 x 2.5"), Price(T, S, FDFBoss::ResolveHit(S, T, EDFBossZone::Vent, 4), Front, 0.f), 25.f, Tol);

	// Armoured is armoured by its arc alone, so the row armour only shows in a phase with flat armour and no
	// arc: there it is the phase's own number that makes the body armoured, whatever shred has done since.
	FDFBossTables Hard = Frame01();
	Hard.Phases[2].FlatArmor = 2.f;
	FDFBossState H;
	FDFBoss::Begin(H, Hard, 1.f);
	HitBody(H, Hard, 804.f, Events);
	TestEqual(TEXT("an Enraged boss with flat armour 2"), FDFBoss::FlatArmor(H, Hard), 2.f, Tol);
	TestEqual(TEXT("is armoured by it: hollow point gets nothing, 10 - 2"), Price(Hard, H, Body, Front, 2.f, 1.5f), 8.f, Tol);
	TestEqual(TEXT("even shredded to 0, the row still says armoured"), Price(Hard, H, Body, Front, 0.f, 1.5f), 10.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossAttackTest, "DF.Unit.Boss.AttackTelegraphsThenLands", DFBossTest::Flags)
bool FDFBossAttackTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;
	const FDFBossTables T = Frame01();
	TArray<FDFBossEvent> Events;

	// Sweep: every 12 s, warned 1 s before, counted from each landing.
	FDFBossState S;
	FDFBoss::Begin(S, T, 1.f);
	FDFBoss::Advance(S, T, 10.5f, 0, Events);
	TestEqual(TEXT("nothing in the first 10.5 s"), Events.Num(), 0);
	FDFBoss::Advance(S, T, 0.5f, 0, Events);
	TestEqual(TEXT("the warning at 11 s"), Describe(Events), FString(TEXT("AttackTelegraphed(0):sweep")));
	TestEqual(TEXT("at the end of that step"), Events.Num() ? Events[0].SecondsIntoStep : -1.f, 0.5f, Tol);
	TestTrue(TEXT("telegraphing"), S.bTelegraphing);
	Events.Reset();
	FDFBoss::Advance(S, T, 1.f, 0, Events);
	TestEqual(TEXT("the sweep lands at 12 s"), Describe(Events), FString(TEXT("AttackLanded(0):sweep")));
	TestEqual(TEXT("at the end of that step"), Events.Num() ? Events[0].SecondsIntoStep : -1.f, 1.f, Tol);
	Events.Reset();
	FDFBoss::Advance(S, T, 11.5f, 0, Events);
	TestEqual(TEXT("the next warning is 11 s after the landing"), Describe(Events), FString(TEXT("AttackTelegraphed(0):sweep")));
	TestEqual(TEXT("11 s into the step"), Events.Num() ? Events[0].SecondsIntoStep : -1.f, 11.f, Tol);
	Events.Reset();
	FDFBoss::Advance(S, T, 0.5f, 0, Events);
	TestEqual(TEXT("and it lands at 24 s"), Describe(Events), FString(TEXT("AttackLanded(0):sweep")));

	// A phase change ends the warning and restarts the clock under the new phase's attack.
	FDFBoss::Begin(S, T, 1.f);
	Events.Reset();
	FDFBoss::Advance(S, T, 11.5f, 0, Events);
	Events.Reset();
	HitBody(S, T, 408.f, Events);
	TestEqual(TEXT("Exposed cancels the Sweep it interrupted"), Describe(Events),
		FString(TEXT("AttackCancelled(0):sweep PhaseEntered(1) PlatesDropped(1)x6")));
	Events.Reset();
	FDFBoss::Advance(S, T, 7.5f, 0, Events);
	TestEqual(TEXT("the Slam clock starts at the phase change, not where the Sweep's stood"), Events.Num(), 0);
	FDFBoss::Advance(S, T, 0.75f, 0, Events);
	TestEqual(TEXT("Slam warns 8 s into Exposed"), Describe(Events), FString(TEXT("AttackTelegraphed(1):slam")));
	TestEqual(TEXT("half a second into the step"), Events.Num() ? Events[0].SecondsIntoStep : -1.f, 0.5f, Tol);

	// Death ends a warning too.
	FDFBoss::Begin(S, T, 1.f);
	HitBody(S, T, 804.f, Events);
	Events.Reset();
	FDFBoss::Advance(S, T, 9.25f, 0, Events);
	TestEqual(TEXT("Stomp warns 9 s into Enraged"), Describe(Events), FString(TEXT("AttackTelegraphed(2):stomp")));
	Events.Reset();
	HitBody(S, T, 1000.f, Events);
	TestEqual(TEXT("the killing blow cancels it"), Describe(Events), FString(TEXT("AttackCancelled(2):stomp Died(2)")));

	// At a tie the attack comes first, and a zero telegraph warns and lands in the same instant.
	FDFBossTables Tie;
	Tie.PhaseIds = { TEXT("only") };
	FDFBossPhaseRow Only;
	Only.HpFrom = 1.f;
	Only.HpTo = 0.f;
	Only.Attack = Attack(TEXT("burst"), 5.f, 3.f, 10.f, 0.f, 0.f, 0.f);
	Only.Spawns.EnemyId = TEXT("mote");
	Only.Spawns.Count = 1;
	Only.Spawns.MaxAlive = 5;
	Only.Spawns.IntervalSeconds = 5.f;
	Tie.Phases = { Only };
	Tie.Body.RawHp = 100.f;
	FString Error;
	const bool bTieValid = Tie.Validate(Error);
	TestTrue(*FString::Printf(TEXT("the tie table validates (%s)"), *Error), bTieValid);
	FDFBoss::Begin(S, Tie, 1.f);
	Events.Reset();
	FDFBoss::Advance(S, Tie, 5.f, 0, Events);
	TestEqual(TEXT("warn, land, then vent"), Describe(Events), FString(TEXT("AttackTelegraphed(0):burst AttackLanded(0):burst BroodVented(0)x1:mote")));
	for (const FDFBossEvent& E : Events)
	{
		TestEqual(TEXT("all at 5 s"), E.SecondsIntoStep, 5.f, Tol);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossFrameRateTest, "DF.Unit.Boss.SameEventsAtAnyFrameRate", DFBossTest::Flags)
bool FDFBossFrameRateTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;
	const FDFBossTables T = Frame01();

	struct FTimed
	{
		FString What;
		double At = 0.0;
	};

	// 39.5 s of Exposed in steps of StepSeconds, feeding back the brood as a caller would.
	auto Run = [&T](float StepSeconds)
	{
		FDFBossState S = Exposed(T);
		TArray<FTimed> Seen;
		int32 Brood = 0;
		double Elapsed = 0.0;
		constexpr double Total = 39.5;
		while (Elapsed < Total - 1e-9)
		{
			const float Step = static_cast<float>(FMath::Min(static_cast<double>(StepSeconds), Total - Elapsed));
			TArray<FDFBossEvent> Events;
			FDFBoss::Advance(S, T, Step, Brood, Events);
			for (const FDFBossEvent& E : Events)
			{
				Seen.Add({ Describe(MakeArrayView(&E, 1)), Elapsed + E.SecondsIntoStep });
				Brood += E.Kind == EDFBossEventKind::BroodVented ? E.Count : 0;
			}
			Elapsed += Step;
		}
		return Seen;
	};

	// Slam every 9 s warned at 8, 17, 26, 35; lands at 9, 18, 27, 36; vents at 15 and 30. No two at once.
	const TArray<FTimed> OneStep = Run(39.5f);
	const TCHAR* Warn = TEXT("AttackTelegraphed(1):slam");
	const TCHAR* Land = TEXT("AttackLanded(1):slam");
	const TCHAR* Vent = TEXT("BroodVented(1)x4:mote");
	const TArray<FTimed> Expected = {
		{ Warn, 8.0 }, { Land, 9.0 }, { Vent, 15.0 }, { Warn, 17.0 }, { Land, 18.0 },
		{ Warn, 26.0 }, { Land, 27.0 }, { Vent, 30.0 }, { Warn, 35.0 }, { Land, 36.0 },
	};

	auto Matches = [this](const TCHAR* Label, const TArray<FTimed>& Got, const TArray<FTimed>& Want)
	{
		TestEqual(*FString::Printf(TEXT("%s: event count"), Label), Got.Num(), Want.Num());
		for (int32 i = 0; i < FMath::Min(Got.Num(), Want.Num()); ++i)
		{
			TestEqual(*FString::Printf(TEXT("%s: event %d"), Label, i), Got[i].What, Want[i].What);
			TestEqual(*FString::Printf(TEXT("%s: event %d at %.3f s"), Label, i, Want[i].At), static_cast<float>(Got[i].At), static_cast<float>(Want[i].At), 1e-3f);
		}
	};
	Matches(TEXT("one 39.5 s step"), OneStep, Expected);
	Matches(TEXT("0.1 s steps"), Run(0.1f), Expected);
	Matches(TEXT("60 Hz steps"), Run(1.f / 60.f), Expected);
	Matches(TEXT("7 s steps"), Run(7.f), Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossBroodTest, "DF.Unit.Boss.BroodVentHoldsItsCap", DFBossTest::Flags)
bool FDFBossBroodTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;
	const FDFBossTables T = Frame01();
	auto Vents = [](const TArray<FDFBossEvent>& Events) { return Describe(OfKind(Events, EDFBossEventKind::BroodVented)); };

	// Armoured has no vent.
	FDFBossState S;
	FDFBoss::Begin(S, T, 1.f);
	TArray<FDFBossEvent> Events;
	FDFBoss::Advance(S, T, 61.f, 0, Events);
	TestEqual(TEXT("Armoured never vents"), Vents(Events), FString());

	// One long frame of Exposed: vents at 15, 30, 45 take the brood to 12; the one at 60 has no room.
	S = Exposed(T);
	Events.Reset();
	FDFBoss::Advance(S, T, 61.f, 0, Events);
	TestEqual(TEXT("three vents of four, then the cap"), Vents(Events), FString(TEXT("BroodVented(1)x4:mote BroodVented(1)x4:mote BroodVented(1)x4:mote")));
	const TArray<FDFBossEvent> Vented = OfKind(Events, EDFBossEventKind::BroodVented);
	for (int32 i = 0; i < Vented.Num(); ++i)
	{
		TestEqual(TEXT("every 15 s"), Vented[i].SecondsIntoStep, 15.f * static_cast<float>(i + 1), Tol);
	}
	// The capped vent at 60 still reset its clock: nothing at 61..74.5, then a vent at 75.
	Events.Reset();
	FDFBoss::Advance(S, T, 13.5f, 0, Events);
	TestEqual(TEXT("a vent at the cap still restarts the clock"), Vents(Events), FString());
	FDFBoss::Advance(S, T, 1.f, 0, Events);
	TestEqual(TEXT("the next vent is 15 s after it"), Vents(Events), FString(TEXT("BroodVented(1)x4:mote")));

	// Bodies vented earlier in the same call count against the cap: 6 alive, +4 at 15, only 2 fit at 30.
	S = Exposed(T);
	Events.Reset();
	FDFBoss::Advance(S, T, 31.f, 6, Events);
	TestEqual(TEXT("4 then the 2 that fit"), Vents(Events), FString(TEXT("BroodVented(1)x4:mote BroodVented(1)x2:mote")));

	// The caller's count is the cap's too.
	S = Exposed(T);
	Events.Reset();
	FDFBoss::Advance(S, T, 16.f, 10, Events);
	TestEqual(TEXT("10 alive leaves room for 2"), Vents(Events), FString(TEXT("BroodVented(1)x2:mote")));
	S = Exposed(T);
	Events.Reset();
	FDFBoss::Advance(S, T, 16.f, 12, Events);
	TestEqual(TEXT("12 alive leaves none"), Vents(Events), FString());

	// A nonsense negative count is read as none alive, not as extra room.
	S = Exposed(T);
	Events.Reset();
	FDFBoss::Advance(S, T, 61.f, -5, Events);
	TestEqual(TEXT("-5 alive vents like 0 alive"), Vents(Events), FString(TEXT("BroodVented(1)x4:mote BroodVented(1)x4:mote BroodVented(1)x4:mote")));

	// Nothing for a step of no time.
	S = Exposed(T);
	Events.Reset();
	FDFBoss::Advance(S, T, 0.f, 0, Events);
	FDFBoss::Advance(S, T, -1.f, 0, Events);
	TestEqual(TEXT("no time, no events"), Events.Num(), 0);
	TestEqual(TEXT("and the clock did not move"), static_cast<float>(S.VentClock), 0.f, 1e-6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossHealTest, "DF.Unit.Boss.HealNeverRearms", DFBossTest::Flags)
bool FDFBossHealTest::RunTest(const FString& Parameters)
{
	using namespace DFBossTest;
	const FDFBossTables T = Frame01();
	FDFBossState S = Exposed(T);
	TArray<FDFBossEvent> Events;

	TestEqual(TEXT("a heal restores only what is missing"), FDFBoss::Heal(S, 1000.f), 408.f, Tol);
	TestEqual(TEXT("back to full"), S.HpFrac(), 1.f, Tol);
	TestEqual(TEXT("but still Exposed"), S.PhaseIndex, 1);
	TestEqual(TEXT("the plates stay off"), S.PlatesHeld(), 0);
	TestEqual(TEXT("so the arc stays down"), FDFBoss::ArmorProfile(S, T).FrontArmorArcDegrees, 0.f, Tol);
	TestEqual(TEXT("and a plate's place is still a vent"), FDFBoss::ResolveHit(S, T, EDFBossZone::Plate, 0).WeakPointFactor, 2.f, Tol);

	// Falling back through 66 % is not a second Exposed, and 33 % is Enraged with nothing left to drop.
	HitBody(S, T, 400.f, Events);
	TestEqual(TEXT("falling through 66 % again says nothing"), Events.Num(), 0);
	HitBody(S, T, 404.f, Events);
	TestEqual(TEXT("33 % is Enraged, once"), Describe(Events), FString(TEXT("PhaseEntered(2)")));

	TestEqual(TEXT("a zero heal is nothing"), FDFBoss::Heal(S, 0.f), 0.f, Tol);
	TestEqual(TEXT("a negative heal is nothing"), FDFBoss::Heal(S, -5.f), 0.f, Tol);
	TestEqual(TEXT("and hurt nobody"), S.Hp, 396.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFBossKillWindowTest, "DF.Unit.Boss.KillWindow", DFBossTest::Flags)
bool FDFBossKillWindowTest::RunTest(const FString& Parameters)
{
	const FDFBossKillWindow Gate;
	TestTrue(TEXT("55 % is inside (inclusive)"), Gate.Contains(FDFBossKillWindow::WalkFraction(55.f, 100.f)));
	TestTrue(TEXT("85 % is inside (inclusive)"), Gate.Contains(FDFBossKillWindow::WalkFraction(85.f, 100.f)));
	TestFalse(TEXT("54 % is too early"), Gate.Contains(FDFBossKillWindow::WalkFraction(54.f, 100.f)));
	TestFalse(TEXT("86 % is too late"), Gate.Contains(FDFBossKillWindow::WalkFraction(86.f, 100.f)));
	TestEqual(TEXT("a leak is the whole walk"), FDFBossKillWindow::WalkFraction(130.f, 100.f), 1.f, 1e-6f);
	TestFalse(TEXT("and fails the gate"), Gate.Contains(FDFBossKillWindow::WalkFraction(130.f, 100.f)));
	TestEqual(TEXT("a route of no length reads as a leak"), FDFBossKillWindow::WalkFraction(10.f, 0.f), 1.f, 1e-6f);
	TestEqual(TEXT("so does a NaN walk"), FDFBossKillWindow::WalkFraction(std::numeric_limits<float>::quiet_NaN(), 100.f), 1.f, 1e-6f);
	TestEqual(TEXT("a negative walk is the start"), FDFBossKillWindow::WalkFraction(-3.f, 100.f), 0.f, 1e-6f);

	FDFBossKillWindow Wide;
	Wide.MinFraction = 0.5f;
	Wide.MaxFraction = 0.9f;
	TestTrue(TEXT("the window is a dial: 86 % inside 50-90 %"), Wide.Contains(FDFBossKillWindow::WalkFraction(86.f, 100.f)));
	return true;
}

#endif
