// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "RockInteractionTestFixture.h"

// The per-frame flow: one scoring pass (what the primary tick runs) picks a target, gathers its options, commits focus and
// broadcasts. Then TriggerInteraction. The pawn is possessed by an AI controller, so the view is the pawn at the origin
// looking along the control rotation. The test world has no physics hits, so every selection here goes through LookAt.

TEST_CLASS(RockInteractorFocusTests, "BRS.RockInteraction.Interactor.Focus")
{
	FRockInteractionFixture Fixture;
	APawn* Pawn = nullptr;
	URockTestInteractorComponent* Interactor = nullptr;
	URockTestInteractorListener* Listener = nullptr;

	BEFORE_EACH()
	{
		Interactor = &Fixture.SpawnReadyInteractor(Pawn);
		Listener = Fixture.MakeObject<URockTestInteractorListener>();
		Listener->Bind(*Interactor);
	}

	/** An interactable ahead of the pawn that offers the given options, registered as a candidate. */
	ARockTestInteractable& Target(const FVector& Location, const TArray<FName>& OptionTags = {RockInteractionTestTags::OptionA})
	{
		ARockTestInteractable& Result = Fixture.SpawnInteractable(Location);
		for (const FName& Tag : OptionTags)
		{
			Result.Options.Add(FRockInteractionFixture::MakeOption(Tag));
		}
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Result);
		Entry.OwningActor = &Result;
		Interactor->AddPersistentCandidate(Entry);
		Interactor->Overlap({});
		return Result;
	}

	void Look(double Yaw)
	{
		Pawn->GetController()->SetControlRotation(FRotator(0, Yaw, 0));
	}

	TEST_METHOD(NothingInView_NoFocus)
	{
		Target(Ahead(30));

		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
		ASSERT_THAT(AreEqual(0, Interactor->GetOptionCount()));
		ASSERT_THAT(AreEqual(0, Listener->FocusChanged));
	}

	TEST_METHOD(TargetInView_GainsFocusAndOptions)
	{
		ARockTestInteractable& Focused = Target(Ahead(0), {RockInteractionTestTags::OptionA, RockInteractionTestTags::OptionB});

		Interactor->ScorePass();

		ASSERT_THAT(IsTrue(Interactor->HasFocus()));
		ASSERT_THAT(AreEqual(2, Interactor->GetOptionCount()));
		ASSERT_THAT(IsTrue(Interactor->GetFocusedContext().Target.GetObject() == &Focused));
		ASSERT_THAT(IsTrue(Interactor->GetFocusedContext().IsValid()));
	}

	TEST_METHOD(GainingFocus_BroadcastsFocusAndOptionsOnce)
	{
		Target(Ahead(0), {RockInteractionTestTags::OptionA, RockInteractionTestTags::OptionB});

		Interactor->ScorePass();

		ASSERT_THAT(AreEqual(1, Listener->FocusChanged));
		ASSERT_THAT(IsTrue(Listener->bLastFocusHadTarget));
		ASSERT_THAT(AreEqual(1, Listener->OptionsChanged));
		ASSERT_THAT(AreEqual(2, Listener->LastOptionCount));
	}

	TEST_METHOD(OptionsAreGatheredWithTheWinningContext)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Focused.Points = {FRockInteractionFixture::MakePoint(Ahead(0), RockInteractionTestTags::PointA)};
		Interactor->QueryInteractionTags.AddTag(RockInteractionTestTags::Get(RockInteractionTestTags::PointB));

		Interactor->ScorePass();

		ASSERT_THAT(AreEqual(1, Focused.GatherOptionsCalls));
		ASSERT_THAT(IsTrue(Focused.LastOptionsContext.Target.GetObject() == &Focused));
		ASSERT_THAT(IsTrue(Focused.LastOptionsContext.Point.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointA)));
		ASSERT_THAT(IsTrue(Focused.LastOptionsContext.Query.Instigator.Get() == Pawn));
	}

	/** The query handed to GatherInteractionPoints names the pawn and carries the component's query tags. */
	TEST_METHOD(Query_CarriesInstigatorAndTags)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		const FGameplayTag Tag = RockInteractionTestTags::Get(RockInteractionTestTags::PointB);
		Interactor->QueryInteractionTags.AddTag(Tag);

		Interactor->ScorePass();

		ASSERT_THAT(IsTrue(Focused.LastPointsQuery.Instigator.Get() == Pawn));
		ASSERT_THAT(IsTrue(Focused.LastPointsQuery.InteractionTags.HasTagExact(Tag)));
		ASSERT_THAT(IsTrue(Interactor->CallBuildQuery().GetInstigatorPawn() == Pawn));
	}

	TEST_METHOD(SameTargetAndPoint_DoesNotGatherOrBroadcastAgain)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Interactor->ScorePass();

		Interactor->ScorePass();
		Interactor->ScorePass();

		ASSERT_THAT(AreEqual(1, Focused.GatherOptionsCalls));
		ASSERT_THAT(AreEqual(1, Listener->FocusChanged));
		ASSERT_THAT(AreEqual(1, Listener->OptionsChanged));
	}

	TEST_METHOD(LookingAway_ClearsFocusAndBroadcasts)
	{
		Target(Ahead(0));
		Interactor->ScorePass();

		Look(90);
		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
		ASSERT_THAT(AreEqual(0, Interactor->GetOptionCount()));
		ASSERT_THAT(IsFalse(Interactor->GetFocusedContext().IsValid()));
		ASSERT_THAT(AreEqual(2, Listener->FocusChanged));
		ASSERT_THAT(IsFalse(Listener->bLastFocusHadTarget));
		ASSERT_THAT(AreEqual(2, Listener->OptionsChanged));
		ASSERT_THAT(AreEqual(0, Listener->LastOptionCount));
	}

	TEST_METHOD(ClearingFocus_WhenNothingIsFocused_DoesNotBroadcast)
	{
		Look(90);
		Interactor->ScorePass();
		Interactor->ScorePass();

		ASSERT_THAT(AreEqual(0, Listener->FocusChanged));
		ASSERT_THAT(AreEqual(0, Listener->OptionsChanged));
	}

	TEST_METHOD(LosingTheController_ClearsFocus)
	{
		Target(Ahead(0));
		Interactor->ScorePass();
		ASSERT_THAT(IsTrue(Interactor->HasFocus()));

		Pawn->GetController()->UnPossess();
		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
	}

	TEST_METHOD(CandidatesEmptied_ClearsFocusWithoutAScoringPass)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Interactor->ScorePass();
		ASSERT_THAT(IsTrue(Interactor->HasFocus()));

		Interactor->RemovePersistentCandidate(FRockInteractionCandidateEntry{FRockInteractionFixture::AsTarget(Focused), &Focused});
		Interactor->Overlap({});

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
		ASSERT_THAT(AreEqual(2, Listener->FocusChanged));
	}

	TEST_METHOD(TurningToAnotherTarget_MovesFocus)
	{
		ARockTestInteractable& Ahead0 = Target(FVector(100, 0, 0), {RockInteractionTestTags::OptionA});
		ARockTestInteractable& Side = Target(FVector(0, 100, 0), {RockInteractionTestTags::OptionA, RockInteractionTestTags::OptionB});
		Interactor->ScorePass();
		ASSERT_THAT(IsTrue(Interactor->GetFocusedContext().Target.GetObject() == &Ahead0));

		Look(90);
		Interactor->ScorePass();

		ASSERT_THAT(IsTrue(Interactor->GetFocusedContext().Target.GetObject() == &Side));
		ASSERT_THAT(AreEqual(2, Interactor->GetOptionCount()));
		ASSERT_THAT(AreEqual(2, Listener->FocusChanged));
	}

	/** Same target, different point (a lever and a button on one panel): options are gathered again for the new point. */
	TEST_METHOD(SameTargetNewPoint_RegathersOptions)
	{
		ARockTestInteractable& Panel = Target(FVector(100, 0, 0));
		Panel.Points = {
			FRockInteractionFixture::MakePoint(FVector(100, 0, 0), RockInteractionTestTags::PointA),
			FRockInteractionFixture::MakePoint(FVector(0, 100, 0), RockInteractionTestTags::PointB),
		};
		Interactor->ScorePass();
		ASSERT_THAT(IsTrue(Panel.LastOptionsContext.Point.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointA)));

		Look(90);
		Interactor->ScorePass();

		ASSERT_THAT(IsTrue(Interactor->HasFocus()));
		ASSERT_THAT(AreEqual(2, Panel.GatherOptionsCalls));
		ASSERT_THAT(IsTrue(Panel.LastOptionsContext.Point.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointB)));
		ASSERT_THAT(IsTrue(Interactor->GetFocusedContext().Point.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointB)));
		ASSERT_THAT(AreEqual(2, Listener->FocusChanged));
	}

	/** A target that wins the scoring but offers nothing is not focused, and nobody is told about it. */
	TEST_METHOD(TargetWithoutOptions_IsNotFocused)
	{
		Target(Ahead(0), {});
		TestRunner->AddExpectedMessagePlain(TEXT("returned no options"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);

		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
		ASSERT_THAT(AreEqual(0, Listener->FocusChanged));
		ASSERT_THAT(AreEqual(0, Listener->OptionsChanged));
	}

	/** Focus on a target that then loses all its options on a re-score (a different point) is dropped. */
	TEST_METHOD(FocusedTargetThatNowHasNoOptionsForANewPoint_LosesFocus)
	{
		ARockTestInteractable& Panel = Target(FVector(100, 0, 0));
		Panel.Points = {
			FRockInteractionFixture::MakePoint(FVector(100, 0, 0), RockInteractionTestTags::PointA),
			FRockInteractionFixture::MakePoint(FVector(0, 100, 0), RockInteractionTestTags::PointB),
		};
		Interactor->ScorePass();
		ASSERT_THAT(IsTrue(Interactor->HasFocus()));

		Panel.Options.Reset();
		Look(90);
		TestRunner->AddExpectedMessagePlain(TEXT("returned no options"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
		ASSERT_THAT(IsFalse(Panel.StateChanged.IsBound()));
	}

	// --- Focused target state changes ---

	TEST_METHOD(Focus_SubscribesToTheTargetStateDelegate)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		ASSERT_THAT(IsFalse(Focused.StateChanged.IsBound()));

		Interactor->ScorePass();

		ASSERT_THAT(IsTrue(Focused.StateChanged.IsBound()));
	}

	TEST_METHOD(ClearingFocus_UnsubscribesFromTheTarget)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Interactor->ScorePass();

		Look(90);
		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Focused.StateChanged.IsBound()));
	}

	TEST_METHOD(MovingFocus_UnsubscribesFromTheOldTargetOnly)
	{
		ARockTestInteractable& First = Target(FVector(100, 0, 0));
		ARockTestInteractable& Second = Target(FVector(0, 100, 0));
		Interactor->ScorePass();

		Look(90);
		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(First.StateChanged.IsBound()));
		ASSERT_THAT(IsTrue(Second.StateChanged.IsBound()));
	}

	TEST_METHOD(TargetWithoutAStateDelegate_CanStillBeFocused)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Focused.bExposeStateDelegate = false;

		Interactor->ScorePass();

		ASSERT_THAT(IsTrue(Interactor->HasFocus()));
	}

	TEST_METHOD(StateChanged_WithDifferentOptions_BroadcastsNewOptions)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Interactor->ScorePass();
		ASSERT_THAT(AreEqual(1, Listener->OptionsChanged));

		Focused.Options.Add(FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionB));
		Focused.StateChanged.Broadcast();

		ASSERT_THAT(AreEqual(2, Listener->OptionsChanged));
		ASSERT_THAT(AreEqual(2, Listener->LastOptionCount));
		ASSERT_THAT(AreEqual(2, Interactor->GetOptionCount()));
		ASSERT_THAT(AreEqual(1, Listener->FocusChanged));
	}

	TEST_METHOD(StateChanged_WithSameOptions_DoesNotBroadcast)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Interactor->ScorePass();

		Focused.StateChanged.Broadcast();

		ASSERT_THAT(AreEqual(1, Listener->OptionsChanged));
		ASSERT_THAT(AreEqual(2, Focused.GatherOptionsCalls));
	}

	// T-110: this used to assert that focus was kept; options running out now drops focus (see Lifecycle.TargetLifetime too).
	TEST_METHOD(StateChanged_OptionsBecomeEmpty_BroadcastsEmptyOptionsAndClearsFocus)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Interactor->ScorePass();

		Focused.Options.Reset();
		Focused.StateChanged.Broadcast();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
		ASSERT_THAT(AreEqual(0, Interactor->GetOptionCount()));
		ASSERT_THAT(AreEqual(0, Listener->LastOptionCount));
	}

	// --- TriggerInteraction ---

	TEST_METHOD(Trigger_WithoutFocus_DoesNothing)
	{
		Interactor->TriggerInteraction(0);
		ASSERT_THAT(AreEqual(0, Listener->Triggered));
	}

	TEST_METHOD(Trigger_NotifiesTargetAndBroadcastsTheOption)
	{
		ARockTestInteractable& Focused = Target(Ahead(0), {RockInteractionTestTags::OptionA, RockInteractionTestTags::OptionB});
		Focused.Points = {FRockInteractionFixture::MakePoint(Ahead(0), RockInteractionTestTags::PointA)};
		Interactor->ScorePass();

		Interactor->TriggerInteraction(1);

		ASSERT_THAT(AreEqual(1, Focused.BeginCalls));
		ASSERT_THAT(IsTrue(Focused.LastBeginContext.Target.GetObject() == &Focused));
		ASSERT_THAT(IsTrue(Focused.LastBeginContext.Point.PointTag == RockInteractionTestTags::Get(RockInteractionTestTags::PointA)));
		ASSERT_THAT(AreEqual(1, Listener->Triggered));
		ASSERT_THAT(IsTrue(Listener->LastTriggeredOption == RockInteractionTestTags::Get(RockInteractionTestTags::OptionB)));
		ASSERT_THAT(IsTrue(Listener->LastTriggeredPoint == RockInteractionTestTags::Get(RockInteractionTestTags::PointA)));
	}

	TEST_METHOD(Trigger_DefaultsToTheFirstOption)
	{
		Target(Ahead(0), {RockInteractionTestTags::OptionA, RockInteractionTestTags::OptionB});
		Interactor->ScorePass();

		Interactor->TriggerInteraction();

		ASSERT_THAT(IsTrue(Listener->LastTriggeredOption == RockInteractionTestTags::Get(RockInteractionTestTags::OptionA)));
	}

	TEST_METHOD(Trigger_OutOfRangeIndex_DoesNothing)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Interactor->ScorePass();

		Interactor->TriggerInteraction(1);
		Interactor->TriggerInteraction(-1);

		ASSERT_THAT(AreEqual(0, Focused.BeginCalls));
		ASSERT_THAT(AreEqual(0, Listener->Triggered));
	}

	TEST_METHOD(Trigger_AfterFocusIsLost_DoesNothing)
	{
		ARockTestInteractable& Focused = Target(Ahead(0));
		Interactor->ScorePass();
		Look(90);
		Interactor->ScorePass();

		Interactor->TriggerInteraction(0);

		ASSERT_THAT(AreEqual(0, Focused.BeginCalls));
		ASSERT_THAT(AreEqual(0, Listener->Triggered));
	}
};

