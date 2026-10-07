// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "RockInteractionTestFixture.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "GameplayTagsManager.h"
#include "RockInteractionLibrary.h"

// The point-gathering helpers: tagged scene components, static mesh sockets, skeletal mesh null handling, and
// GetCandidateActor. All of them append to the array they are given, and the Refresh functions return false (and log a
// warning) when the topology changed and a full re-gather is needed.

namespace
{
	const FVector Origin(0.0, 0.0, 0.0);

	USceneComponent* AddComponent(AActor& Owner, USceneComponent* Parent, const TCHAR* Name, const FVector& WorldLocation, const TArray<FName>& Tags)
	{
		USceneComponent* Component = NewObject<USceneComponent>(&Owner, Name);
		Component->SetupAttachment(Parent ? Parent : Owner.GetRootComponent());
		Component->ComponentTags = Tags;
		Component->RegisterComponent();
		Component->SetWorldLocation(WorldLocation);
		return Component;
	}

	UStaticMeshSocket* AddSocket(UStaticMesh& Mesh, const TCHAR* Name, const FVector& RelativeLocation)
	{
		UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(&Mesh);
		Socket->SocketName = Name;
		Socket->RelativeLocation = RelativeLocation;
		Mesh.Sockets.Add(Socket);
		return Socket;
	}
}

