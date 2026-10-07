// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "RockInteractableTarget.h"
#include "RockInteractionContext.h"
#include "RockInteractionOption.h"
#include "RockInteractorComponent.h"

#include "RockInteractionTestTypes.generated.h"

/** Test-only tags, registered by the module at startup. */
namespace RockInteractionTestTags
{
	extern const FName PointA;
	extern const FName PointB;
	extern const FName OptionA;
	extern const FName OptionB;

	FGameplayTag Get(const FName& Name);
}

/** Implements only the pure virtuals, to cover the interface defaults. */
UCLASS()
class ARockTestMinimalInteractable : public AActor, public IRockInteractableTarget
{
	GENERATED_BODY()

public:
	virtual bool GatherInteractionPoints(const FRockInteractionQuery& Context, TArray<FRockInteractionPoint>& OutPoints) const override { return true; }
	virtual void GatherInteractionOptions(const FRockInteractionContext& Context, FRockInteractionOptions& InteractionOptions) override {}
	virtual void GatherInteractionAbilities(TArray<TSubclassOf<UGameplayAbility>>& OutAbilities) const override {}
};

/** A configurable interactable that records how it was called. */
UCLASS()
class ARockTestInteractable : public AActor, public IRockInteractableTarget
{
	GENERATED_BODY()

public:
	ARockTestInteractable()
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	}

	// Configuration
	TArray<FRockInteractionPoint> Points;
	TArray<FRockInteractionOption> Options;
	bool bInteractable = true;
	bool bDirectHitOnly = false;
	bool bExposeStateDelegate = true;
	FSimpleMulticastDelegate StateChanged;

	// Observations
	mutable int32 GatherPointsCalls = 0;
	mutable FRockInteractionQuery LastPointsQuery;
	int32 GatherOptionsCalls = 0;
	FRockInteractionContext LastOptionsContext;
	int32 BeginCalls = 0;
	FRockInteractionContext LastBeginContext;

	virtual bool GatherInteractionPoints(const FRockInteractionQuery& Context, TArray<FRockInteractionPoint>& OutPoints) const override
	{
		++GatherPointsCalls;
		LastPointsQuery = Context;
		OutPoints.Append(Points);
		return bInteractable;
	}

	virtual void GatherInteractionOptions(const FRockInteractionContext& Context, FRockInteractionOptions& InteractionOptions) override
	{
		++GatherOptionsCalls;
		LastOptionsContext = Context;
		for (const FRockInteractionOption& Option : Options)
		{
			InteractionOptions.AddOption(Option);
		}
	}

	virtual void GatherInteractionAbilities(TArray<TSubclassOf<UGameplayAbility>>& OutAbilities) const override {}

	virtual bool RequiresDirectHit() const override { return bDirectHitOnly; }

	virtual void OnInteractionBegin(const FRockInteractionContext& Context) override
	{
		++BeginCalls;
		LastBeginContext = Context;
	}

	virtual FSimpleMulticastDelegate* GetInteractionStateChangedDelegate() override { return bExposeStateDelegate ? &StateChanged : nullptr; }
};

/** The interactor component with its protected scan steps made public and its hooks recorded. */
UCLASS()
class URockTestInteractorComponent : public URockInteractorComponent
{
	GENERATED_BODY()

public:
	TArray<TWeakObjectPtr<UObject>> Entered;
	TArray<TWeakObjectPtr<UObject>> Exited;
	int32 CandidatesUpdatedCalls = 0;

	/** Feeds a sphere scan result containing these actors into UpdateCandidates. */
	void Overlap(const TArray<AActor*>& Actors)
	{
		TArray<FOverlapResult> Overlaps;
		for (AActor* Actor : Actors)
		{
			FOverlapResult& Result = Overlaps.AddDefaulted_GetRef();
			Result.OverlapObjectHandle = FActorInstanceHandle(Actor);
		}
		UpdateCandidates(Overlaps);
	}

	int32 NumCandidates() const { return Candidates.Num(); }
	int32 NumPersistentCandidates() const { return PersistentCandidates.Num(); }

	/** One scoring pass, as the primary tick does. */
	void ScorePass() { TickLineTrace(); }

