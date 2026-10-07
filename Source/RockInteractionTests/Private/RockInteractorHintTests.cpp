// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "RockInteractionTestFixture.h"

// The hint list: Interaction-role points of candidates near the pawn, the MaxHints closest to the centre of the view, with the
// focused one flagged and a visibility flag filled by a (stubbed) trace. The pawn is at the origin looking along +X; the default
// range is ScanRange (250) and Ahead(deg) places a point 100 units out, deg off the view axis.

TEST_CLASS(RockInteractorHintTests, "BRS.RockInteraction.Hints")
{
	FRockInteractionFixture Fixture;
	APawn* Pawn = nullptr;
	URockTestInteractorComponent* Interactor = nullptr;

	BEFORE_EACH()
	{
		Interactor = &Fixture.SpawnReadyInteractor(Pawn);
		Interactor->bEnableHints = true;
		Interactor->bTraceHintVisibility = false;
	}

	/** An interactable with one Interaction point per location, registered as a candidate. */
	ARockTestInteractable& Target(const TArray<FVector>& Locations, const FName& Tag = RockInteractionTestTags::PointA)
	{
		ARockTestInteractable& Result = Fixture.SpawnInteractable(Locations.IsEmpty() ? FVector(100, 0, 0) : Locations[0]);
		for (const FVector& Location : Locations)
		{
			Result.Points.Add(FRockInteractionFixture::MakePoint(Location, Tag));
		}
		Result.Options.Add(FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionA));
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Result);
		Entry.OwningActor = &Result;
		Interactor->AddPersistentCandidate(Entry);
		Interactor->Overlap({});
		return Result;
	}

	ARockTestInteractable& Target(const FVector& Location, const FName& Tag = RockInteractionTestTags::PointA)
	{
		return Target(TArray<FVector>{Location}, Tag);
	}

	TEST_METHOD(Disabled_ListStaysEmpty)
	{
		Interactor->bEnableHints = false;
		Target(Ahead(0));

		Interactor->HintPass();

		ASSERT_THAT(AreEqual(0, Interactor->GetHintPoints().Num()));
	}

	TEST_METHOD(DisablingHints_ClearsTheList)
	{
		Target(Ahead(0));
		Interactor->HintPass();
		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));

		Interactor->bEnableHints = false;
		Interactor->HintPass();

		ASSERT_THAT(AreEqual(0, Interactor->GetHintPoints().Num()));
	}

	TEST_METHOD(InteractionPoint_IsListedWithItsData)
	{
		ARockTestInteractable& Owner = Target(Ahead(10, 100.0));

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
		const FRockInteractionHintPoint& Hint = Interactor->GetHintPoints()[0];
		ASSERT_THAT(IsTrue(Hint.OwningActor.Get() == &Owner));
		ASSERT_THAT(IsTrue(Hint.Point.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointA)));
		ASSERT_THAT(IsNear(Hint.AimAngleDegrees, 10.f, 0.01f));
		ASSERT_THAT(IsNear(Hint.Distance, static_cast<float>(Ahead(10, 100.0).Size()), 0.01f));
		ASSERT_THAT(IsFalse(Hint.bFocused));
	}

	TEST_METHOD(VisibilityProxyPoints_AreNotListed)
	{
		ARockTestInteractable& Owner = Target(Ahead(0));
		Owner.Points.Add(FRockInteractionFixture::MakePoint(Ahead(5), RockInteractionTestTags::PointB, ERockInteractionPointRole::Visibility));

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].Point.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointA)));
	}

	TEST_METHOD(NotInteractableTarget_IsNotListed)
	{
		ARockTestInteractable& Closed = Target(Ahead(0));
		Closed.bInteractable = false;
		Target(Ahead(5), RockInteractionTestTags::PointB);

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].Point.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointB)));
	}

	/** No points of its own: marked at the actor, as LookAt scores it. */
	TEST_METHOD(ZeroPointTarget_IsListedAtTheActor)
	{
		ARockTestInteractable& Owner = Fixture.SpawnInteractable(Ahead(0));
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Owner);
		Entry.OwningActor = &Owner;
		Interactor->AddPersistentCandidate(Entry);
		Interactor->Overlap({});

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].Point.WorldLocation.Equals(Ahead(0), 0.01)));
		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].OwningActor.Get() == &Owner));
	}

	TEST_METHOD(DirectHitOnlyTarget_IsListed)
	{
		ARockTestInteractable& Door = Target(Ahead(0));
		Door.bDirectHitOnly = true;

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
	}

	TEST_METHOD(PointBeyondRange_IsCutButItsNeighbourStays)
	{
		Target({Ahead(0, 100.0), Ahead(0, 400.0)});

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
		ASSERT_THAT(IsNear(Interactor->GetHintPoints()[0].Distance, 100.f, 0.01f));
	}

	TEST_METHOD(HintRange_ReplacesScanRange)
	{
		Interactor->HintRange = 50.f;
		Target(Ahead(0, 100.0));
		ASSERT_THAT(IsTrue(Interactor->ScanRange > 100.f));

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(0, Interactor->GetHintPoints().Num()));
	}

	TEST_METHOD(ZeroHintRange_UsesScanRange)
	{
		Interactor->HintRange = 0.f;
		Target(Ahead(0, Interactor->ScanRange - 10.f));
		Target(Ahead(5, Interactor->ScanRange + 10.f));

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
	}

	TEST_METHOD(ListIsOrderedByAngleFromTheViewCentre)
	{
		Target(Ahead(30));
		Target(Ahead(-5));
		Target(Ahead(15));

		Interactor->RefreshHints();

		const TArray<FRockInteractionHintPoint>& Hints = Interactor->GetHintPoints();
		ASSERT_THAT(AreEqual(3, Hints.Num()));
		ASSERT_THAT(IsNear(Hints[0].AimAngleDegrees, 5.f, 0.01f));
		ASSERT_THAT(IsNear(Hints[1].AimAngleDegrees, 15.f, 0.01f));
		ASSERT_THAT(IsNear(Hints[2].AimAngleDegrees, 30.f, 0.01f));
	}

	/** The cap keeps the points nearest the centre of the view, not the nearest to the pawn. */
	TEST_METHOD(Cap_KeepsTheClosestToTheViewCentre)
	{
		Interactor->MaxHints = 2;
		Target(Ahead(40, 50.0));
		Target(Ahead(2, 200.0));
		Target(Ahead(20, 60.0));
		Target(Ahead(8, 220.0));

		Interactor->RefreshHints();

		const TArray<FRockInteractionHintPoint>& Hints = Interactor->GetHintPoints();
		ASSERT_THAT(AreEqual(2, Hints.Num()));
		ASSERT_THAT(IsNear(Hints[0].AimAngleDegrees, 2.f, 0.01f));
		ASSERT_THAT(IsNear(Hints[1].AimAngleDegrees, 8.f, 0.01f));
	}

	TEST_METHOD(PointBehindTheView_IsNotListed)
	{
		Target(FVector(-100, 0, 0));
		Target(Ahead(0));

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
	}

	TEST_METHOD(PointOutsideTheAimCone_IsNotListed)
	{
		Interactor->HintMaxAimDegrees = 40.f;
		Target(Ahead(60));
		Target(Ahead(30));

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
		ASSERT_THAT(IsNear(Interactor->GetHintPoints()[0].AimAngleDegrees, 30.f, 0.01f));
	}

	TEST_METHOD(DestroyedTarget_IsNotListed)
	{
		ARockTestInteractable& Gone = Target(Ahead(0));
		Target(Ahead(5), RockInteractionTestTags::PointB);
		Gone.Destroy();

		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));
		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].Point.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointB)));
	}

	TEST_METHOD(FocusedPoint_IsFlagged)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Target(Ahead(20), RockInteractionTestTags::PointB);

		Interactor->ScorePass();
		ASSERT_THAT(IsTrue(Interactor->HasFocus()));
		Interactor->HintPass();

		const TArray<FRockInteractionHintPoint>& Hints = Interactor->GetHintPoints();
		ASSERT_THAT(AreEqual(2, Hints.Num()));
		ASSERT_THAT(IsTrue(Hints[0].OwningActor.Get() == &Focused));
		ASSERT_THAT(IsTrue(Hints[0].bFocused));
		ASSERT_THAT(IsFalse(Hints[1].bFocused));
	}

	/** Only the point with focus is flagged, not every point of the focused target. */
	TEST_METHOD(FocusedTargetWithTwoPoints_FlagsOnlyTheFocusedPoint)
	{
		ARockTestInteractable& Focused = Target({Ahead(0), Ahead(20)});
		Focused.Points[1].PointTag = RockInteractionTestTags::Get(RockInteractionTestTags::PointB);

		Interactor->ScorePass();
		Interactor->HintPass();

		const TArray<FRockInteractionHintPoint>& Hints = Interactor->GetHintPoints();
		ASSERT_THAT(AreEqual(2, Hints.Num()));
		ASSERT_THAT(IsTrue(Hints[0].bFocused));
		ASSERT_THAT(IsFalse(Hints[1].bFocused));
	}

	/** The flag follows focus on every pass, between list refreshes. */
	TEST_METHOD(FocusFlag_FollowsFocusBetweenRefreshes)
	{
		Target(Ahead(0));
		Target(Ahead(20), RockInteractionTestTags::PointB);
		Pawn->GetWorld()->TimeSeconds = 1.0;
		Interactor->HintPass();
		ASSERT_THAT(IsFalse(Interactor->GetHintPoints()[0].bFocused));

		Interactor->ScorePass();
		Interactor->HintPass(); // same time: no refresh, flags only

		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].bFocused));

		Pawn->GetController()->SetControlRotation(FRotator(0, 90, 0));
		Interactor->ScorePass();
		Interactor->HintPass();

		ASSERT_THAT(IsFalse(Interactor->GetHintPoints()[0].bFocused));
	}

	TEST_METHOD(RefreshRate_GatersTheRebuild)
	{
		Interactor->HintRefreshRate = 0.5f;
		ARockTestInteractable& Owner = Target(Ahead(0));
		UWorld* World = Pawn->GetWorld();

		World->TimeSeconds = 1.0;
		Interactor->HintPass();
		const int32 AfterFirst = Owner.GatherPointsCalls;
		World->TimeSeconds = 1.2;
		Interactor->HintPass();
		ASSERT_THAT(AreEqual(AfterFirst, Owner.GatherPointsCalls));

		World->TimeSeconds = 1.6;
		Interactor->HintPass();
		ASSERT_THAT(IsTrue(Owner.GatherPointsCalls > AfterFirst));
	}

	TEST_METHOD(WithoutVisibilityTracing_ListedPointsAreVisibleAndUntraced)
	{
		Interactor->bTraceHintVisibility = false;
		Interactor->BlockedLocations.Add(Ahead(0));
		Target(Ahead(0));

		Interactor->HintPass();

		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].bVisible));
		ASSERT_THAT(AreEqual(0, Interactor->VisibilityTraceCalls));
	}

	TEST_METHOD(NewPoint_IsHiddenUntilItHasBeenTraced)
	{
		Interactor->bTraceHintVisibility = true;
		Target(Ahead(0));

		Interactor->RefreshHints();
		ASSERT_THAT(IsFalse(Interactor->GetHintPoints()[0].bVisible));

		Interactor->TraceHints();
		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].bVisible));
	}

	TEST_METHOD(BlockedPoint_StaysHidden)
	{
		Interactor->bTraceHintVisibility = true;
		Interactor->BlockedLocations.Add(Ahead(0));
		Target(Ahead(0));
		Target(Ahead(10), RockInteractionTestTags::PointB);

		Interactor->RefreshHints();
		Interactor->TraceHints();

		const TArray<FRockInteractionHintPoint>& Hints = Interactor->GetHintPoints();
		ASSERT_THAT(AreEqual(2, Hints.Num()));
		ASSERT_THAT(IsFalse(Hints[0].bVisible));
		ASSERT_THAT(IsTrue(Hints[1].bVisible));
	}

	/** Four listed points, two traces per pass: every point is traced once after two passes, none twice before that. */
	TEST_METHOD(TraceBudget_RoundRobinsOverTheList)
	{
		Interactor->bTraceHintVisibility = true;
		Interactor->HintTracesPerPass = 2;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Target(Ahead(Index * 5.0));
		}
		Interactor->RefreshHints();

		Interactor->TraceHints();
		ASSERT_THAT(AreEqual(2, Interactor->VisibilityTraceCalls));
		const TArray<FRockInteractionHintPoint>& Hints = Interactor->GetHintPoints();
		ASSERT_THAT(IsTrue(Hints[0].bVisible && Hints[1].bVisible));
		ASSERT_THAT(IsFalse(Hints[2].bVisible || Hints[3].bVisible));

		Interactor->TraceHints();
		ASSERT_THAT(AreEqual(4, Interactor->VisibilityTraceCalls));
		ASSERT_THAT(IsTrue(Hints[2].bVisible && Hints[3].bVisible));
	}

	TEST_METHOD(TraceBudget_IsCappedByTheListSize)
	{
		Interactor->bTraceHintVisibility = true;
		Interactor->HintTracesPerPass = 5;
		Target(Ahead(0));
		Interactor->RefreshHints();

		Interactor->TraceHints();

		ASSERT_THAT(AreEqual(1, Interactor->VisibilityTraceCalls));
	}

	/** A point that stays listed keeps its last result across a rebuild; it does not flash hidden. */
	TEST_METHOD(Refresh_KeepsTheVisibilityOfPointsThatStay)
	{
		Interactor->bTraceHintVisibility = true;
		Target(Ahead(0));
		Interactor->RefreshHints();
		Interactor->TraceHints();
		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].bVisible));

		Interactor->RefreshHints();

		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].bVisible));
	}

	/** The visibility result is replaced by the next trace: a point that becomes blocked hides again. */
	TEST_METHOD(PointThatBecomesBlocked_HidesAgain)
	{
		Interactor->bTraceHintVisibility = true;
		Target(Ahead(0));
		Interactor->RefreshHints();
		Interactor->TraceHints();
		ASSERT_THAT(IsTrue(Interactor->GetHintPoints()[0].bVisible));

		Interactor->BlockedLocations.Add(Ahead(0));
		Interactor->TraceHints();

		ASSERT_THAT(IsFalse(Interactor->GetHintPoints()[0].bVisible));
	}

	TEST_METHOD(NoViewPoint_ClearsTheList)
	{
		Target(Ahead(0));
		Interactor->RefreshHints();
		ASSERT_THAT(AreEqual(1, Interactor->GetHintPoints().Num()));

		Pawn->GetController()->UnPossess();
		Interactor->RefreshHints();

		ASSERT_THAT(AreEqual(0, Interactor->GetHintPoints().Num()));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
