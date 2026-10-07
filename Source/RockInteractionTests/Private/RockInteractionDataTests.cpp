// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "RockInteractionTestFixture.h"

#include "GameplayTagsManager.h"

// The plain data types: options container, option and candidate equality, context validity, query pawn lookup, point
// defaults, and the defaults of IRockInteractableTarget.

TEST_CLASS(RockInteractionOptionsTests, "BRS.RockInteraction.Options")
{
	TEST_METHOD(Default_IsEmpty)
	{
		FRockInteractionOptions Options;
		ASSERT_THAT(IsTrue(Options.IsEmpty()));
		ASSERT_THAT(AreEqual(0, Options.AvailableOptions.Num()));
	}

	TEST_METHOD(AddOption_MakesNonEmptyAndKeepsOrder)
	{
		FRockInteractionOptions Options;
		Options.AddOption(FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionA));
		Options.AddOption(FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionB));

		ASSERT_THAT(IsFalse(Options.IsEmpty()));
		ASSERT_THAT(AreEqual(2, Options.AvailableOptions.Num()));
		ASSERT_THAT(IsTrue(Options.AvailableOptions[0].OptionTag == RockInteractionTestTags::Get(RockInteractionTestTags::OptionA)));
		ASSERT_THAT(IsTrue(Options.AvailableOptions[1].OptionTag == RockInteractionTestTags::Get(RockInteractionTestTags::OptionB)));
	}

	TEST_METHOD(Reset_EmptiesTheOptions)
	{
		FRockInteractionOptions Options;
		Options.AddOption(FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionA));

		Options.Reset();

		ASSERT_THAT(IsTrue(Options.IsEmpty()));
	}

	/** Options are identified by tag only, which is how the interactor decides whether a re-gather changed anything. */
	TEST_METHOD(OptionEquality_ComparesTagOnly)
	{
		FRockInteractionOption A = FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionA);
		FRockInteractionOption AWithText = FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionA);
		AWithText.Text = FText::FromString(TEXT("Different"));
		AWithText.SubText = FText::FromString(TEXT("Hint"));
		const FRockInteractionOption B = FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionB);

		ASSERT_THAT(IsTrue(A == AWithText));
		ASSERT_THAT(IsFalse(A == B));
	}

	TEST_METHOD(OptionArrays_CompareElementwiseByTag)
	{
		TArray<FRockInteractionOption> First = {FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionA), FRockInteractionFixture::MakeOption(RockInteractionTestTags::OptionB)};
		TArray<FRockInteractionOption> Same = First;
		TArray<FRockInteractionOption> Reordered = {First[1], First[0]};
		TArray<FRockInteractionOption> Shorter = {First[0]};

		ASSERT_THAT(IsTrue(First == Same));
		ASSERT_THAT(IsFalse(First == Reordered));
		ASSERT_THAT(IsFalse(First == Shorter));
	}
};

