// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "RockInteractionTestFixture.h"

#include "Components/BoxComponent.h"

// Candidate selection: the candidate list (sphere scan results plus persistent candidates), LookAt scoring, direct hit
// resolution and the visibility-proxy swap. The scan steps are protected, so they are driven through
// URockTestInteractorComponent. The pawn sits at the origin looking along +X with the default 250 range and 3 degree
// threshold, and targets are placed at X=100 with a Y offset that gives the angle under test.

TEST_CLASS(RockInteractorCandidateTests, "BRS.RockInteraction.Interactor.Candidates")
{
	FRockInteractionFixture Fixture;
	APawn* Pawn = nullptr;
	URockTestInteractorComponent* Interactor = nullptr;

	BEFORE_EACH()
	{
		Interactor = &Fixture.SpawnReadyInteractor(Pawn);
	}

	FRockInteractionCandidateEntry Entry(ARockTestInteractable& Target)
	{
		FRockInteractionCandidateEntry Result;
		Result.Target = FRockInteractionFixture::AsTarget(Target);
		Result.OwningActor = &Target;
		return Result;
	}

	TEST_METHOD(AddPersistent_EmptyTarget_IsIgnored)
	{
		Interactor->AddPersistentCandidate(FRockInteractionCandidateEntry());
		ASSERT_THAT(AreEqual(0, Interactor->NumPersistentCandidates()));
	}

	TEST_METHOD(AddPersistent_SameTargetTwice_IsKeptOnce)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));
		Interactor->AddPersistentCandidate(Entry(Target));
		Interactor->AddPersistentCandidate(Entry(Target));
		ASSERT_THAT(AreEqual(1, Interactor->NumPersistentCandidates()));
	}

	TEST_METHOD(RemovePersistent_RemovesIt)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));
		Interactor->AddPersistentCandidate(Entry(Target));
		Interactor->RemovePersistentCandidate(Entry(Target));
		ASSERT_THAT(AreEqual(0, Interactor->NumPersistentCandidates()));
	}

	TEST_METHOD(RemovePersistent_Unknown_IsANoOp)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));
		Interactor->RemovePersistentCandidate(Entry(Target));
		ASSERT_THAT(AreEqual(0, Interactor->NumPersistentCandidates()));
	}

	/** Persistent candidates are relevant regardless of what the sphere scan found. */
	TEST_METHOD(Persistent_BecomesCandidateWithoutAnyOverlap)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));
		Interactor->AddPersistentCandidate(Entry(Target));

		Interactor->Overlap({});

		ASSERT_THAT(AreEqual(1, Interactor->NumCandidates()));
		ASSERT_THAT(AreEqual(1, Interactor->Entered.Num()));
		ASSERT_THAT(IsTrue(Interactor->Entered[0] == &Target));
	}

	TEST_METHOD(Overlap_InteractableActor_BecomesCandidate)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));

		Interactor->Overlap({&Target});

		ASSERT_THAT(AreEqual(1, Interactor->NumCandidates()));
		ASSERT_THAT(AreEqual(1, Interactor->Entered.Num()));
		ASSERT_THAT(IsTrue(Interactor->Entered[0] == &Target));
	}

	TEST_METHOD(Overlap_ActorWithoutTheInterface_IsIgnored)
	{
		AActor& Plain = Fixture.Spawner.SpawnActor<AActor>();

		Interactor->Overlap({&Plain});

		ASSERT_THAT(AreEqual(0, Interactor->NumCandidates()));
		ASSERT_THAT(AreEqual(0, Interactor->Entered.Num()));
	}

	TEST_METHOD(Overlap_ResultWithoutAnActor_IsSkipped)
	{
		Interactor->Overlap({nullptr});
		ASSERT_THAT(AreEqual(0, Interactor->NumCandidates()));
	}

	TEST_METHOD(Overlap_SameActorTwice_IsOneCandidate)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));

		Interactor->Overlap({&Target, &Target});

		ASSERT_THAT(AreEqual(1, Interactor->NumCandidates()));
		ASSERT_THAT(AreEqual(1, Interactor->Entered.Num()));
	}

	TEST_METHOD(Overlap_PersistentAndOverlappingSameActor_IsOneCandidate)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));
		Interactor->AddPersistentCandidate(Entry(Target));

		Interactor->Overlap({&Target});

		ASSERT_THAT(AreEqual(1, Interactor->NumCandidates()));
		ASSERT_THAT(AreEqual(1, Interactor->Entered.Num()));
	}

	TEST_METHOD(Overlap_StayingCandidate_DoesNotEnterAgain)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));
		Interactor->Overlap({&Target});

		Interactor->Overlap({&Target});

		ASSERT_THAT(AreEqual(1, Interactor->Entered.Num()));
		ASSERT_THAT(AreEqual(0, Interactor->Exited.Num()));
	}

	TEST_METHOD(Overlap_CandidateLeaves_ExitsOnce)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));
		Interactor->Overlap({&Target});

		Interactor->Overlap({});
		Interactor->Overlap({});

		ASSERT_THAT(AreEqual(0, Interactor->NumCandidates()));
		ASSERT_THAT(AreEqual(1, Interactor->Exited.Num()));
		ASSERT_THAT(IsTrue(Interactor->Exited[0] == &Target));
	}

	TEST_METHOD(Overlap_Swap_ExitsOldAndEntersNew)
	{
		ARockTestInteractable& Old = Fixture.SpawnInteractable(Ahead(0));
		ARockTestInteractable& New = Fixture.SpawnInteractable(Ahead(1));
		Interactor->Overlap({&Old});

		Interactor->Overlap({&New});

		ASSERT_THAT(AreEqual(2, Interactor->Entered.Num()));
		ASSERT_THAT(AreEqual(1, Interactor->Exited.Num()));
		ASSERT_THAT(IsTrue(Interactor->Exited[0] == &Old));
		ASSERT_THAT(IsTrue(Interactor->Entered[1] == &New));
	}

	TEST_METHOD(Overlap_CallsCandidatesUpdatedEveryTime)
	{
		Interactor->Overlap({});
		Interactor->Overlap({});
		ASSERT_THAT(AreEqual(2, Interactor->CandidatesUpdatedCalls));
	}

	/** bEnableCandidateEnterEvents turns off the enter events only. Exit events keep firing. */
	TEST_METHOD(EnterEventsDisabled_SuppressesEnterOnly)
	{
		Interactor->bEnableCandidateEnterEvents = false;
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));

		Interactor->Overlap({&Target});
		ASSERT_THAT(AreEqual(0, Interactor->Entered.Num()));

		Interactor->Overlap({});
		ASSERT_THAT(AreEqual(1, Interactor->Exited.Num()));
	}

	/** bEnableCandidateExitEvents turns off the exit events only. Enter events keep firing. */
	TEST_METHOD(ExitEventsDisabled_SuppressesExitOnly)
	{
		Interactor->bEnableCandidateExitEvents = false;
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));

		Interactor->Overlap({&Target});
		ASSERT_THAT(AreEqual(1, Interactor->Entered.Num()));

		Interactor->Overlap({});
		ASSERT_THAT(AreEqual(0, Interactor->Exited.Num()));
	}
};

