// DF.Unit.World.* — the placeholder looks of the importer's actors (the spawn portal, the core, the
// lane strip): they exist, they are the colours PROGRAMME C§1 gives them, they face and stand where
// the wave needs them, and nothing in the game can touch them. The last is checked with real
// queries — every C16 trace channel, and the hero's and enemies' own channels as their movement
// sweeps them — against a blocking box that proves the same queries do hit. Run:
// unreal\deepfield.cmd test DF.Unit.World

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DFWorldCollision.h"
#include "Engine/World.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Testing/DFTestUtils.h"
#include "World/DFCore.h"
#include "Look/DFShapeLook.h"
#include "World/DFLaneGraphInfo.h"
#include "World/DFSocket.h"
#include "World/DFSpawnPortal.h"
#include "World/DFWorldLook.h"

namespace DFWorldLookTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	struct FChannel
	{
		ECollisionChannel Channel;
		const TCHAR* Name;
	};

	// C16's five trace channels, the hero's and enemies' object channels (what their movement sweeps
	// with), and the engine's own three a camera, a cursor or a pawn would use.
	const FChannel Channels[] = {
		{ DFCollision::Sight,       TEXT("DF_Sight") },
		{ DFCollision::Weapon,      TEXT("DF_Weapon") },
		{ DFCollision::Build,       TEXT("DF_Build") },
		{ DFCollision::Interact,    TEXT("DF_Interact") },
		{ DFCollision::LaneSurface, TEXT("DF_LaneSurface") },
		{ DFCollision::Hero,        TEXT("DF_Hero") },
		{ DFCollision::Enemy,       TEXT("DF_Enemy") },
		{ ECC_Visibility,           TEXT("Visibility") },
		{ ECC_Camera,               TEXT("Camera") },
		{ ECC_Pawn,                 TEXT("Pawn") },
	};

	/** Hits on Target by a line and a 30 cm sphere along each axis through Through, on Channel. */
	int32 HitsOn(UWorld* World, const UPrimitiveComponent* Target, const FVector& Through, ECollisionChannel Channel)
	{
		int32 Count = 0;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(DFWorldLookTest), /*bTraceComplex*/ true);
		for (const FVector& Axis : { FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0) })
		{
			const FVector Start = Through - Axis * 1000.0;
			const FVector End = Through + Axis * 1000.0;
			TArray<FHitResult> Hits;
			World->LineTraceMultiByChannel(Hits, Start, End, Channel, Params);
			for (const FHitResult& Hit : Hits)
			{
				Count += Hit.GetComponent() == Target ? 1 : 0;
			}
			Hits.Reset();
			World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeSphere(30.f), Params);
			for (const FHitResult& Hit : Hits)
			{
				Count += Hit.GetComponent() == Target ? 1 : 0;
			}
		}
		return Count;
	}

	/**
	 * The control: a box that blocks everything, somewhere out of the way. If the queries in HitsOn
	 * find it on every channel, a look part they do not find is inert, not merely missed.
	 */
	bool QueriesWork(FAutomationTestBase& Test, FDFTestWorld& World)
	{
		AActor* Wall = World.SpawnActor<AActor>(FTransform(FVector(0.0, 0.0, -5000.0)));
		if (!Test.TestNotNull(TEXT("control actor"), Wall))
		{
			return false;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(100.0));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Box->SetWorldLocation(FVector(0.0, 0.0, -5000.0));
		bool bAll = true;
		for (const FChannel& C : Channels)
		{
			bAll &= Test.TestTrue(FString::Printf(TEXT("control: a blocking box is found on %s"), C.Name),
				HitsOn(World.GetWorld(), Box, Box->GetComponentLocation(), C.Channel) > 0);
		}
		return bAll;
	}

	/** A look part is set dressing: nothing collides with it, overlaps it or navigates round it. */
	void ExpectInert(FAutomationTestBase& Test, UWorld* World, const UPrimitiveComponent* Part, const FString& What)
	{
		Test.TestTrue(What + TEXT(": NoCollision"), Part->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		Test.TestFalse(What + TEXT(": raises no overlap events"), Part->GetGenerateOverlapEvents());
		Test.TestFalse(What + TEXT(": never affects navigation"), Part->CanEverAffectNavigation());
		for (const FChannel& C : Channels)
		{
			Test.TestTrue(FString::Printf(TEXT("%s: ignores %s"), *What, C.Name), Part->GetCollisionResponseToChannel(C.Channel) == ECR_Ignore);
			Test.TestEqual(FString::Printf(TEXT("%s: a %s query through it finds nothing"), *What, C.Name),
				HitsOn(World, Part, Part->GetComponentLocation(), C.Channel), 0);
		}
	}

	UStaticMeshComponent* PartNamed(const AActor* Actor, const TCHAR* Name)
	{
		TArray<UStaticMeshComponent*> Parts;
		Actor->GetComponents(Parts);
		for (UStaticMeshComponent* Part : Parts)
		{
			if (Part->GetFName() == FName(Name))
			{
				return Part;
			}
		}
		return nullptr;
	}

	bool TintedAs(const UStaticMeshComponent* Part, const FLinearColor& Expected)
	{
		UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0));
		return Mid && Mid->HasAnyFlags(RF_Transient) && Mid->K2_GetVectorParameterValue(TEXT("Color")).Equals(Expected, 1e-4f);
	}

	/** Top and bottom (world Z) of a part made from a 1 m basic shape centred on its pivot. */
	double TopOf(const USceneComponent* Part)    { return Part->GetComponentLocation().Z + Part->GetComponentScale().Z * 50.0; }
	double BottomOf(const USceneComponent* Part) { return Part->GetComponentLocation().Z - Part->GetComponentScale().Z * 50.0; }

	void AddEdge(UDFLaneGraphAsset& Graph, const TCHAR* From, const TCHAR* To, EDFEnemyLayer Layer, EDFLaneEdgeKind Kind, const TArray<FVector>& Waypoints)
	{
		FDFLaneEdge& E = Graph.Edges.AddDefaulted_GetRef();
		E.Id = FName(*(FString(From) + TEXT("-") + To + (Layer == EDFEnemyLayer::Air ? TEXT("@air") : TEXT(""))));
		E.From = From;
		E.To = To;
		E.Layer = Layer;
		E.Kind = Kind;
		E.Waypoints = Waypoints;
	}

	/**
	 * westGate -> bend -> core on the ground, the first leg along +Y; an air lane airWest -> airCore
	 * that climbs before it leaves; a warp pad -> westGate. Enough for the portal's facing rule and
	 * the strip's ground-only rule.
	 */
	UDFLaneGraphAsset* MakeGraph()
	{
		UDFLaneGraphAsset* Graph = NewObject<UDFLaneGraphAsset>(GetTransientPackage());
		AddEdge(*Graph, TEXT("westGate"), TEXT("core"), EDFEnemyLayer::Ground, EDFLaneEdgeKind::Walk,
			{ FVector(0.0, 0.0, 0.0), FVector(0.0, 1000.0, 0.0), FVector(1500.0, 1000.0, 0.0) });
		AddEdge(*Graph, TEXT("airWest"), TEXT("airCore"), EDFEnemyLayer::Air, EDFLaneEdgeKind::Walk,
			{ FVector(0.0, 0.0, 900.0), FVector(0.0, 0.0, 1200.0), FVector(-2000.0, 0.0, 1200.0) });
		AddEdge(*Graph, TEXT("pad"), TEXT("westGate"), EDFEnemyLayer::Ground, EDFLaneEdgeKind::Warp,
			{ FVector(5000.0, 0.0, 0.0), FVector(0.0, 0.0, 0.0) });
		return Graph;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWorldPortalLookTest, "DF.Unit.World.PortalLook", DFWorldLookTest::Flags)