	bool Resolve(const TScriptInterface<IRockInteractableTarget>& Candidate, const FInteractionScanContext& Scan, TScriptInterface<IRockInteractableTarget>& OutTarget, FRockInteractionPoint& OutPoint) const
	{
		return ResolvePointsFromTarget(Candidate, Scan, BuildQueryForTest(), OutTarget, OutPoint);
	}

	bool DirectHit(const FInteractionScanContext& Scan, TScriptInterface<IRockInteractableTarget>& OutTarget, FRockInteractionPoint& OutPoint) const
	{
		return TryResolveDirectHit(Scan, BuildQueryForTest(), OutTarget, OutPoint);
	}

	bool LookAt(const FInteractionScanContext& Scan, TScriptInterface<IRockInteractableTarget>& OutTarget, FRockInteractionPoint& OutPoint) const
	{
		return ScoreCandidatesByLookAt(Scan, BuildQueryForTest(), OutTarget, OutPoint);
	}

	void ResolveProxy(TScriptInterface<IRockInteractableTarget>& Target, FRockInteractionPoint& InOutPoint) const
	{
		ResolveVisibilityProxy(BuildQueryForTest(), Target, InOutPoint);
	}

	FRockInteractionQuery BuildQueryForTest() const
	{
		FRockInteractionQuery Query;
		Query.Instigator = GetOwner();
		Query.InteractionTags = QueryInteractionTags;
		return Query;
	}

	FRockInteractionQuery CallBuildQuery() { return BuildQuery(); }

	// --- Hints ---
	// The test world has no physics, so the visibility trace is stubbed: points near a blocked location are not visible.
	TArray<FVector> BlockedLocations;
	mutable int32 VisibilityTraceCalls = 0;

	void RefreshHints() { RefreshHintList(); }
	/** One tick of hint work, as the primary tick runs it after scoring. */
	void HintPass() { TickHints(); }
	void TraceHints() { TraceHintVisibility(); }

	virtual bool IsHintPointVisible(const FVector& ViewOrigin, const FRockInteractionHintPoint& Hint) const override
	{
		++VisibilityTraceCalls;
		for (const FVector& Blocked : BlockedLocations)
		{
			if (FVector::DistSquared(Blocked, Hint.Point.WorldLocation) < 1.0) { return false; }
		}
		return true;
	}

protected:
	virtual void OnCandidateEntered(const TScriptInterface<IRockInteractableTarget>& Target) override { Entered.Add(Target.GetObject()); }
	virtual void OnCandidateExited(const TScriptInterface<IRockInteractableTarget>& Target) override { Exited.Add(Target.GetObject()); }
	virtual void OnCandidatesUpdated(const TArray<FRockInteractionCandidateEntry>& NewCandidates) override { ++CandidatesUpdatedCalls; }
};

/** Records the interactor's delegates. */
UCLASS()
class URockTestInteractorListener : public UObject
{
	GENERATED_BODY()

public:
	int32 FocusChanged = 0;
	int32 OptionsChanged = 0;
	int32 Triggered = 0;
	bool bLastFocusHadTarget = false;
	int32 LastOptionCount = 0;
	FGameplayTag LastTriggeredOption;
	FGameplayTag LastTriggeredPoint;

	UFUNCTION()
	void HandleFocusChanged(const FRockInteractionContext& Context)
	{
		++FocusChanged;
		bLastFocusHadTarget = Context.IsValid();
	}

	UFUNCTION()
	void HandleOptionsChanged(const FRockInteractionOptions& Options)
	{
		++OptionsChanged;
		LastOptionCount = Options.AvailableOptions.Num();
	}

	UFUNCTION()
	void HandleTriggered(const FRockInteractionContext& Context, const FRockInteractionOption& Option)
	{
		++Triggered;
		LastTriggeredOption = Option.OptionTag;
		LastTriggeredPoint = Context.Point.PointTag;
	}

	void Bind(URockInteractorComponent& Component)
	{
		Component.OnFocusChanged.AddDynamic(this, &URockTestInteractorListener::HandleFocusChanged);
		Component.OnOptionsChanged.AddDynamic(this, &URockTestInteractorListener::HandleOptionsChanged);
		Component.OnInteractionTriggered.AddDynamic(this, &URockTestInteractorListener::HandleTriggered);
	}
};