TEST_CLASS(RockInteractorLookAtTests, "BRS.RockInteraction.Interactor.LookAt")
{
	FRockInteractionFixture Fixture;
	APawn* Pawn = nullptr;
	URockTestInteractorComponent* Interactor = nullptr;
	TScriptInterface<IRockInteractableTarget> Result;
	FRockInteractionPoint ResultPoint;

	BEFORE_EACH()
	{
		Interactor = &Fixture.SpawnReadyInteractor(Pawn);
	}

	ARockTestInteractable& Candidate(const FVector& Location)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Location);
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Target);
		Entry.OwningActor = &Target;
		Interactor->AddPersistentCandidate(Entry);
		Interactor->Overlap({});
		return Target;
	}

	bool Run(float ThresholdDegrees = 3.f)
	{
		return Interactor->LookAt(FRockInteractionFixture::MakeScan(ThresholdDegrees), Result, ResultPoint);
	}

	bool Picked(const AActor& Target) const
	{
		return Result.GetObject() == &Target;
	}

	TEST_METHOD(NoCandidates_SelectsNothing)
	{
		ASSERT_THAT(IsFalse(Run()));
		ASSERT_THAT(IsNull(Result.GetObject()));
	}

	// --- Candidates without points: scored by the actor location ---

	TEST_METHOD(ZeroPoints_DeadAhead_IsSelected)
	{
		ARockTestInteractable& Target = Candidate(Ahead(0));

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(Picked(Target)));
	}

	/** The fallback point is the actor's root component at its location, as an interaction point. */
	TEST_METHOD(ZeroPoints_FallbackPointDescribesTheActor)
	{
		ARockTestInteractable& Target = Candidate(Ahead(0));

		Run();

		ASSERT_THAT(IsTrue(ResultPoint.WorldLocation.Equals(Target.GetActorLocation())));
		ASSERT_THAT(IsTrue(ResultPoint.SourceComponent.Get() == Target.GetRootComponent()));
		ASSERT_THAT(IsTrue(ResultPoint.Role == ERockInteractionPointRole::Interaction));
		ASSERT_THAT(IsFalse(ResultPoint.PointTag.IsValid()));
	}

	TEST_METHOD(ZeroPoints_InsideThreshold_IsSelected)
	{
		ARockTestInteractable& Target = Candidate(Ahead(2));
		ASSERT_THAT(IsTrue(Run(3.f)));
		ASSERT_THAT(IsTrue(Picked(Target)));
	}

	TEST_METHOD(ZeroPoints_OutsideThreshold_IsNotSelected)
	{
		Candidate(Ahead(4));
		ASSERT_THAT(IsFalse(Run(3.f)));
	}

	TEST_METHOD(ZeroPoints_ThresholdComesFromTheScanContext)
	{
		ARockTestInteractable& Target = Candidate(Ahead(4));
		ASSERT_THAT(IsTrue(Run(5.f)));
		ASSERT_THAT(IsTrue(Picked(Target)));
	}

	TEST_METHOD(ZeroPoints_BehindTheViewer_IsNotSelected)
	{
		Candidate(FVector(-100, 0, 0));
		ASSERT_THAT(IsFalse(Run(90.f)));
	}

	TEST_METHOD(ZeroPoints_WithoutOwningActor_IsSkipped)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Target);
		Entry.OwningActor = nullptr;
		Interactor->AddPersistentCandidate(Entry);
		Interactor->Overlap({});

		ASSERT_THAT(IsFalse(Run()));
	}

	// --- Candidate gating ---

	TEST_METHOD(NonInteractableCandidate_IsSkipped)
	{
		ARockTestInteractable& Target = Candidate(Ahead(0));
		Target.bInteractable = false;

		ASSERT_THAT(IsFalse(Run()));
	}

	TEST_METHOD(RequiresDirectHit_IsSkippedAndNotEvenGathered)
	{
		ARockTestInteractable& Target = Candidate(Ahead(0));
		Target.bDirectHitOnly = true;

		ASSERT_THAT(IsFalse(Run()));
		ASSERT_THAT(AreEqual(0, Target.GatherPointsCalls));
	}

	TEST_METHOD(ClosestToTheCenterWins)
	{
		Candidate(Ahead(2));
		ARockTestInteractable& Closer = Candidate(Ahead(1));
		Candidate(Ahead(-2.5));

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(Picked(Closer)));
	}

	TEST_METHOD(ExactTie_KeepsTheFirstCandidate)
	{
		ARockTestInteractable& First = Candidate(Ahead(1));
		Candidate(Ahead(-1));

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(Picked(First)));
	}

	// --- Candidates with points: scored per point ---

	TEST_METHOD(Points_AlignedPointWins_AndIsReturned)
	{
		ARockTestInteractable& Target = Candidate(FVector(100, 0, 0));
		Target.Points = {
			FRockInteractionFixture::MakePoint(Ahead(20), RockInteractionTestTags::PointA),
			FRockInteractionFixture::MakePoint(Ahead(1), RockInteractionTestTags::PointB),
		};

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(Picked(Target)));
		ASSERT_THAT(IsTrue(ResultPoint.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointB)));
		ASSERT_THAT(IsTrue(ResultPoint.WorldLocation.Equals(Ahead(1))));
	}

	TEST_METHOD(Points_NoneAligned_SelectsNothing)
	{
		ARockTestInteractable& Target = Candidate(FVector(100, 0, 0));
		Target.Points = {
			FRockInteractionFixture::MakePoint(Ahead(20), RockInteractionTestTags::PointA),
			FRockInteractionFixture::MakePoint(Ahead(-20), RockInteractionTestTags::PointB),
		};

		ASSERT_THAT(IsFalse(Run()));
	}

	TEST_METHOD(Points_BeyondScanRange_AreIgnored)
	{
		ARockTestInteractable& Target = Candidate(FVector(100, 0, 0));
		Target.Points = {FRockInteractionFixture::MakePoint(Ahead(0, 300.0), RockInteractionTestTags::PointA)};

		ASSERT_THAT(IsFalse(Run()));
	}

	TEST_METHOD(Points_WithinScanRange_AreAccepted)
	{
		ARockTestInteractable& Target = Candidate(FVector(100, 0, 0));
		Target.Points = {FRockInteractionFixture::MakePoint(Ahead(0, 240.0), RockInteractionTestTags::PointA)};

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(Picked(Target)));
	}

	/** A point 5 degrees off is outside the 3 degree cone, but a threshold scale of 2 makes its cone 6 degrees. */
	TEST_METHOD(Points_ThresholdScaleWidensTheCone)
	{
		ARockTestInteractable& Target = Candidate(FVector(100, 0, 0));
		FRockInteractionPoint Point = FRockInteractionFixture::MakePoint(Ahead(5), RockInteractionTestTags::PointA);
		Target.Points = {Point};
		ASSERT_THAT(IsFalse(Run(3.f)));

		Target.Points[0].LookAtThresholdScale = 2.f;

		ASSERT_THAT(IsTrue(Run(3.f)));
		ASSERT_THAT(IsTrue(Picked(Target)));
	}

	TEST_METHOD(Points_ThresholdScaleBelowOneNarrowsTheCone)
	{
		ARockTestInteractable& Target = Candidate(FVector(100, 0, 0));
		FRockInteractionPoint Point = FRockInteractionFixture::MakePoint(Ahead(2), RockInteractionTestTags::PointA);
		Point.LookAtThresholdScale = 0.5f;
		Target.Points = {Point};

		ASSERT_THAT(IsFalse(Run(3.f)));
	}

	/** Visibility points take part in scoring so oddly shaped objects get a wider target. The proxy swap happens afterwards. */
	TEST_METHOD(Points_VisibilityPointCanWin)
	{
		ARockTestInteractable& Target = Candidate(FVector(100, 0, 0));
		Target.Points = {
			FRockInteractionFixture::MakePoint(Ahead(30), RockInteractionTestTags::PointA, ERockInteractionPointRole::Interaction),
			FRockInteractionFixture::MakePoint(Ahead(1), RockInteractionTestTags::PointA, ERockInteractionPointRole::Visibility),
		};

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(ResultPoint.Role == ERockInteractionPointRole::Visibility));
	}

	TEST_METHOD(Points_BestPointAcrossCandidatesWins)
	{
		ARockTestInteractable& Far = Candidate(FVector(100, 0, 0));
		Far.Points = {FRockInteractionFixture::MakePoint(Ahead(2), RockInteractionTestTags::PointA)};
		ARockTestInteractable& Near = Candidate(FVector(100, 0, 0));
		Near.Points = {FRockInteractionFixture::MakePoint(Ahead(0.5), RockInteractionTestTags::PointB)};

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(Picked(Near)));
		ASSERT_THAT(IsTrue(ResultPoint.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointB)));
	}
};