TEST_CLASS(RockInteractionTaggedComponentTests, "BRS.RockInteraction.Library.TaggedComponents")
{
	FRockInteractionFixture Fixture;
	ARockTestInteractable* Actor = nullptr;

	BEFORE_EACH()
	{
		Actor = &Fixture.SpawnInteractable(Origin);
	}

	TEST_METHOD(Append_NullRoot_AddsNothing)
	{
		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(0, URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, nullptr, true)));
		ASSERT_THAT(IsTrue(Points.IsEmpty()));
	}

	TEST_METHOD(Append_ComponentsWithoutIXTags_AddNothing)
	{
		AddComponent(*Actor, nullptr, TEXT("Plain"), FVector(10, 0, 0), {});
		AddComponent(*Actor, nullptr, TEXT("OtherTag"), FVector(20, 0, 0), {TEXT("Interact"), TEXT("XIX_Nope")});

		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(0, URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true)));
	}

	TEST_METHOD(Append_IXTag_AddsInteractionPoint)
	{
		USceneComponent* Lever = AddComponent(*Actor, nullptr, TEXT("Lever"), FVector(10, 20, 30), {TEXT("IX_Lever")});

		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(1, URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true)));

		ASSERT_THAT(IsTrue(Points[0].WorldLocation.Equals(FVector(10, 20, 30))));
		ASSERT_THAT(IsTrue(Points[0].SourceComponent.Get() == Lever));
		ASSERT_THAT(IsTrue(Points[0].Role == ERockInteractionPointRole::Interaction));
		ASSERT_THAT(IsTrue(Points[0].SocketName == NAME_None));
		ASSERT_THAT(IsTrue(Points[0].LookAtThresholdScale == 1.f));
	}

	/** Every point gets the Activate verb. The caller is expected to override it. */
	TEST_METHOD(Append_PointTag_IsTheActivateVerb)
	{
		AddComponent(*Actor, nullptr, TEXT("Lever"), FVector(10, 0, 0), {TEXT("IX_Lever")});

		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true);

		const FGameplayTag Activate = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Verb.Activate"));
		ASSERT_THAT(IsTrue(Points[0].PointTag == Activate));
	}

	TEST_METHOD(Append_IXVPTag_AddsVisibilityPoint)
	{
		AddComponent(*Actor, nullptr, TEXT("Tip"), FVector(10, 0, 0), {TEXT("IX_VP_Tip")});

		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(1, URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true)));
		ASSERT_THAT(IsTrue(Points[0].Role == ERockInteractionPointRole::Visibility));
	}

	/** Only the first IX_ tag on a component is looked at, so its role comes from that tag. */
	TEST_METHOD(Append_FirstMatchingTagDecidesRole)
	{
		AddComponent(*Actor, nullptr, TEXT("IXFirst"), FVector(10, 0, 0), {TEXT("IX_Lever"), TEXT("IX_VP_Other")});
		AddComponent(*Actor, nullptr, TEXT("VPFirst"), FVector(20, 0, 0), {TEXT("Unrelated"), TEXT("IX_VP_Tip"), TEXT("IX_Lever")});

		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(2, URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true)));

		const FRockInteractionPoint* IXFirst = Points.FindByPredicate([](const FRockInteractionPoint& P) { return P.WorldLocation.X == 10.0; });
		const FRockInteractionPoint* VPFirst = Points.FindByPredicate([](const FRockInteractionPoint& P) { return P.WorldLocation.X == 20.0; });
		ASSERT_THAT(IsNotNull(IXFirst));
		ASSERT_THAT(IsNotNull(VPFirst));
		ASSERT_THAT(IsTrue(IXFirst->Role == ERockInteractionPointRole::Interaction));
		ASSERT_THAT(IsTrue(VPFirst->Role == ERockInteractionPointRole::Visibility));
	}

	TEST_METHOD(Append_RootItselfIsIncludedWhenTagged)
	{
		Actor->GetRootComponent()->ComponentTags.Add(TEXT("IX_Root"));

		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(1, URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true)));
		ASSERT_THAT(IsTrue(Points[0].SourceComponent.Get() == Actor->GetRootComponent()));
	}

	TEST_METHOD(Append_Recursive_FindsGrandchildren)
	{
		USceneComponent* Child = AddComponent(*Actor, nullptr, TEXT("Child"), FVector(10, 0, 0), {});
		USceneComponent* Grandchild = AddComponent(*Actor, Child, TEXT("Grandchild"), FVector(20, 0, 0), {TEXT("IX_Deep")});

		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(1, URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true)));
		ASSERT_THAT(IsTrue(Points[0].SourceComponent.Get() == Grandchild));
	}

	TEST_METHOD(Append_NotRecursive_OnlyDirectChildren)
	{
		USceneComponent* Child = AddComponent(*Actor, nullptr, TEXT("Child"), FVector(10, 0, 0), {TEXT("IX_Near")});
		AddComponent(*Actor, Child, TEXT("Grandchild"), FVector(20, 0, 0), {TEXT("IX_Deep")});

		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(1, URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), false)));
		ASSERT_THAT(IsTrue(Points[0].SourceComponent.Get() == Child));
	}

	TEST_METHOD(Append_DoesNotResetTheArray_AndReturnsOnlyWhatItAdded)
	{
		AddComponent(*Actor, nullptr, TEXT("Lever"), FVector(10, 0, 0), {TEXT("IX_Lever")});

		TArray<FRockInteractionPoint> Points;
		Points.Add(FRockInteractionFixture::MakePoint(FVector(99, 0, 0), RockInteractionTestTags::PointA));

		ASSERT_THAT(AreEqual(1, URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true)));
		ASSERT_THAT(AreEqual(2, Points.Num()));
		ASSERT_THAT(IsTrue(Points[0].WorldLocation.X == 99.0));
	}

	// --- Refresh ---

	TEST_METHOD(Refresh_NullRoot_ReturnsFalse)
	{
		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(IsFalse(URockInteractionLibrary::RefreshPointsFromTaggedComponents(Points, nullptr, true, 0, INDEX_NONE)));
	}

	TEST_METHOD(Refresh_UpdatesLocationsAfterComponentsMove)
	{
		USceneComponent* A = AddComponent(*Actor, nullptr, TEXT("A"), FVector(10, 0, 0), {TEXT("IX_A")});
		USceneComponent* B = AddComponent(*Actor, nullptr, TEXT("B"), FVector(20, 0, 0), {TEXT("IX_B")});
		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true);

		A->SetWorldLocation(FVector(100, 0, 0));
		B->SetWorldLocation(FVector(200, 0, 0));

		ASSERT_THAT(IsTrue(URockInteractionLibrary::RefreshPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true, 0, INDEX_NONE)));
		const FRockInteractionPoint* PointA = Points.FindByPredicate([A](const FRockInteractionPoint& P) { return P.SourceComponent.Get() == A; });
		const FRockInteractionPoint* PointB = Points.FindByPredicate([B](const FRockInteractionPoint& P) { return P.SourceComponent.Get() == B; });
		ASSERT_THAT(IsNotNull(PointA));
		ASSERT_THAT(IsNotNull(PointB));
		ASSERT_THAT(IsTrue(PointA->WorldLocation.Equals(FVector(100, 0, 0))));
		ASSERT_THAT(IsTrue(PointB->WorldLocation.Equals(FVector(200, 0, 0))));
	}

	TEST_METHOD(Refresh_CountLimitsTheWindow)
	{
		TArray<USceneComponent*> Components;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Components.Add(AddComponent(*Actor, nullptr, *FString::Printf(TEXT("C%d"), Index), FVector(10.0 * (Index + 1), 0, 0), {TEXT("IX_Point")}));
		}
		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true);
		ASSERT_THAT(AreEqual(3, Points.Num()));
		for (USceneComponent* Component : Components)
		{
			Component->SetWorldLocation(FVector(500, 0, 0));
		}

		// Only the one point at index 1 is refreshed. The others keep their old locations.
		ASSERT_THAT(IsTrue(URockInteractionLibrary::RefreshPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true, 1, 1)));

		int32 Refreshed = 0;
		for (const FRockInteractionPoint& Point : Points)
		{
			Refreshed += Point.WorldLocation.X == 500.0 ? 1 : 0;
		}
		ASSERT_THAT(AreEqual(1, Refreshed));
		ASSERT_THAT(IsTrue(Points[1].WorldLocation.X == 500.0));
	}

	TEST_METHOD(Refresh_NegativeCount_RefreshesToTheEnd)
	{
		TArray<USceneComponent*> Components;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Components.Add(AddComponent(*Actor, nullptr, *FString::Printf(TEXT("C%d"), Index), FVector(10.0 * (Index + 1), 0, 0), {TEXT("IX_Point")}));
		}
		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true);
		for (USceneComponent* Component : Components)
		{
			Component->SetWorldLocation(FVector(500, 0, 0));
		}

		ASSERT_THAT(IsTrue(URockInteractionLibrary::RefreshPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true, 1, INDEX_NONE)));

		int32 Refreshed = 0;
		for (const FRockInteractionPoint& Point : Points)
		{
			Refreshed += Point.WorldLocation.X == 500.0 ? 1 : 0;
		}
		ASSERT_THAT(AreEqual(2, Refreshed));
	}

	TEST_METHOD(Refresh_EmptyRange_ReturnsTrueWithoutWarning)
	{
		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(IsTrue(URockInteractionLibrary::RefreshPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true, 0, INDEX_NONE)));
		ASSERT_THAT(IsTrue(URockInteractionLibrary::RefreshPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true, 5, 2)));
	}

	TEST_METHOD(Refresh_SourceNotUnderRoot_ReturnsFalse)
	{
		ARockTestInteractable& Other = Fixture.SpawnInteractable(Origin);
		USceneComponent* Elsewhere = AddComponent(Other, nullptr, TEXT("Elsewhere"), FVector(10, 0, 0), {TEXT("IX_Elsewhere")});
		TArray<FRockInteractionPoint> Points;
		Points.Add(FRockInteractionFixture::MakePoint(FVector(10, 0, 0), RockInteractionTestTags::PointA, ERockInteractionPointRole::Interaction, Elsewhere));

		TestRunner->AddExpectedMessagePlain(TEXT("RefreshPointsFromTaggedComponents: refreshed 0 / 1"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		ASSERT_THAT(IsFalse(URockInteractionLibrary::RefreshPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true, 0, INDEX_NONE)));
	}

	/** The refresh must use the same recursion as the append, otherwise deep points are reported as gone. */
	TEST_METHOD(Refresh_NotRecursive_MissesGrandchildren)
	{
		USceneComponent* Child = AddComponent(*Actor, nullptr, TEXT("Child"), FVector(10, 0, 0), {});
		AddComponent(*Actor, Child, TEXT("Grandchild"), FVector(20, 0, 0), {TEXT("IX_Deep")});
		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromTaggedComponents(Points, Actor->GetRootComponent(), true);
		ASSERT_THAT(AreEqual(1, Points.Num()));

		TestRunner->AddExpectedMessagePlain(TEXT("RefreshPointsFromTaggedComponents: refreshed 0 / 1"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		ASSERT_THAT(IsFalse(URockInteractionLibrary::RefreshPointsFromTaggedComponents(Points, Actor->GetRootComponent(), false, 0, INDEX_NONE)));
	}
};

TEST_CLASS(RockInteractionStaticMeshTests, "BRS.RockInteraction.Library.StaticMesh")
{
	FRockInteractionFixture Fixture;
	ARockTestInteractable* Actor = nullptr;
	UStaticMesh* Mesh = nullptr;
	UStaticMeshComponent* Component = nullptr;

	BEFORE_EACH()
	{
		Actor = &Fixture.SpawnInteractable(Origin);
		Mesh = Fixture.MakeObject<UStaticMesh>();
		Component = NewObject<UStaticMeshComponent>(Actor);
		Component->SetStaticMesh(Mesh);
		Component->SetWorldLocation(FVector(100, 0, 0));
	}

	TEST_METHOD(Append_NullComponent_AddsNothing)
	{
		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(0, URockInteractionLibrary::AppendPointsFromStaticMesh(Points, nullptr)));
	}

	TEST_METHOD(Append_ComponentWithoutMesh_AddsNothing)
	{
		UStaticMeshComponent* Bare = NewObject<UStaticMeshComponent>(Actor);
		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(0, URockInteractionLibrary::AppendPointsFromStaticMesh(Points, Bare)));
	}

	TEST_METHOD(Append_OnlyIXSocketsBecomePoints_InOrder)
	{
		AddSocket(*Mesh, TEXT("Decor"), FVector(1, 1, 1));
		AddSocket(*Mesh, TEXT("IX_Lever"), FVector(1, 0, 0));
		AddSocket(*Mesh, TEXT("Other"), FVector(2, 2, 2));
		AddSocket(*Mesh, TEXT("IX_Button"), FVector(0, 1, 0));

		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(2, URockInteractionLibrary::AppendPointsFromStaticMesh(Points, Component)));
		ASSERT_THAT(AreEqual(2, Points.Num()));
		ASSERT_THAT(IsTrue(Points[0].SocketName == TEXT("IX_Lever")));
		ASSERT_THAT(IsTrue(Points[1].SocketName == TEXT("IX_Button")));
	}

	TEST_METHOD(Append_PointCarriesSourceRoleAndTag)
	{
		AddSocket(*Mesh, TEXT("IX_Lever"), FVector(1, 0, 0));
		AddSocket(*Mesh, TEXT("IX_VP_Tip"), FVector(0, 0, 5));

		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromStaticMesh(Points, Component);

		ASSERT_THAT(AreEqual(2, Points.Num()));
		ASSERT_THAT(IsTrue(Points[0].SourceComponent.Get() == Component));
		ASSERT_THAT(IsTrue(Points[0].Role == ERockInteractionPointRole::Interaction));
		ASSERT_THAT(IsTrue(Points[1].Role == ERockInteractionPointRole::Visibility));
		const FGameplayTag Activate = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Verb.Activate"));
		ASSERT_THAT(IsTrue(Points[0].PointTag == Activate));
	}

	TEST_METHOD(Append_LocationFollowsTheComponentTransform)
	{
		AddSocket(*Mesh, TEXT("IX_Lever"), FVector(10, 0, 0));
		Component->SetWorldLocationAndRotation(FVector(100, 0, 0), FRotator(0, 90, 0));

		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromStaticMesh(Points, Component);

		// A 90 degree yaw turns the socket's +X offset into +Y.
		ASSERT_THAT(AreEqual(1, Points.Num()));
		ASSERT_THAT(IsTrue(Points[0].WorldLocation.Equals(FVector(100, 10, 0), 0.01)));
	}

	TEST_METHOD(Append_DoesNotResetTheArray)
	{
		AddSocket(*Mesh, TEXT("IX_Lever"), FVector(1, 0, 0));
		TArray<FRockInteractionPoint> Points;
		Points.Add(FRockInteractionFixture::MakePoint(FVector(99, 0, 0), RockInteractionTestTags::PointA));

		ASSERT_THAT(AreEqual(1, URockInteractionLibrary::AppendPointsFromStaticMesh(Points, Component)));
		ASSERT_THAT(AreEqual(2, Points.Num()));
	}

	TEST_METHOD(Refresh_NullComponentOrMesh_ReturnsFalse)
	{
		TArray<FRockInteractionPoint> Points;
		UStaticMeshComponent* Bare = NewObject<UStaticMeshComponent>(Actor);
		ASSERT_THAT(IsFalse(URockInteractionLibrary::RefreshPointsFromStaticMesh(Points, nullptr, 0, INDEX_NONE)));
		ASSERT_THAT(IsFalse(URockInteractionLibrary::RefreshPointsFromStaticMesh(Points, Bare, 0, INDEX_NONE)));
	}

	/** The sockets between the IX_ ones were filtered out by the append, and the refresh has to step over them. */
	TEST_METHOD(Refresh_StepsOverFilteredSockets_AndUpdatesLocations)
	{
		AddSocket(*Mesh, TEXT("Decor"), FVector(1, 1, 1));
		AddSocket(*Mesh, TEXT("IX_Lever"), FVector(1, 0, 0));
		AddSocket(*Mesh, TEXT("Other"), FVector(2, 2, 2));
		AddSocket(*Mesh, TEXT("IX_Button"), FVector(0, 1, 0));
		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromStaticMesh(Points, Component);
		ASSERT_THAT(AreEqual(2, Points.Num()));

		Component->SetWorldLocation(FVector(500, 0, 0));

		ASSERT_THAT(IsTrue(URockInteractionLibrary::RefreshPointsFromStaticMesh(Points, Component, 0, INDEX_NONE)));
		ASSERT_THAT(IsTrue(Points[0].WorldLocation.Equals(FVector(501, 0, 0), 0.01)));
		ASSERT_THAT(IsTrue(Points[1].WorldLocation.Equals(FVector(500, 1, 0), 0.01)));
	}

	TEST_METHOD(Refresh_CountLimitsTheWindow)
	{
		AddSocket(*Mesh, TEXT("IX_A"), FVector(1, 0, 0));
		AddSocket(*Mesh, TEXT("IX_B"), FVector(2, 0, 0));
		AddSocket(*Mesh, TEXT("IX_C"), FVector(3, 0, 0));
		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromStaticMesh(Points, Component);
		Component->SetWorldLocation(FVector(500, 0, 0));

		// Window of one starting at index 1: point B moves, A and C keep their old location.
		ASSERT_THAT(IsTrue(URockInteractionLibrary::RefreshPointsFromStaticMesh(Points, Component, 1, 1)));
		ASSERT_THAT(IsTrue(Points[0].WorldLocation.Equals(FVector(101, 0, 0), 0.01)));
		ASSERT_THAT(IsTrue(Points[1].WorldLocation.Equals(FVector(502, 0, 0), 0.01)));
		ASSERT_THAT(IsTrue(Points[2].WorldLocation.Equals(FVector(103, 0, 0), 0.01)));
	}

	TEST_METHOD(Refresh_SocketRemovedFromMesh_ReturnsFalse)
	{
		AddSocket(*Mesh, TEXT("IX_A"), FVector(1, 0, 0));
		AddSocket(*Mesh, TEXT("IX_B"), FVector(2, 0, 0));
		TArray<FRockInteractionPoint> Points;
		URockInteractionLibrary::AppendPointsFromStaticMesh(Points, Component);
		Mesh->Sockets.RemoveAt(1);

		TestRunner->AddExpectedMessagePlain(TEXT("RefreshPointsFromStaticMesh: point/socket mismatch"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		ASSERT_THAT(IsFalse(URockInteractionLibrary::RefreshPointsFromStaticMesh(Points, Component, 0, INDEX_NONE)));
	}

	TEST_METHOD(Refresh_EmptyRange_ReturnsTrue)
	{
		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(IsTrue(URockInteractionLibrary::RefreshPointsFromStaticMesh(Points, Component, 0, INDEX_NONE)));
	}
};

TEST_CLASS(RockInteractionSkeletalMeshTests, "BRS.RockInteraction.Library.SkeletalMesh")
{
	FRockInteractionFixture Fixture;

	// Building a skeletal mesh asset with a skeleton and sockets needs far more than a unit test should set up, so these
	// only cover the null and no-asset handling.

	TEST_METHOD(Append_NullComponent_AddsNothing)
	{
		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(0, URockInteractionLibrary::AppendPointsFromSkeletalMesh(Points, nullptr)));
	}

	TEST_METHOD(Append_ComponentWithoutMeshAsset_AddsNothing)
	{
		ARockTestInteractable& Actor = Fixture.SpawnInteractable(Origin);
		USkeletalMeshComponent* Bare = NewObject<USkeletalMeshComponent>(&Actor);
		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(AreEqual(0, URockInteractionLibrary::AppendPointsFromSkeletalMesh(Points, Bare)));
	}

	TEST_METHOD(Refresh_NullOrNoAsset_ReturnsFalse)
	{
		ARockTestInteractable& Actor = Fixture.SpawnInteractable(Origin);
		USkeletalMeshComponent* Bare = NewObject<USkeletalMeshComponent>(&Actor);
		TArray<FRockInteractionPoint> Points;
		ASSERT_THAT(IsFalse(URockInteractionLibrary::RefreshPointsFromSkeletalMesh(Points, nullptr, 0, INDEX_NONE)));
		ASSERT_THAT(IsFalse(URockInteractionLibrary::RefreshPointsFromSkeletalMesh(Points, Bare, 0, INDEX_NONE)));
	}
};

