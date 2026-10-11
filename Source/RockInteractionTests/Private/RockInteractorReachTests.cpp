// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "RockInteractionTestFixture.h"

// IsInReach: the authority's check of a triggered interaction. A target is in reach when it is a current candidate and the pawn is
// within ScanRange (250) + ReachSlack (100) of it; persistent candidates skip the distance part. The pawn is at the origin.

TEST_CLASS(RockInteractorReachTests, "BRS.RockInteraction.Interactor.Reach")
{
	FRockInteractionFixture Fixture;
	APawn* Pawn = nullptr;
	URockTestInteractorComponent* Interactor = nullptr;

	BEFORE_EACH()
	{
		Interactor = &Fixture.SpawnReadyInteractor(Pawn);
	}

	TEST_METHOD(SphereCandidate_IsInReach)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));
		Interactor->Overlap({&Target});

		ASSERT_THAT(IsTrue(Interactor->IsInReach(&Target)));
	}

	TEST_METHOD(NonCandidate_IsNotInReach)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));

		ASSERT_THAT(IsFalse(Interactor->IsInReach(&Target)));
	}

	TEST_METHOD(NullTarget_IsNotInReach)
	{
		ASSERT_THAT(IsFalse(Interactor->IsInReach(nullptr)));
	}

	TEST_METHOD(CandidateTheOverlapDropped_IsNotInReach)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));
		Interactor->Overlap({&Target});
		Interactor->Overlap({});

		ASSERT_THAT(IsFalse(Interactor->IsInReach(&Target)));
	}

	TEST_METHOD(CandidateThePawnWalkedAwayFrom_IsNotInReach)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));
		Interactor->Overlap({&Target});
		Target.SetActorLocation(FVector(400, 0, 0)); // ScanRange + ReachSlack = 350

		ASSERT_THAT(IsFalse(Interactor->IsInReach(&Target)));
	}

	TEST_METHOD(CandidateInsideTheSlack_IsInReach)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));
		Interactor->Overlap({&Target});
		Target.SetActorLocation(FVector(340, 0, 0)); // past ScanRange (250), inside ScanRange + ReachSlack

		ASSERT_THAT(IsTrue(Interactor->IsInReach(&Target)));
	}

	TEST_METHOD(ZeroSlack_UsesScanRangeOnly)
	{
		Interactor->ReachSlack = 0.f;
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));
		Interactor->Overlap({&Target});
		Target.SetActorLocation(FVector(300, 0, 0));

		ASSERT_THAT(IsFalse(Interactor->IsInReach(&Target)));
	}

	TEST_METHOD(PersistentCandidate_SkipsTheDistanceCheck)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(5000, 0, 0));
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Target);
		Entry.OwningActor = &Target;
		Interactor->AddPersistentCandidate(Entry);

		ASSERT_THAT(IsTrue(Interactor->IsInReach(&Target)));
	}

	TEST_METHOD(DestroyedCandidate_IsNotInReach)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));
		Interactor->Overlap({&Target});
		const UObject* Destroyed = &Target;
		Target.Destroy();

		ASSERT_THAT(IsFalse(Interactor->IsInReach(Destroyed)));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