TEST_CLASS(RockInteractorDirectHitTests, "BRS.RockInteraction.Interactor.DirectHit")
{
	FRockInteractionFixture Fixture;
	APawn* Pawn = nullptr;
	URockTestInteractorComponent* Interactor = nullptr;
	TScriptInterface<IRockInteractableTarget> Result;
	FRockInteractionPoint ResultPoint;

	BEFORE_EACH()
	{
		Interactor = &Fixture.SpawnReadyInteractor(Pawn);
	}

	ARockTestInteractable& Candidate(const FVector& Location = FVector(100, 0, 0))
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Location);
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Target);
		Entry.OwningActor = &Target;
		Interactor->AddPersistentCandidate(Entry);
		Interactor->Overlap({});
		return Target;
	}

	bool Run(AActor* HitActor, UPrimitiveComponent* HitComp = nullptr)
	{
		FInteractionScanContext Scan = FRockInteractionFixture::MakeScan();
		Scan.HitActor = HitActor;
		Scan.HitComp = HitComp;
		return Interactor->DirectHit(Scan, Result, ResultPoint);
	}

	/** Only identity matters to the resolver (the hit component is compared with each point's source), so the component is not registered. */
	UPrimitiveComponent* AddComponent(ARockTestInteractable& Target)
	{
		return NewObject<UBoxComponent>(&Target);
	}

	TEST_METHOD(NoHitActor_ResolvesNothing)
	{
		Candidate();
		ASSERT_THAT(IsFalse(Run(nullptr)));
		ASSERT_THAT(IsNull(Result.GetObject()));
	}

	TEST_METHOD(HitActorWithoutTheInterface_ResolvesNothing)
	{
		AActor& Plain = Fixture.Spawner.SpawnActor<AActor>();
		ASSERT_THAT(IsFalse(Run(&Plain)));
	}

	TEST_METHOD(HitActorThatIsNotACandidate_ResolvesNothing)
	{
		Candidate();
		ARockTestInteractable& Stranger = Fixture.SpawnInteractable(FVector(50, 0, 0));
		ASSERT_THAT(IsFalse(Run(&Stranger)));
	}

	/** With the sphere scan off there is no candidate list, so the hit actor is wrapped directly. */
	TEST_METHOD(DirectHitOnlyMode_ResolvesAnActorThatIsNotACandidate)
	{
		Interactor->ScanMode = ERockInteractorScanMode::DirectHitOnly;
		ARockTestInteractable& Stranger = Fixture.SpawnInteractable(FVector(50, 0, 0));
		Stranger.Points = {FRockInteractionFixture::MakePoint(FVector(50, 0, 0), RockInteractionTestTags::PointA)};

		ASSERT_THAT(IsTrue(Run(&Stranger)));
		ASSERT_THAT(IsTrue(Result.GetObject() == &Stranger));
	}

	TEST_METHOD(SingleInteractionPoint_ResolvesWithThatPoint)
	{
		ARockTestInteractable& Target = Candidate();
		Target.Points = {FRockInteractionFixture::MakePoint(FVector(100, 5, 0), RockInteractionTestTags::PointA)};

		ASSERT_THAT(IsTrue(Run(&Target)));
		ASSERT_THAT(IsTrue(Result.GetObject() == &Target));
		ASSERT_THAT(IsTrue(ResultPoint.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointA)));
	}

	TEST_METHOD(SingleInteractionPoint_ResolvesEvenWhenTheHitComponentIsOther)
	{
		ARockTestInteractable& Target = Candidate();
		UPrimitiveComponent* PointComponent = AddComponent(Target);
		UPrimitiveComponent* HitComponent = AddComponent(Target);
		Target.Points = {FRockInteractionFixture::MakePoint(FVector(100, 5, 0), RockInteractionTestTags::PointA, ERockInteractionPointRole::Interaction, PointComponent)};

		ASSERT_THAT(IsTrue(Run(&Target, HitComponent)));
		ASSERT_THAT(IsTrue(ResultPoint.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointA)));
	}

	/** An interactable with no points is selected by hitting it, with an empty point. */
	TEST_METHOD(NoPoints_ResolvesWithAnEmptyPoint)
	{
		ARockTestInteractable& Target = Candidate();

		ASSERT_THAT(IsTrue(Run(&Target)));
		ASSERT_THAT(IsTrue(Result.GetObject() == &Target));
		ASSERT_THAT(IsFalse(ResultPoint.PointTag.IsValid()));
		ASSERT_THAT(IsFalse(ResultPoint.SourceComponent.IsValid()));
	}

	/** Visibility points do not count toward the interaction point total. */
	TEST_METHOD(OnlyVisibilityPoints_ResolvesWithAnEmptyPoint)
	{
		ARockTestInteractable& Target = Candidate();
		Target.Points = {FRockInteractionFixture::MakePoint(FVector(100, 5, 0), RockInteractionTestTags::PointA, ERockInteractionPointRole::Visibility)};

		ASSERT_THAT(IsTrue(Run(&Target)));
		ASSERT_THAT(IsFalse(ResultPoint.PointTag.IsValid()));
	}

	TEST_METHOD(VisibilityPointsAreNotCounted_WhenChoosingTheSingleInteractionPoint)
	{
		ARockTestInteractable& Target = Candidate();
		Target.Points = {
			FRockInteractionFixture::MakePoint(FVector(100, 5, 0), RockInteractionTestTags::PointA, ERockInteractionPointRole::Visibility),
			FRockInteractionFixture::MakePoint(FVector(100, 9, 0), RockInteractionTestTags::PointB, ERockInteractionPointRole::Interaction),
		};

		ASSERT_THAT(IsTrue(Run(&Target)));
		ASSERT_THAT(IsTrue(ResultPoint.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointB)));
	}

	/** A hit on a non-interactable target ends selection. It does not fall through to LookAt. */
	TEST_METHOD(NonInteractableTarget_ResolvesNothing)
	{
		ARockTestInteractable& Target = Candidate();
		Target.bInteractable = false;

		ASSERT_THAT(IsFalse(Run(&Target)));
	}

	TEST_METHOD(TwoPointsOnDistinctComponents_HitComponentPicksThePoint)
	{
		ARockTestInteractable& Target = Candidate();
		UPrimitiveComponent* First = AddComponent(Target);
		UPrimitiveComponent* Second = AddComponent(Target);
		Target.Points = {
			FRockInteractionFixture::MakePoint(FVector(100, 5, 0), RockInteractionTestTags::PointA, ERockInteractionPointRole::Interaction, First),
			FRockInteractionFixture::MakePoint(FVector(100, 9, 0), RockInteractionTestTags::PointB, ERockInteractionPointRole::Interaction, Second),
		};

		ASSERT_THAT(IsTrue(Run(&Target, Second)));
		ASSERT_THAT(IsTrue(ResultPoint.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointB)));
	}

	/** Two points on the one component that was hit cannot be told apart by the hit, so it is ambiguous too. */
	TEST_METHOD(TwoPointsOnTheHitComponent_IsAmbiguous)
	{
		ARockTestInteractable& Target = Candidate();
		UPrimitiveComponent* Shared = AddComponent(Target);
		Target.Points = {
			FRockInteractionFixture::MakePoint(FVector(100, 5, 0), RockInteractionTestTags::PointA, ERockInteractionPointRole::Interaction, Shared),
			FRockInteractionFixture::MakePoint(FVector(100, 9, 0), RockInteractionTestTags::PointB, ERockInteractionPointRole::Interaction, Shared),
		};

		ASSERT_THAT(IsFalse(Run(&Target, Shared)));
	}

	/** Two interaction points and no unique component match is ambiguous: the direct hit is ignored so LookAt can decide. */
	TEST_METHOD(TwoPointsAndNoHitComponent_IsAmbiguous)
	{
		ARockTestInteractable& Target = Candidate();
		Target.Points = {
			FRockInteractionFixture::MakePoint(FVector(100, 5, 0), RockInteractionTestTags::PointA),
			FRockInteractionFixture::MakePoint(FVector(100, 9, 0), RockInteractionTestTags::PointB),
		};

		ASSERT_THAT(IsFalse(Run(&Target)));
		ASSERT_THAT(IsNull(Result.GetObject()));
	}
};

