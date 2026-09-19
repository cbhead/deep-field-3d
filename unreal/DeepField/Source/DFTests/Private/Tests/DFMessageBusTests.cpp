// DF.Unit.MessageBus.* — the C15 transport (ADR-0004, ADR-0019): parent fan-out, exact-only
// subscriptions, and the "a subscriber may (un)subscribe during delivery" promise in
// DFMessageBus.h, exercised through the shared test helpers (DFCore/Public/Testing/DFTestUtils.h).

#include "DFGameplayTags.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMessageBusParentFanoutTest, "DF.Unit.MessageBus.ParentFanout",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDFMessageBusParentFanoutTest::RunTest(const FString& Parameters)
{
	FDFTestWorld World;
	UDFMessageBus* Bus = World.MessageBus();
	if (!TestNotNull(TEXT("UDFMessageBus exists on the test game instance"), Bus))
	{
		return false;
	}

	FDFMessageCapture Everything(Bus, DFTags::Message);                                  // DF.Message, children included
	FDFMessageCapture ExactRoot(Bus, DFTags::Message, /*bIncludeChildren*/ false);       // DF.Message, exact only
	FDFMessageCapture ExactLeaf(Bus, DFTags::Message_TowerPlaced, /*bIncludeChildren*/ false);

	FDFMsg_Structure Placed;
	Placed.StructureId = 7;
	Placed.PlayerId = 2;
	Placed.DefId = TEXT("lance");
	const int64 CountBefore = Bus->GetBroadcastCount();
	Bus->Broadcast(DFTags::Message_TowerPlaced, Placed);

	TestEqual(TEXT("broadcast count advanced"), Bus->GetBroadcastCount(), CountBefore + 1);
	TestEqual(TEXT("parent subscriber received the child message"), Everything.Num(), 1);
	TestEqual(TEXT("exact-only subscriber on the parent did not"), ExactRoot.Num(), 0);
	TestEqual(TEXT("exact subscriber on the leaf received it"), ExactLeaf.Num(), 1);

	if (Everything.Num() == 1)
	{
		TestTrue(TEXT("delivered tag is the leaf that was broadcast, not the parent subscribed to"),
			Everything[0].Tag == DFTags::Message_TowerPlaced.GetTag());
		const FDFMsg_Structure* Payload = Everything.PayloadAt<FDFMsg_Structure>(0);
		if (TestNotNull(TEXT("payload is the struct that was broadcast"), Payload))
		{
			TestEqual(TEXT("payload StructureId"), Payload->StructureId, 7);
			TestEqual(TEXT("payload DefId"), Payload->DefId, FName(TEXT("lance")));
		}
	}

	// A second leaf under the same parent: the parent subscriber hears it, the leaf-exact one does not.
	FDFMsg_Structure Sold;
	Sold.StructureId = 7;
	Bus->Broadcast(DFTags::Message_TowerSold, Sold);
	TestEqual(TEXT("parent subscriber heard both leaves"), Everything.Num(), 2);
	TestEqual(TEXT("parent subscriber: one TowerPlaced"), Everything.CountOf(DFTags::Message_TowerPlaced), 1);
	TestEqual(TEXT("parent subscriber: one TowerSold"), Everything.CountOf(DFTags::Message_TowerSold), 1);
	TestEqual(TEXT("exact leaf subscriber ignored the sibling leaf"), ExactLeaf.Num(), 1);
	TestEqual(TEXT("exact root subscriber still heard nothing"), ExactRoot.Num(), 0);

	// The typed Subscribe<T> helper filters on payload type: a different struct under the same tag is dropped.
	int32 TypedHits = 0;
	FDFMessageHandle Typed = Bus->Subscribe<FDFMsg_Structure>(DFTags::Message,
		[&TypedHits](const FGameplayTag&, const FDFMsg_Structure&) { ++TypedHits; });
	Bus->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure());
	Bus->Broadcast(DFTags::Message_PlayerJoined, FDFMsg_Player());
	TestEqual(TEXT("typed subscriber saw the matching payload only"), TypedHits, 1);
	TestEqual(TEXT("raw parent subscriber saw both"), Everything.Num(), 4);
	Bus->Unsubscribe(Typed);
	Bus->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure());
	TestEqual(TEXT("unsubscribed typed handler is silent"), TypedHits, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMessageBusUnsubscribeDuringDeliveryTest, "DF.Unit.MessageBus.UnsubscribeDuringDelivery",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDFMessageBusUnsubscribeDuringDeliveryTest::RunTest(const FString& Parameters)
{
	FDFTestWorld World;
	UDFMessageBus* Bus = World.MessageBus();
	if (!TestNotNull(TEXT("UDFMessageBus exists on the test game instance"), Bus))
	{
		return false;
	}

	// A: on its first delivery, unsubscribes itself and B (subscribed after it) and subscribes C.
	// Mutating the subscriber list mid-delivery must neither crash nor skip/duplicate anyone;
	// after the broadcast the removed handlers hear nothing more and the added one hears the next.
	int32 HitsA = 0, HitsB = 0, HitsC = 0;
	FDFMessageHandle A, B, C;
	B = Bus->Subscribe<FDFMsg_Structure>(DFTags::Message_TowerPlaced, [&HitsB](const FGameplayTag&, const FDFMsg_Structure&) { ++HitsB; });
	A = Bus->Subscribe<FDFMsg_Structure>(DFTags::Message_TowerPlaced,
		[&](const FGameplayTag&, const FDFMsg_Structure&)
		{
			++HitsA;
			Bus->Unsubscribe(A);
			Bus->Unsubscribe(B);
			if (!C.IsValid())
			{
				C = Bus->Subscribe<FDFMsg_Structure>(DFTags::Message_TowerPlaced, [&HitsC](const FGameplayTag&, const FDFMsg_Structure&) { ++HitsC; });
			}
		});
	// A parent-tag listener after the leaf ones, to prove delivery continues up the tag chain.
	FDFMessageCapture Parent(Bus, DFTags::Message);

	Bus->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure());

	TestEqual(TEXT("A ran once"), HitsA, 1);
	TestTrue(TEXT("B heard at most the in-flight message"), HitsB <= 1);
	TestEqual(TEXT("C, subscribed during delivery, did not hear the in-flight message"), HitsC, 0);
	TestEqual(TEXT("delivery continued to the parent subscriber"), Parent.Num(), 1);

	const int32 HitsBAfterFirst = HitsB;
	Bus->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure());
	Bus->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure());

	TestEqual(TEXT("A, unsubscribed by itself, heard nothing more"), HitsA, 1);
	TestEqual(TEXT("B, unsubscribed by A, heard nothing more"), HitsB, HitsBAfterFirst);
	TestEqual(TEXT("C heard every broadcast after its subscription"), HitsC, 2);
	TestEqual(TEXT("parent subscriber heard all three"), Parent.Num(), 3);

	Bus->Unsubscribe(C);
	Bus->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure());
	TestEqual(TEXT("C silent after unsubscribe"), HitsC, 2);

	// A capture that stops itself from inside its own callback (the FDFMessageCapture::Stop promise).
	{
		FDFMessageCapture* SelfStopping = nullptr;
		int32 StopHits = 0;
		FDFMessageHandle Stopper = Bus->SubscribeRaw(DFTags::Message_TowerPlaced, FDFMessageDelegate::CreateLambda(
			[&](const FGameplayTag&, const FInstancedStruct&)
			{
				++StopHits;
				if (SelfStopping)
				{
					SelfStopping->Stop();
				}
			}));
		FDFMessageCapture Capture(Bus, DFTags::Message_TowerPlaced);
		SelfStopping = &Capture;
		Bus->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure());
		Bus->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure());
		TestEqual(TEXT("stopper ran twice"), StopHits, 2);
		TestTrue(TEXT("capture stopped during the first delivery heard at most that one"), Capture.Num() <= 1);
		Bus->Unsubscribe(Stopper);
	}
	TestEqual(TEXT("C, unsubscribed, heard none of the later broadcasts"), HitsC, 2);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