TEST_CLASS(RockInteractionCandidateActorTests, "BRS.RockInteraction.Library.CandidateActor")
{
	FRockInteractionFixture Fixture;

	TEST_METHOD(Actor_ReturnsItself)
	{
		ARockTestInteractable& Actor = Fixture.SpawnInteractable(Origin);
		ASSERT_THAT(IsTrue(URockInteractionLibrary::GetCandidateActor(FRockInteractionFixture::AsTarget(Actor)) == &Actor));
	}

	TEST_METHOD(Component_ReturnsItsOwner)
	{
		ARockTestInteractable& Actor = Fixture.SpawnInteractable(Origin);
		TScriptInterface<IRockInteractableTarget> Target;
		Target.SetObject(Actor.GetRootComponent());

		ASSERT_THAT(IsTrue(URockInteractionLibrary::GetCandidateActor(Target) == &Actor));
	}

	TEST_METHOD(EmptyInterface_ReturnsNull)
	{
		ASSERT_THAT(IsNull(URockInteractionLibrary::GetCandidateActor(TScriptInterface<IRockInteractableTarget>())));
	}

	TEST_METHOD(ObjectThatIsNeitherActorNorComponent_ReturnsNull)
	{
		TScriptInterface<IRockInteractableTarget> Target;
		Target.SetObject(Fixture.MakeObject<URockTestInteractorListener>());

		ASSERT_THAT(IsNull(URockInteractionLibrary::GetCandidateActor(Target)));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