TEST_CLASS(RockInteractorLifecycleTests, "BRS.RockInteraction.Interactor.Lifecycle")
{
	FRockInteractionFixture Fixture;

	TEST_METHOD(Defaults)
	{
		const URockInteractorComponent* Defaults = GetDefault<URockInteractorComponent>();
		ASSERT_THAT(IsTrue(Defaults->ScanRange == 250.f));
		ASSERT_THAT(IsTrue(Defaults->SphereScanRate == 0.05f));
		ASSERT_THAT(IsTrue(Defaults->LineTraceScanRate == 0.01f));
		ASSERT_THAT(IsTrue(Defaults->LookAtThresholdDegrees == 3.f));
		ASSERT_THAT(IsTrue(Defaults->ScanMode == ERockInteractorScanMode::DirectHitWithSphereOverlap));
		ASSERT_THAT(IsTrue(Defaults->bEnableCandidateEnterEvents));
		ASSERT_THAT(IsTrue(Defaults->bEnableCandidateExitEvents));
	}

	TEST_METHOD(NoFocusYet_QueriesAreEmpty)
	{
		APawn& Pawn = Fixture.SpawnPawn();
		URockTestInteractorComponent& Interactor = Fixture.AddInteractor(Pawn);

		ASSERT_THAT(IsFalse(Interactor.HasFocus()));
		ASSERT_THAT(AreEqual(0, Interactor.GetOptionCount()));
		ASSERT_THAT(IsFalse(Interactor.GetFocusedContext().IsValid()));
		ASSERT_THAT(IsTrue(Interactor.GetFocusedOptions().IsEmpty()));
	}

	TEST_METHOD(BeginPlay_WithoutController_WaitsForOne)
	{
		APawn& Pawn = Fixture.SpawnPawn();
		URockTestInteractorComponent& Interactor = Fixture.AddInteractor(Pawn);

		FRockInteractionFixture::BeginPlay(Pawn);

		ASSERT_THAT(IsTrue(Pawn.ReceiveControllerChangedDelegate.IsBound()));
	}

	TEST_METHOD(GainingAController_StopsListeningForOne)
	{
		APawn& Pawn = Fixture.SpawnPawn();
		URockTestInteractorComponent& Interactor = Fixture.AddInteractor(Pawn);
		FRockInteractionFixture::BeginPlay(Pawn);

		Fixture.Possess(Pawn);

		ASSERT_THAT(IsFalse(Pawn.ReceiveControllerChangedDelegate.IsBound()));
	}

	TEST_METHOD(BeginPlay_WithAController_DoesNotWaitForOne)
	{
		APawn& Pawn = Fixture.SpawnPawn();
		URockTestInteractorComponent& Interactor = Fixture.AddInteractor(Pawn);
		Fixture.Possess(Pawn);

		FRockInteractionFixture::BeginPlay(Pawn);

		ASSERT_THAT(IsFalse(Pawn.ReceiveControllerChangedDelegate.IsBound()));
	}

	TEST_METHOD(BeginPlay_CopiesTheScanRatesToTheTickIntervals)
	{
		APawn& Pawn = Fixture.SpawnPawn();
		URockTestInteractorComponent& Interactor = Fixture.AddInteractor(Pawn);
		Interactor.LineTraceScanRate = 0.5f;

		FRockInteractionFixture::BeginPlay(Pawn);

		ASSERT_THAT(IsTrue(Interactor.PrimaryComponentTick.TickInterval == 0.5f));
	}

	/** The interactor only drives pawns: a plain actor owner never starts scanning. */
	TEST_METHOD(BeginPlay_OnANonPawnOwner_DoesNotStartScanning)
	{
		ARockTestInteractable& Owner = Fixture.SpawnInteractable(FVector::ZeroVector);
		URockTestInteractorComponent* Interactor = NewObject<URockTestInteractorComponent>(&Owner);
		Interactor->RegisterComponent();

		FRockInteractionFixture::BeginPlay(Owner);
		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
	}
};