TEST_CLASS(RockInteractionDataTests, "BRS.RockInteraction.Data")
{
	FRockInteractionFixture Fixture;

	TEST_METHOD(Point_Defaults)
	{
		const FRockInteractionPoint Point;
		ASSERT_THAT(IsTrue(Point.WorldLocation.IsZero()));
		ASSERT_THAT(IsFalse(Point.PointTag.IsValid()));
		ASSERT_THAT(IsFalse(Point.SourceComponent.IsValid()));
		ASSERT_THAT(IsTrue(Point.SocketName == NAME_None));
		ASSERT_THAT(IsTrue(Point.LookAtThresholdScale == 1.f));
		ASSERT_THAT(IsTrue(Point.Role == ERockInteractionPointRole::Interaction));
	}

	TEST_METHOD(Context_DefaultIsNotValid)
	{
		const FRockInteractionContext Context;
		ASSERT_THAT(IsFalse(Context.IsValid()));
	}

	TEST_METHOD(Context_WithTargetIsValid)
	{
		ARockTestInteractable& Target = Fixture.SpawnInteractable(FVector::ZeroVector);
		FRockInteractionContext Context;
		Context.Target = FRockInteractionFixture::AsTarget(Target);

		ASSERT_THAT(IsTrue(Context.IsValid()));
	}

	TEST_METHOD(Query_DefaultHasNoInstigatorPawn)
	{
		const FRockInteractionQuery Query;
		ASSERT_THAT(IsNull(Query.GetInstigatorPawn()));
	}

	TEST_METHOD(Query_PawnInstigator_IsReturned)
	{
		APawn& Pawn = Fixture.SpawnPawn();
		FRockInteractionQuery Query;
		Query.Instigator = &Pawn;

		ASSERT_THAT(IsTrue(Query.GetInstigatorPawn() == &Pawn));
	}

	/** The instigator is typed as AActor so automation systems can use it, but those have no pawn. */
	TEST_METHOD(Query_NonPawnInstigator_HasNoPawn)
	{
		ARockTestInteractable& Actor = Fixture.SpawnInteractable(FVector::ZeroVector);
		FRockInteractionQuery Query;
		Query.Instigator = &Actor;

		ASSERT_THAT(IsNull(Query.GetInstigatorPawn()));
	}

	TEST_METHOD(CandidateEntry_EqualityIsTheTargetObject)
	{
		ARockTestInteractable& First = Fixture.SpawnInteractable(FVector::ZeroVector);
		ARockTestInteractable& Second = Fixture.SpawnInteractable(FVector::ZeroVector);

		FRockInteractionCandidateEntry A;
		A.Target = FRockInteractionFixture::AsTarget(First);
		A.OwningActor = &First;
		FRockInteractionCandidateEntry SameTargetOtherOwner = A;
		SameTargetOtherOwner.OwningActor = &Second;
		FRockInteractionCandidateEntry B;
		B.Target = FRockInteractionFixture::AsTarget(Second);
		B.OwningActor = &Second;

		ASSERT_THAT(IsTrue(A == SameTargetOtherOwner));
		ASSERT_THAT(IsFalse(A == B));
	}

	TEST_METHOD(ActivateTag_IsRegistered)
	{
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Verb.Activate"), false);
		ASSERT_THAT(IsTrue(Tag.IsValid()));
	}
};

TEST_CLASS(RockInteractionTargetDefaultsTests, "BRS.RockInteraction.TargetDefaults")
{
	FRockInteractionFixture Fixture;

	TEST_METHOD(RequiresDirectHit_DefaultsToFalse)
	{
		ARockTestMinimalInteractable& Target = Fixture.Spawner.SpawnActor<ARockTestMinimalInteractable>();
		ASSERT_THAT(IsFalse(Target.RequiresDirectHit()));
	}

	TEST_METHOD(StateChangedDelegate_DefaultsToNull)
	{
		ARockTestMinimalInteractable& Target = Fixture.Spawner.SpawnActor<ARockTestMinimalInteractable>();
		ASSERT_THAT(IsNull(Target.GetInteractionStateChangedDelegate()));
	}

	TEST_METHOD(DisplayName_HasADefault)
	{
		ARockTestMinimalInteractable& Target = Fixture.Spawner.SpawnActor<ARockTestMinimalInteractable>();
		ASSERT_THAT(AreEqual(FString(TEXT("InteractableActor")), Target.GetInteractableDisplayName().ToString()));
	}

	TEST_METHOD(OnInteractionBegin_DefaultIsANoOp)
	{
		ARockTestMinimalInteractable& Target = Fixture.Spawner.SpawnActor<ARockTestMinimalInteractable>();
		Target.OnInteractionBegin(FRockInteractionContext());
		ASSERT_THAT(IsTrue(true));
	}

	TEST_METHOD(Actor_ImplementsTheInterface)
	{
		ARockTestMinimalInteractable& Target = Fixture.Spawner.SpawnActor<ARockTestMinimalInteractable>();
		ASSERT_THAT(IsTrue(Target.Implements<URockInteractableTarget>()));
		ASSERT_THAT(IsNotNull(Cast<IRockInteractableTarget>(&Target)));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