TEST_CLASS(RockInteractorVisibilityProxyTests, "BRS.RockInteraction.Interactor.VisibilityProxy")
{
	FRockInteractionFixture Fixture;
	APawn* Pawn = nullptr;
	URockTestInteractorComponent* Interactor = nullptr;

	BEFORE_EACH()
	{
		Interactor = &Fixture.SpawnReadyInteractor(Pawn);
	}

	TEST_METHOD(InteractionPoint_IsLeftAlone_WithoutGathering)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));
		TScriptInterface<IRockInteractableTarget> Winner = FRockInteractionFixture::AsTarget(Target);
		FRockInteractionPoint Point = FRockInteractionFixture::MakePoint(FVector(1, 2, 3), RockInteractionTestTags::PointA);

		Interactor->ResolveProxy(Winner, Point);

		ASSERT_THAT(IsTrue(Point.WorldLocation.Equals(FVector(1, 2, 3))));
		ASSERT_THAT(AreEqual(0, Target.GatherPointsCalls));
	}

	/** An IX_VP_ proxy resolves back to the interaction point with the same tag, so the UI and ability see the real point. */
	TEST_METHOD(VisibilityPoint_IsReplacedByTheInteractionPointWithTheSameTag)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));
		Target.Points = {
			FRockInteractionFixture::MakePoint(FVector(100, 0, 50), RockInteractionTestTags::PointB, ERockInteractionPointRole::Interaction),
			FRockInteractionFixture::MakePoint(FVector(100, 0, 10), RockInteractionTestTags::PointA, ERockInteractionPointRole::Interaction),
			FRockInteractionFixture::MakePoint(FVector(100, 0, 20), RockInteractionTestTags::PointA, ERockInteractionPointRole::Visibility),
		};
		TScriptInterface<IRockInteractableTarget> Winner = FRockInteractionFixture::AsTarget(Target);
		FRockInteractionPoint Proxy = Target.Points[2];

		Interactor->ResolveProxy(Winner, Proxy);

		ASSERT_THAT(IsTrue(Proxy.Role == ERockInteractionPointRole::Interaction));
		ASSERT_THAT(IsTrue(Proxy.WorldLocation.Equals(FVector(100, 0, 10))));
	}

	TEST_METHOD(VisibilityPoint_WithoutAMatchingInteractionPoint_StaysAProxy)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector(100, 0, 0));
		Target.Points = {
			FRockInteractionFixture::MakePoint(FVector(100, 0, 10), RockInteractionTestTags::PointB, ERockInteractionPointRole::Interaction),
			FRockInteractionFixture::MakePoint(FVector(100, 0, 20), RockInteractionTestTags::PointA, ERockInteractionPointRole::Visibility),
		};
		TScriptInterface<IRockInteractableTarget> Winner = FRockInteractionFixture::AsTarget(Target);
		FRockInteractionPoint Proxy = Target.Points[1];

		Interactor->ResolveProxy(Winner, Proxy);

		ASSERT_THAT(IsTrue(Proxy.Role == ERockInteractionPointRole::Visibility));
		ASSERT_THAT(IsTrue(Proxy.WorldLocation.Equals(FVector(100, 0, 20))));
	}
};