bool FDFWorldPortalLookTest::RunTest(const FString& Parameters)
{
	using namespace DFWorldLookTest;
	FDFTestWorld World;
	if (!QueriesWork(*this, World))
	{
		return false;
	}

	ADFSpawnPortal* Portal = World.SpawnActor<ADFSpawnPortal>();
	if (!TestNotNull(TEXT("portal"), Portal))
	{
		return false;
	}
	UStaticMeshComponent* PostLeft = PartNamed(Portal, TEXT("PostLeft"));
	UStaticMeshComponent* PostRight = PartNamed(Portal, TEXT("PostRight"));
	UStaticMeshComponent* Lintel = PartNamed(Portal, TEXT("Lintel"));
	if (!TestNotNull(TEXT("left post"), PostLeft) || !TestNotNull(TEXT("right post"), PostRight) || !TestNotNull(TEXT("lintel"), Lintel)
		|| !TestNotNull(TEXT("look root"), Portal->GetLookRoot()))
	{
		return false;
	}

	// The actor's own components are as they were: the static root the importer moves, with the look under it.
	TestTrue(TEXT("the root is still the static scene root"), Portal->GetRootComponent() && Portal->GetRootComponent()->GetMobility() == EComponentMobility::Static);
	TestTrue(TEXT("the look hangs off the root"), Portal->GetLookRoot()->GetAttachParent() == Portal->GetRootComponent());

	for (UStaticMeshComponent* Part : { PostLeft, PostRight, Lintel })
	{
		const FString What = FString::Printf(TEXT("portal %s"), *Part->GetName());
		TestNotNull(What + TEXT(": has a shape"), Part->GetStaticMesh().Get());
		TestTrue(What + TEXT(": crimson #FF2E4A (C§1: portals are the only crimson)"), TintedAs(Part, DFWorldLook::PortalFrame()));
		ExpectInert(*this, World.GetWorld(), Part, What);
	}

	// A gate the wave fits through: 3-4 m tall, standing on the ground it is placed on, the opening
	// wider than the widest scatter (2.8 m) plus a body (0.8 m).
	TestEqual(TEXT("the frame is 4 m tall"), TopOf(Lintel), 400.0, 1.0);
	TestEqual(TEXT("a ground portal stands on its origin"), BottomOf(PostLeft), 0.0, 1.0);
	const double Opening = FVector::Dist(PostLeft->GetComponentLocation(), PostRight->GetComponentLocation()) - PostLeft->GetComponentScale().Y * 100.0;
	TestTrue(FString::Printf(TEXT("the opening (%.0f cm) takes the widest scatter plus a body (360 cm)"), Opening), Opening >= 360.0);

	// With no lane graph the opening faces the actor's own forward. (Spawned turned: the root is
	// static, and a static root is not moved once a game has begun.)
	ADFSpawnPortal* Turned = World.SpawnActor<ADFSpawnPortal>(FTransform(FRotator(0.f, 30.f, 0.f), FVector(0.0, -3000.0, 0.0)));
	if (TestNotNull(TEXT("turned portal"), Turned))
	{
		TestEqual(TEXT("no graph: faces the actor's yaw"), Turned->GetLookRoot()->GetComponentRotation().Yaw, 30.0, 0.01);
	}

	// With the level's graph it faces the lane leaving its node: westGate's first leg runs +Y, so the
	// posts stand either side of the spine (on X) and the wave walks out between them.
	UDFLaneGraphAsset* Graph = MakeGraph();
	ADFLaneGraphInfo* Info = World.SpawnActor<ADFLaneGraphInfo>();
	if (!TestNotNull(TEXT("lane graph info"), Info))
	{
		return false;
	}
	Info->LaneGraph = Graph;
	Portal->SetStableId(TEXT("westGate"));
	TestEqual(TEXT("the stable id is the portal's own business still"), Portal->GetStableId(), FName(TEXT("westGate")));
	Portal->RefreshLook();
	TestEqual(TEXT("faces the first leg of westGate-core (+Y)"), Portal->GetLookRoot()->GetComponentRotation().Yaw, 90.0, 0.01);
	TestEqual(TEXT("left post is off the spine, across it"), FMath::Abs(PostLeft->GetComponentLocation().X), 230.0, 1.0);
	TestEqual(TEXT("left post is level with the node along the lane"), PostLeft->GetComponentLocation().Y, 0.0, 1.0);
	TestEqual(TEXT("right post mirrors it"), PostRight->GetComponentLocation().X, -PostLeft->GetComponentLocation().X, 1.0);
	TestTrue(TEXT("the actor itself is not turned (the importer owns its transform)"), Portal->GetActorRotation().IsNearlyZero(0.01));

	// LaneYawOutOf: the first leg with horizontal length; warps and unknown nodes give nothing.
	float Yaw = 0.f;
	TestTrue(TEXT("airWest leaves by climbing, then heads -X"), ADFSpawnPortal::LaneYawOutOf(*Graph, TEXT("airWest"), Yaw));
	TestEqual(TEXT("airWest yaw"), FMath::Abs(Yaw), 180.f, 0.01f);
	TestFalse(TEXT("a warp is not a lane to face"), ADFSpawnPortal::LaneYawOutOf(*Graph, TEXT("pad"), Yaw));
	TestFalse(TEXT("a node with no edge out"), ADFSpawnPortal::LaneYawOutOf(*Graph, TEXT("core"), Yaw));
	TestFalse(TEXT("no id"), ADFSpawnPortal::LaneYawOutOf(*Graph, NAME_None, Yaw));

	// An air portal: the flyers leave at its height, so the opening is centred on it.
	Portal->Layer = EDFEnemyLayer::Air;
	Portal->RefreshLook();
	TestEqual(TEXT("air: the lintel's top is half the frame above the node"), TopOf(Lintel), 200.0, 1.0);
	TestEqual(TEXT("air: the posts' feet are half the frame below it"), BottomOf(PostLeft), -200.0, 1.0);
	for (UStaticMeshComponent* Part : { PostLeft, PostRight, Lintel })
	{
		ExpectInert(*this, World.GetWorld(), Part, FString::Printf(TEXT("air portal %s, turned"), *Part->GetName()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWorldCoreLookTest, "DF.Unit.World.CoreLook", DFWorldLookTest::Flags)
bool FDFWorldCoreLookTest::RunTest(const FString& Parameters)
{
	using namespace DFWorldLookTest;
	FDFTestWorld World;
	if (!QueriesWork(*this, World))
	{
		return false;
	}

	ADFCore* Core = World.SpawnActor<ADFCore>(FTransform(FVector(600.0, 3000.0, 0.0)));
	if (!TestNotNull(TEXT("core"), Core))
	{
		return false;
	}
	Core->SetStableId(TEXT("core"));
	TestEqual(TEXT("stable id"), Core->GetStableId(), FName(TEXT("core")));
	TestTrue(TEXT("the root is still the static scene root"), Core->GetRootComponent() && Core->GetRootComponent()->GetMobility() == EComponentMobility::Static);

	UStaticMeshComponent* Plinth = PartNamed(Core, TEXT("Plinth"));
	UStaticMeshComponent* Shaft = PartNamed(Core, TEXT("Shaft"));
	UStaticMeshComponent* Cap = PartNamed(Core, TEXT("Cap"));
	UPointLightComponent* Glow = Core->FindComponentByClass<UPointLightComponent>();
	if (!TestNotNull(TEXT("plinth"), Plinth) || !TestNotNull(TEXT("shaft"), Shaft) || !TestNotNull(TEXT("cap"), Cap) || !TestNotNull(TEXT("glow"), Glow))
	{
		return false;
	}
	TestTrue(TEXT("shaft: cyan #22D3EE (C§1)"), TintedAs(Shaft, DFWorldLook::CoreEnergy()));
	TestTrue(TEXT("cap: the bright end of the cyan"), TintedAs(Cap, DFWorldLook::CoreCap()));
	TestTrue(TEXT("plinth: obsidian"), TintedAs(Plinth, DFWorldLook::CoreBase()));
	for (UStaticMeshComponent* Part : { Plinth, Shaft, Cap })
	{
		const FString What = FString::Printf(TEXT("core %s"), *Part->GetName());
		TestNotNull(What + TEXT(": has a shape"), Part->GetStaticMesh().Get());
		ExpectInert(*this, World.GetWorld(), Part, What);
	}

	// Tall enough to read from across the map, standing on the node.
	const double Top = TopOf(Cap) - Core->GetActorLocation().Z;
	TestTrue(FString::Printf(TEXT("the core is 4-6 m tall (%.0f cm)"), Top), Top >= 400.0 && Top <= 600.0);
	TestEqual(TEXT("it stands on its origin"), BottomOf(Plinth), Core->GetActorLocation().Z, 1.0);
	TestTrue(TEXT("the cap is the top"), TopOf(Cap) > TopOf(Shaft));
	TestTrue(TEXT("the glow is above the cap (inside it, it would light no face of it)"), Glow->GetComponentLocation().Z > TopOf(Cap));
	TestFalse(TEXT("the glow casts no shadow"), Glow->CastShadows != 0);
	TestTrue(TEXT("the glow is movable (no static lighting in this project)"), Glow->GetMobility() == EComponentMobility::Movable);

	// Tinting again (construction and BeginPlay both do) reuses the instance.
	UMaterialInterface* Before = Shaft->GetMaterial(0);
	Core->RefreshLook();
	TestTrue(TEXT("a second tint reuses the dynamic instance"), Shaft->GetMaterial(0) == Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWorldSocketLookTest, "DF.Unit.World.SocketLook", DFWorldLookTest::Flags)
bool FDFWorldSocketLookTest::RunTest(const FString& Parameters)
{
	using namespace DFWorldLookTest;
	FDFTestWorld World;
	if (!QueriesWork(*this, World))
	{
		return false;
	}

	ADFSocket* Socket = World.SpawnActor<ADFSocket>(FTransform(FVector(-1600.0, 500.0, 0.0)));
	if (!TestNotNull(TEXT("socket"), Socket))
	{
		return false;
	}
	Socket->SocketId = TEXT("s1");
	Socket->RefreshLook();
	UStaticMeshComponent* Plate = PartNamed(Socket, TEXT("Plate"));
	UStaticMeshComponent* Ring = PartNamed(Socket, TEXT("AimRing"));
	if (!TestNotNull(TEXT("plate"), Plate) || !TestNotNull(TEXT("aim ring"), Ring))
	{
		return false;
	}
	TestNotNull(TEXT("plate: has a shape"), Plate->GetStaticMesh().Get());
	TestTrue(TEXT("a ground pad is brass"), TintedAs(Plate, DFWorldLook::PadGround()));
	ExpectInert(*this, World.GetWorld(), Plate, TEXT("pad plate"));
	ExpectInert(*this, World.GetWorld(), Ring, TEXT("pad aim ring"));

	// The plate is the pad box: a tower placed at GetPadTop stands on it, and the ring shows round it.
	TestEqual(TEXT("the plate's top is the pad top"), TopOf(Plate), Socket->GetPadTop().Z, 1.0);
	TestEqual(TEXT("the plate stands on the socket's origin"), BottomOf(Plate), Socket->GetActorLocation().Z, 1.0);
	TestTrue(TEXT("the ring is wider than the plate"), Ring->GetComponentScale().X > Plate->GetComponentScale().X);
	TestTrue(TEXT("the ring sits below the plate's top"), TopOf(Ring) < TopOf(Plate));

	// The pad box still answers the build trace through the look (the plate collides with nothing).
	FHitResult Hit;
	const FVector Above = Socket->GetPadTop() + FVector(0.0, 0.0, 300.0);
	TestTrue(TEXT("a DF_Build trace from above still finds the pad box"),
		World.GetWorld()->LineTraceSingleByChannel(Hit, Above, Socket->GetActorLocation() - FVector(0.0, 0.0, 50.0), DFCollision::Build) && Hit.GetActor() == Socket
		&& Hit.GetComponent() == Socket->GetPad());

	// The aim ring: hidden until the local player aims at the pad; green when free, gold when a tower is on it.
	TestFalse(TEXT("no ring until aimed"), Ring->IsVisible());
	Socket->SetAimHighlight(EDFPadHighlight::Free);
	TestTrue(TEXT("aimed at a free pad: ring shown"), Ring->IsVisible());
	UMaterialInstanceDynamic* Glow = Cast<UMaterialInstanceDynamic>(Ring->GetMaterial(0));
	TestTrue(TEXT("the ring glows (EmissiveMeshMaterial)"), Glow && Glow->Parent == DFShapeLook::GlowMaterial());
	TestTrue(TEXT("free: green"), Glow && Glow->K2_GetVectorParameterValue(TEXT("Color")).Equals(DFWorldLook::PadAimFree(), 1e-4f));
	Socket->SetAimHighlight(EDFPadHighlight::Occupied);
	TestTrue(TEXT("occupied: gold"), Glow && Glow->K2_GetVectorParameterValue(TEXT("Color")).Equals(DFWorldLook::PadAimOccupied(), 1e-4f));
	TestTrue(TEXT("re-colouring keeps the instance"), Ring->GetMaterial(0) == Glow);
	Socket->SetAimHighlight(EDFPadHighlight::Blocked);
	TestTrue(TEXT("blocked: red"), Glow && Glow->K2_GetVectorParameterValue(TEXT("Color")).Equals(DFWorldLook::PadAimBlocked(), 1e-4f));
	Socket->SetAimHighlight(EDFPadHighlight::None);
	TestFalse(TEXT("aimed away: ring hidden"), Ring->IsVisible());

	// A ground pad shows its full plate; the flush one is for pads in the lane.
	UStaticMeshComponent* Flush = PartNamed(Socket, TEXT("FlushPlate"));
	if (!TestNotNull(TEXT("flush plate"), Flush))
	{
		return false;
	}
	ExpectInert(*this, World.GetWorld(), Flush, TEXT("pad flush plate"));
	TestTrue(TEXT("a ground pad shows the full plate"), Plate->IsVisible() && !Flush->IsVisible());

	// A trap pad lies in the lane: a flush, hazard-amber plate that a body walking the lane crosses
	// rather than sinks into; a refresh keeps its instance.
	ADFSocket* Trap = World.SpawnActor<ADFSocket>(FTransform(FVector(-1600.0, 1500.0, 0.0)));
	Trap->Tag = EDFSocketTag::Trap;
	Trap->RefreshLook();
	UStaticMeshComponent* TrapPlate = PartNamed(Trap, TEXT("Plate"));
	UStaticMeshComponent* TrapFlush = PartNamed(Trap, TEXT("FlushPlate"));
	if (!TestNotNull(TEXT("trap plates"), TrapPlate) || !TestNotNull(TEXT("trap flush plate"), TrapFlush))
	{
		return false;
	}
	TestTrue(TEXT("a trap pad shows only the flush plate"), TrapFlush->IsVisible() && !TrapPlate->IsVisible());
	TestTrue(TEXT("a trap pad is hazard amber"), TintedAs(TrapFlush, DFWorldLook::PadTrap()));
	const double FlushTop = TopOf(TrapFlush) - Trap->GetActorLocation().Z;
	// Low enough to walk over, high enough to show above the lane strip (2 cm thick, lifted 1 cm).
	TestTrue(FString::Printf(TEXT("the flush plate is 3.5-5 cm high (%.1f)"), FlushTop), FlushTop > 3.5 && FlushTop <= 5.0);
	TestTrue(TEXT("the ring's top is above a flush plate's (the whole in-lane pad lights)"), TopOf(PartNamed(Trap, TEXT("AimRing"))) > TopOf(TrapFlush));
	TestFalse(TEXT("no lane surface under the pad: it stays level"), Trap->AlignToLaneSurface());
	UMaterialInterface* Before = TrapFlush->GetMaterial(0);
	Trap->RefreshLook();
	TestTrue(TEXT("a second tint reuses the dynamic instance"), TrapFlush->GetMaterial(0) == Before);

	// A barricade pad lies in the lane too.
	ADFSocket* Barricade = World.SpawnActor<ADFSocket>(FTransform(FVector(-1600.0, 2500.0, 0.0)));
	Barricade->Tag = EDFSocketTag::Barricade;
	Barricade->RefreshLook();
	UStaticMeshComponent* BarricadeFlush = PartNamed(Barricade, TEXT("FlushPlate"));
	TestTrue(TEXT("a barricade pad is flush concrete"), BarricadeFlush && BarricadeFlush->IsVisible() && TintedAs(BarricadeFlush, DFWorldLook::PadBarricade()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWorldLanePadSlopeTest, "DF.Unit.World.LanePadFollowsSlope", DFWorldLookTest::Flags)
bool FDFWorldLanePadSlopeTest::RunTest(const FString& Parameters)
{
	// Foundry's trap pads sit on 13-25 % climbs: a level plate there stands out of the lane on its
	// downhill side and the wave walks through it. An in-lane pad lies along the DF_LaneSurface under it.
	using namespace DFWorldLookTest;
	FDFTestWorld World;
	const FVector At(4000.0, -3000.0, 0.0);
	const FRotator Grade(14.f, 0.f, 0.f);   // a 25 % climb along +X
	AActor* Ramp = World.SpawnActor<AActor>(FTransform(Grade, At));
	UBoxComponent* Surface = NewObject<UBoxComponent>(Ramp, TEXT("Surface"));
	Ramp->SetRootComponent(Surface);
	Surface->SetBoxExtent(FVector(600.0, 600.0, 10.0));
	Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Surface->SetCollisionResponseToAllChannels(ECR_Ignore);
	Surface->SetCollisionResponseToChannel(DFCollision::LaneSurface, ECR_Block);
	Surface->RegisterComponent();
	Surface->SetWorldTransform(FTransform(Grade, At - Grade.RotateVector(FVector(0.0, 0.0, 10.0))));   // top face through At

	ADFSocket* Trap = World.SpawnActor<ADFSocket>(FTransform(At));
	if (!TestNotNull(TEXT("trap socket"), Trap))
	{
		return false;
	}
	Trap->Tag = EDFSocketTag::Trap;
	Trap->RefreshLook();
	TestTrue(TEXT("a lane surface under the pad is found"), Trap->AlignToLaneSurface());
	const FVector Normal = Grade.RotateVector(FVector::UpVector);
	for (const TCHAR* Name : { TEXT("FlushPlate"), TEXT("AimRing") })
	{
		UStaticMeshComponent* Part = PartNamed(Trap, Name);
		if (!TestNotNull(Name, Part))
		{
			return false;
		}
		const double Dot = FVector::DotProduct(Part->GetUpVector(), Normal);
		TestTrue(FString::Printf(TEXT("%s lies along the 25 %% grade (up . normal = %.4f)"), Name, Dot), Dot > 0.9999);
		// Its centre sits just above the surface: nothing of it stands out on the downhill side.
		const double Above = FVector::DotProduct(Part->GetComponentLocation() - At, Normal);
		TestTrue(FString::Printf(TEXT("%s sits on the surface (%.1f cm above it)"), Name, Above), Above > 0.0 && Above < 3.0);
		ExpectInert(*this, World.GetWorld(), Part, FString::Printf(TEXT("sloped %s"), Name));
	}

	// A ground pad is never tilted: a tower stands on it, and the importer levels its pad.
	ADFSocket* Ground = World.SpawnActor<ADFSocket>(FTransform(At + FVector(200.0, 0.0, 50.0)));
	TestFalse(TEXT("a ground pad does not follow the lane"), Ground && Ground->AlignToLaneSurface());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWorldLaneStripTest, "DF.Unit.World.LaneStrip", DFWorldLookTest::Flags)
bool FDFWorldLaneStripTest::RunTest(const FString& Parameters)
{
	using namespace DFWorldLookTest;
	UDFLaneGraphAsset* Graph = MakeGraph();

	// The editor never gets a strip: the level the importer saves carries none.
	{
		FDFTestWorld Editor(EWorldType::Editor);
		ADFLaneGraphInfo* Info = Editor.SpawnActor<ADFLaneGraphInfo>();
		if (TestNotNull(TEXT("editor: info"), Info))
		{
			Info->LaneGraph = Graph;
			Info->RebuildLaneStrip();
			TestNull(TEXT("editor: no strip"), Info->GetLaneStrip());
			TestTrue(TEXT("editor: the info stays hidden"), Info->IsHidden());
		}
	}

	FDFTestWorld World;
	if (!QueriesWork(*this, World))
	{
		return false;
	}
	ADFLaneGraphInfo* Info = World.SpawnActor<ADFLaneGraphInfo>();
	if (!TestNotNull(TEXT("info"), Info))
	{
		return false;
	}
	Info->LaneGraph = Graph;
	Info->RebuildLaneStrip();
	UInstancedStaticMeshComponent* Strip = Info->GetLaneStrip();
	if (!TestNotNull(TEXT("a game draws the strip"), Strip))
	{
		return false;
	}
	// westGate-core has two legs; the air lane and the warp are not drawn.
	TestEqual(TEXT("one flat piece per ground leg"), Strip->GetInstanceCount(), 2);
	TestFalse(TEXT("the info is drawn (AInfo is hidden by default)"), Info->IsHidden());
	TestTrue(TEXT("strip: steel"), TintedAs(Strip, DFWorldLook::LaneStrip()));
	ExpectInert(*this, World.GetWorld(), Strip, TEXT("lane strip"));
	// The one query that looks straight down at a lane: projecting onto DF_LaneSurface (importer,
	// scrap settling) must still find the ground, not the strip.
	TestEqual(TEXT("a DF_LaneSurface trace down onto the first leg misses the strip"),
		HitsOn(World.GetWorld(), Strip, FVector(0.0, 500.0, 1.0), DFCollision::LaneSurface), 0);

	FTransform First;
	if (TestTrue(TEXT("first piece"), Strip->GetInstanceTransform(0, First, /*bWorldSpace*/ true)))
	{
		TestEqual(TEXT("the first piece lies along its leg (+Y)"), First.Rotator().Yaw, 90.0, 0.01);
		TestEqual(TEXT("and runs half a width past each end (10 m + 1.2 m)"), First.GetScale3D().X, 11.2, 1e-3);
		TestTrue(TEXT("and sits just on the ground"), First.GetLocation().Z > 0.0 && First.GetLocation().Z < 5.0);
	}

	Info->RebuildLaneStrip();
	TestEqual(TEXT("drawing again replaces, never doubles"), Strip->GetInstanceCount(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