// A target can disappear under the interactor (a picked-up world item is destroyed). Destroy() marks the actor garbage
// at once, before GC runs, so these tests cover the window where the interactor still holds the pointer.

TEST_CLASS(RockInteractorTargetLifetimeTests, "BRS.RockInteraction.Interactor.Lifecycle.TargetLifetime")
{
	FRockInteractionFixture Fixture;
	APawn* Pawn = nullptr;
	URockTestInteractorComponent* Interactor = nullptr;
	URockTestInteractorListener* Listener = nullptr;

	BEFORE_EACH()
	{
		Interactor = &Fixture.SpawnReadyInteractor(Pawn);
		Listener = Fixture.MakeObject<URockTestInteractorListener>();
		Listener->Bind(*Interactor);
	}

	/** An interactable straight ahead with one option, registered as a persistent candidate. */
	ARockTestInteractable& Target(double Distance = 100.0)
	{
		ARockTestInteractable& Result = Fixture.SpawnInteractable(Ahead(0, Distance));
		Result.Options.Add(FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionA));
		FRockInteractionCandidateEntry Entry;
		Entry.Target = FRockInteractionFixture::AsTarget(Result);
		Entry.OwningActor = &Result;
		Interactor->AddPersistentCandidate(Entry);
		Interactor->Overlap({});
		return Result;
	}

	TEST_METHOD(DestroyedFocusedTarget_ClearsFocusOnTheNextPass)
	{
		ARockTestInteractable& Focused = Target();
		Interactor->ScorePass();
		ASSERT_THAT(IsTrue(Interactor->HasFocus()));

		Focused.Destroy();
		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
		ASSERT_THAT(IsFalse(Interactor->GetFocusedContext().IsValid()));
		ASSERT_THAT(AreEqual(0, Interactor->GetOptionCount()));
		ASSERT_THAT(AreEqual(2, Listener->FocusChanged));
	}

	TEST_METHOD(DestroyedFocusedTarget_IsNotCalledIntoAgain)
	{
		ARockTestInteractable& Focused = Target();
		Interactor->ScorePass();
		const int32 PointsBefore = Focused.GatherPointsCalls;
		const int32 OptionsBefore = Focused.GatherOptionsCalls;

		Focused.Destroy();
		Interactor->ScorePass();
		Interactor->ScorePass();

		ASSERT_THAT(AreEqual(PointsBefore, Focused.GatherPointsCalls));
		ASSERT_THAT(AreEqual(OptionsBefore, Focused.GatherOptionsCalls));
	}

	TEST_METHOD(DestroyedTarget_CannotBeRefocusedBeforeTheNextScan)
	{
		ARockTestInteractable& Item = Target();
		Item.Destroy();

		Interactor->ScorePass();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
		ASSERT_THAT(AreEqual(0, Item.GatherPointsCalls));
		ASSERT_THAT(AreEqual(0, Listener->FocusChanged));
	}

	TEST_METHOD(DestroyedCandidate_IsDroppedFromBothLists)
	{
		ARockTestInteractable& Item = Target();
		ASSERT_THAT(AreEqual(1, Interactor->NumCandidates()));
		ASSERT_THAT(AreEqual(1, Interactor->NumPersistentCandidates()));

		Item.Destroy();
		Interactor->ScorePass();

		ASSERT_THAT(AreEqual(0, Interactor->NumCandidates()));
		ASSERT_THAT(AreEqual(0, Interactor->NumPersistentCandidates()));
	}

	TEST_METHOD(DestroyedCandidate_StillGetsItsExitEventOnce)
	{
		ARockTestInteractable& Item = Target();
		ASSERT_THAT(AreEqual(1, Interactor->Entered.Num()));

		Item.Destroy();
		Interactor->ScorePass();
		Interactor->Overlap({});

		ASSERT_THAT(AreEqual(1, Interactor->Exited.Num()));
	}

	TEST_METHOD(DestroyedPersistentCandidate_IsNotRevivedByTheNextScan)
	{
		ARockTestInteractable& Item = Target();
		Item.Destroy();

		Interactor->Overlap({});

		ASSERT_THAT(AreEqual(0, Interactor->NumCandidates()));
		ASSERT_THAT(AreEqual(0, Interactor->NumPersistentCandidates()));
	}

	TEST_METHOD(DestroyedActorInAnOverlap_IsNotACandidate)
	{
		ARockTestInteractable& Item = Fixture.SpawnInteractable(Ahead(0));
		Item.Destroy();

		Interactor->Overlap({&Item});

		ASSERT_THAT(AreEqual(0, Interactor->NumCandidates()));
	}

	TEST_METHOD(DestroyedTarget_OtherCandidatesStayFocusable)
	{
		ARockTestInteractable& Gone = Target();
		ARockTestInteractable& Kept = Target(200.0);

		Gone.Destroy();
		Interactor->ScorePass();

		ASSERT_THAT(IsTrue(Interactor->HasFocus()));
		ASSERT_THAT(IsTrue(Interactor->GetFocusedContext().Target.GetObject() == &Kept));
		ASSERT_THAT(AreEqual(1, Interactor->NumPersistentCandidates()));
	}

	TEST_METHOD(OptionsBecomeEmpty_ClearsFocusAndBroadcastsIt)
	{
		ARockTestInteractable& Focused = Target();
		Interactor->ScorePass();
		ASSERT_THAT(IsTrue(Interactor->HasFocus()));

		Focused.Options.Reset();
		Focused.StateChanged.Broadcast();

		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
		ASSERT_THAT(IsFalse(Listener->bLastFocusHadTarget));
		ASSERT_THAT(AreEqual(2, Listener->FocusChanged));
	}

	TEST_METHOD(OptionsChangeButStayNonEmpty_KeepsFocus)
	{
		ARockTestInteractable& Focused = Target();
		Interactor->ScorePass();

		Focused.Options.Add(FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionB));
		Focused.StateChanged.Broadcast();

		ASSERT_THAT(IsTrue(Interactor->HasFocus()));
		ASSERT_THAT(AreEqual(2, Interactor->GetOptionCount()));
	}

	TEST_METHOD(TriggerOnADestroyedFocusedTarget_DoesNothingAndClearsFocus)
	{
		ARockTestInteractable& Focused = Target();
		Interactor->ScorePass();

		Focused.Destroy();
		Interactor->TriggerInteraction();

		ASSERT_THAT(AreEqual(0, Focused.BeginCalls));
		ASSERT_THAT(AreEqual(0, Listener->Triggered));
		ASSERT_THAT(IsFalse(Interactor->HasFocus()));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