// Line of sight for LookAt focus (T-113). The trace is the virtual IsHintPointVisible, stubbed by BlockedLocations.
TEST_CLASS(RockInteractorLineOfSightTests, "BRS.RockInteraction.Interactor.LookAt.LineOfSight")
{
	FRockInteractionFixture Fixture;
	APawn* Pawn = nullptr;
	URockTestInteractorComponent* Interactor = nullptr;
	TScriptInterface<IRockInteractableTarget> Result;
	FRockInteractionPoint ResultPoint;

	BEFORE_EACH()
	{
		Interactor = &Fixture.SpawnReadyInteractor(Pawn);
	}

	ARockTestInteractable& Candidate(const FVector& Location)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Location);
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Target);
		Entry.OwningActor = &Target;
		Interactor->AddPersistentCandidate(Entry);
		Interactor->Overlap({});
		return Target;
	}

	bool Run()
	{
		return Interactor->LookAt(FRockInteractionFixture::MakeScan(), Result, ResultPoint);
	}

	TEST_METHOD(BlockedWinner_IsNotSelected)
	{
		ARockTestInteractable& Target = Candidate(Ahead(0));
		Interactor->BlockedLocations = {Target.GetActorLocation()};

		ASSERT_THAT(IsFalse(Run()));
		ASSERT_THAT(IsNull(Result.GetObject()));
	}

	TEST_METHOD(BlockedWinner_GivesWayToTheNextBest)
	{
		ARockTestInteractable& Blocked = Candidate(Ahead(0.5));
		ARockTestInteractable& Open = Candidate(Ahead(2));
		Interactor->BlockedLocations = {Blocked.GetActorLocation()};

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(Result.GetObject() == &Open));
	}

	TEST_METHOD(VisibleWinner_CostsOneTrace)
	{
		Candidate(Ahead(0.5));
		Candidate(Ahead(2));

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(AreEqual(1, Interactor->VisibilityTraceCalls));
	}

	TEST_METHOD(Traces_AreCappedPerPass)
	{
		Interactor->FocusLineOfSightTraces = 2;
		for (double Angle : {0.2, 0.4, 0.6, 0.8})
		{
			ARockTestInteractable& Target = Candidate(Ahead(Angle));
			Interactor->BlockedLocations.Add(Target.GetActorLocation());
		}
		// A fifth, open candidate is further from the centre than the traced ones.
		Candidate(Ahead(2));

		ASSERT_THAT(IsFalse(Run()));
		ASSERT_THAT(AreEqual(2, Interactor->VisibilityTraceCalls));
	}

	TEST_METHOD(Disabled_SelectsABlockedWinnerWithoutTracing)
	{
		Interactor->bFocusRequiresLineOfSight = false;
		ARockTestInteractable& Target = Candidate(Ahead(0));
		Interactor->BlockedLocations = {Target.GetActorLocation()};

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(Result.GetObject() == &Target));
		ASSERT_THAT(AreEqual(0, Interactor->VisibilityTraceCalls));
	}

	TEST_METHOD(OnlyThePointTraced_NotItsSiblings)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(Ahead(0));
		Target.Points = {
			FRockInteractionFixture::MakePoint(Ahead(0.5), RockInteractionTestTags::PointA),
			FRockInteractionFixture::MakePoint(Ahead(1.5), RockInteractionTestTags::PointB),
		};
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Target);
		Entry.OwningActor = &Target;
		Interactor->AddPersistentCandidate(Entry);
		Interactor->Overlap({});
		Interactor->BlockedLocations = {Ahead(0.5)};

		ASSERT_THAT(IsTrue(Run()));
		ASSERT_THAT(IsTrue(ResultPoint.WorldLocation.Equals(Ahead(1.5))));
	}

	/** The whole pass: a blocked winner does not take focus. */
	TEST_METHOD(ScorePass_BlockedWinner_DoesNotFocus)
	{
		ARockTestInteractable& Target = Candidate(Ahead(0));
		Target.Options = {FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionA)};
		Interactor->BlockedLocations = {Target.GetActorLocation()};

		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
