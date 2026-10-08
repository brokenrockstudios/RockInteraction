// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockInteractorComponent.h"

#include "RockInteractableTarget.h"
#include "Engine/OverlapResult.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

static TAutoConsoleVariable<bool> CVarShowInteractionPoints(
	TEXT("Rock.Interaction.ShowDebugPoints"),
	false,
	TEXT("Draw interaction point spheres and LookAt scores in world"),
	ECVF_Cheat);

DECLARE_STATS_GROUP(TEXT("RockInteraction"), STATGROUP_RockInteraction, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("RockInteraction_ScoreAndSelect"), STAT_RockInteraction_ScoreAndSelect, STATGROUP_RockInteraction);
DECLARE_CYCLE_STAT(TEXT("RockInteraction_UpdateCandidates"), STAT_RockInteraction_UpdateCandidates, STATGROUP_RockInteraction);
DECLARE_CYCLE_STAT(TEXT("RockInteraction_LineTrace"), STAT_RockInteraction_LineTrace, STATGROUP_RockInteraction);
DECLARE_CYCLE_STAT(TEXT("RockInteraction_GatherPoints"), STAT_RockInteraction_GatherPoints, STATGROUP_RockInteraction);
DECLARE_CYCLE_STAT(TEXT("RockInteraction_Hints"), STAT_RockInteraction_Hints, STATGROUP_RockInteraction);
DECLARE_CYCLE_STAT(TEXT("RockInteraction_HintTrace"), STAT_RockInteraction_HintTrace, STATGROUP_RockInteraction);

void FRockInteractorSecondaryTick::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (Target && !Target->IsUnreachable())
	{
		Target->SecondaryTickComponent(DeltaTime, TickType);
	}
}

// Sets default values for this component's properties
URockInteractorComponent::URockInteractorComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.SetTickFunctionEnable(false);

	SecondaryTickFunction.bCanEverTick = true;
	SecondaryTickFunction.TickGroup = TG_PrePhysics;
	SecondaryTickFunction.SetTickFunctionEnable(false);
}

// ----------------------------------------------------------------
// Lifecycle

void URockInteractorComponent::BeginPlay()
{
	Super::BeginPlay();
	PrimaryComponentTick.TickInterval = LineTraceScanRate;
	SecondaryTickFunction.TickInterval = SphereScanRate;

	SecondaryTickFunction.Target = this;
	SecondaryTickFunction.RegisterTickFunction(GetComponentLevel());

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;
	if (Pawn->GetController())
	{
		StartScans();
	}
	else
	{
		// Sometimes our pawn gets spawned before it has a controller, so let's just wait until we have one.
		Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &ThisClass::OnControllerChanged);
	}

	ScanRangeSquared = ScanRange * ScanRange;
}

void URockInteractorComponent::OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	StartScans();
	Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::OnControllerChanged);
}

void URockInteractorComponent::StartScans()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) { return; }

	bLineTraceScanActive = Pawn->IsLocallyControlled();
	if (bLineTraceScanActive)
	{
		SetComponentTickEnabled(true);
	}

	const bool bLocalOrAuthority = Pawn->IsLocallyControlled() || Pawn->HasAuthority();
	bSphereScanActive = bLocalOrAuthority && ScanMode == ERockInteractorScanMode::DirectHitWithSphereOverlap;
	if (bSphereScanActive)
	{
		SecondaryTickFunction.SetTickFunctionEnable(true);
		SphereScanDelegate.BindUObject(this, &URockInteractorComponent::OnScanComplete);
	}
}

void URockInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bLineTraceScanActive)
	{
		TickLineTrace();
		TickHints();
	}

#if ENABLE_DRAW_DEBUG
	const bool bShowPoints = CVarShowInteractionPoints.GetValueOnGameThread();
	if (bShowPoints)
	{
		const float LifeTime = PrimaryComponentTick.TickInterval;
		UWorld* World = GetWorld();
		DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation(), ScanRange, 12, Candidates.Num() > 0 ? FColor::Green : FColor::Cyan, false, LifeTime);
		if (bHasFocus)
		{
			DrawDebugSphere(World, CurrentContext.Point.WorldLocation, 12.f, 8, FColor::White, false, LifeTime, 1, .25);
		}
	}
#endif
}

void URockInteractorComponent::SecondaryTickComponent(float DeltaTime, ELevelTick TickType)
{
	if (bSphereScanActive)
	{
		TickSphereScan();
	}
}

void URockInteractorComponent::PruneInvalidCandidates()
{
	PersistentCandidates.RemoveAll([](const FRockInteractionCandidateEntry& Entry) { return !Entry.IsValid(); });

	// A destroyed target still gets its exit event (the game side keys grants by object), as the scan diff did before.
	// An entry GC already nulled has no object left to report.
	for (int32 Index = Candidates.Num() - 1; Index >= 0; --Index)
	{
		if (Candidates[Index].IsValid()) { continue; }
		const FRockInteractionCandidateEntry Gone = Candidates[Index];
		Candidates.RemoveAt(Index, EAllowShrinking::No);
		if (bEnableCandidateExitEvents && Gone.Target.GetObject())
		{
			OnCandidateExited(Gone.Target);
		}
	}

	if (bHasFocus && !::IsValid(CurrentContext.Target.GetObject()))
	{
		ClearFocus();
	}
}

void URockInteractorComponent::AddPersistentCandidate(const FRockInteractionCandidateEntry& CandidateEntry)
{
	if (CandidateEntry.Target)
	{
		PersistentCandidates.AddUnique(CandidateEntry);
	}
}

void URockInteractorComponent::RemovePersistentCandidate(const FRockInteractionCandidateEntry& CandidateEntry)
{
	PersistentCandidates.RemoveSwap(CandidateEntry);
}

void URockInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SphereScanDelegate.Unbind();
	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------
// Sphere overlap at ScanRate (Default 20hz)
void URockInteractorComponent::TickSphereScan()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) { return; }

	// If a scan is still in flight, you can either skip or force-fetch it here
	if (World->IsTraceHandleValid(PendingSphereScanHandle, true)) { return; }

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RockInteractionInstigatorScan), /* bTraceComplex= */ false);
	Params.AddIgnoredActor(Owner);

	PendingSphereScanHandle = World->AsyncOverlapByChannel(
		Owner->GetActorLocation(),
		FQuat::Identity,
		ScanChannel,
		FCollisionShape::MakeSphere(ScanRange),
		Params,
		FCollisionResponseParams::DefaultResponseParam,
		&SphereScanDelegate); // delegate fires when done
}

void URockInteractorComponent::OnScanComplete(const FTraceHandle& Handle, FOverlapDatum& Datum)
{
	// stale result, discard
	if (Handle != PendingSphereScanHandle) { return; }

	UpdateCandidates(Datum.OutOverlaps);
}

static FRockInteractionCandidateEntry WrapAsTarget(AActor* OwningActor, UObject* TargetObject)
{
	FRockInteractionCandidateEntry CandidateEntry;
	CandidateEntry.OwningActor = OwningActor;
	CandidateEntry.Target.SetObject(TargetObject);
	CandidateEntry.Target.SetInterface(Cast<IRockInteractableTarget>(TargetObject));
	return CandidateEntry;
}

// ----------------------------------------------------------------

void URockInteractorComponent::UpdateCandidates(TArray<FOverlapResult>& Overlaps)
{
	SCOPE_CYCLE_COUNTER(STAT_RockInteraction_UpdateCandidates);

	PruneInvalidCandidates();

	TArray<FRockInteractionCandidateEntry> NewCandidates;

	// Inject persistent candidates. Always relevant regardless of overlap
	for (const auto& Persistent : PersistentCandidates)
	{
		NewCandidates.AddUnique(Persistent);
	}

	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Actor = Result.GetActor();
		if (!IsValid(Actor)) { continue; }

		// Check the actor itself first, then its components
		if (Actor->Implements<URockInteractableTarget>())
		{
			NewCandidates.AddUnique(WrapAsTarget(Actor, Actor));
		}
	}

	// Diff: exits first, then enters
	if (bEnableCandidateExitEvents)
	{
		for (const auto& Prev : Candidates)
		{
			if (!NewCandidates.Contains(Prev))
			{
				OnCandidateExited(Prev.Target);
			}
		}
	}
	if (bEnableCandidateEnterEvents)
	{
		for (const auto& Next : NewCandidates)
		{
			if (!Candidates.Contains(Next))
			{
				OnCandidateEntered(Next.Target);
			}
		}
	}

	Candidates = MoveTemp(NewCandidates);

	// The trade-off probably is around N=30 ish, but an occasion boop above is fine, but if we are hitting 50, its time to seriously consider.
	UE_CLOG(Candidates.Num() > 48, LogRockInteraction, Warning, TEXT("Candidates exceed 48, consider TSet instead of TArray: %d"), Candidates.Num());

	// If candidates emptied, clear focus immediately rather than waiting for next score pass
	if (Candidates.IsEmpty())
	{
		ClearFocus();
	}

	OnCandidatesUpdated(Candidates);
}

void URockInteractorComponent::OnCandidateEntered(const TScriptInterface<IRockInteractableTarget>& Target)
{
	// Do game specific things, such as adding GAS abilities based upon candidates.
}

void URockInteractorComponent::OnCandidateExited(const TScriptInterface<IRockInteractableTarget>& Target)
{
	// Do game specific things, such as adding GAS abilities based upon candidates.
}

void URockInteractorComponent::OnCandidatesUpdated(const TArray<FRockInteractionCandidateEntry>& NewCandidates)
{
	// Do game specific things, such as adding GAS abilities based upon candidates.
}


// ----------------------------------------------------------------
// Per-frame scoring

// ScoreAndSelectFocused
void URockInteractorComponent::TickLineTrace()
{
	SCOPE_CYCLE_COUNTER(STAT_RockInteraction_ScoreAndSelect);

	// A target destroyed since the last pass (a picked-up world item) must not be scored or stay focused.
	PruneInvalidCandidates();

	FInteractionScanContext ScanCtx;
	if (!GetViewPoint(ScanCtx.ViewOrigin, ScanCtx.ViewDirection))
	{
		ClearFocus();
		return;
	}

	ScanCtx.LookAtThresholdCos = FMath::Cos(FMath::DegreesToRadians(LookAtThresholdDegrees));

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RockInteractionInstigatorTrace), false);
	TraceParams.AddIgnoredActor(Owner);
	// Trace out to max range regardless of candidate distance; we just want to know what the player is looking at, and if it's one of our candidates then that wins outright
	// Because the camera could be 'further back,' we need to make sure the ScanRange is at least far enough to make it past the sphere overlap or something quite far. 
	// If we've done a registered PersistentActor, we might want this to be something like 20m? 
	{
		SCOPE_CYCLE_COUNTER(STAT_RockInteraction_LineTrace);
		const float TraceLength = ScanRange + FVector::Dist(ScanCtx.ViewOrigin, Owner->GetActorLocation());
		const FVector TraceEnd = ScanCtx.ViewOrigin + ScanCtx.ViewDirection * TraceLength;
		World->LineTraceSingleByChannel(ScanCtx.HitResult, ScanCtx.ViewOrigin, TraceEnd, ScanChannel, TraceParams);
		ScanCtx.HitActor = ScanCtx.HitResult.GetActor();
		ScanCtx.HitComp = ScanCtx.HitResult.GetComponent();
	}

	// --- Build query ---
	FRockInteractionQuery Query = BuildQuery();

	// --- Score ---
	TScriptInterface<IRockInteractableTarget> BestTarget = nullptr;
	FRockInteractionPoint BestPoint;

	// Try Direct Hit, otherwise fallback to LookAt
	TryResolveDirectHit(ScanCtx, Query, BestTarget, BestPoint);
	if (!BestTarget && ScanMode == ERockInteractorScanMode::DirectHitWithSphereOverlap)
	{
		ScoreCandidatesByLookAt(ScanCtx, Query, BestTarget, BestPoint);
	}

	if (!BestTarget)
	{
		ClearFocus();
		return;
	}

	// --- Resolve IV_ proxy to its owning IX_ point ---
	ResolveVisibilityProxy(Query, BestTarget, BestPoint);

	// --- Dirty check & broadcast ---
	const bool bTargetChanged = (CurrentContext.Target != BestTarget);
	// Track point identity, not its animated world location.
	const bool bPointChanged = CurrentContext.Point.PointTag != BestPoint.PointTag
		|| CurrentContext.Point.SourceComponent != BestPoint.SourceComponent
		|| CurrentContext.Point.SocketName != BestPoint.SocketName
		|| CurrentContext.Point.Role != BestPoint.Role;

	if (bTargetChanged || bPointChanged)
	{
		// Gather against the winning target before committing focus.
		FRockInteractionContext NewContext = CurrentContext;
		NewContext.Target = BestTarget;
		NewContext.Point = BestPoint;
		NewContext.Query = Query;
		NewContext.TraceHitResult = ScanCtx.HitResult;
		FRockInteractionOptions NewOptions;
		BestTarget->GatherInteractionOptions(NewContext, NewOptions);

		if (NewOptions.IsEmpty())
		{
			UE_LOG(
				LogRockInteraction, Warning,
				TEXT("[RockInteractor] %s scored as best candidate but returned no options. Point should gate availability upstream. Skipping focus."),
				*GetNameSafe(BestTarget.GetObject()));
			// Drops focus from whatever was focused before. A no-op (and no broadcast) if nothing was.
			ClearFocus();
			return;
		}

		// Commit, we know we have valid options
		if (bTargetChanged)
		{
			SetFocusedTarget(BestTarget);
		}
		CurrentContext.Point = BestPoint;
		CurrentContext.Query = Query;
		CurrentContext.TraceHitResult = ScanCtx.HitResult;
		bHasFocus = true;
		CurrentOptions = MoveTemp(NewOptions);
		OnFocusChanged.Broadcast(CurrentContext);
		OnOptionsChanged.Broadcast(CurrentOptions);
	}
	else
	{
		CurrentContext.Point = BestPoint;
		CurrentContext.Query = Query;
		CurrentContext.TraceHitResult = ScanCtx.HitResult;
	}
}

bool URockInteractorComponent::TryResolveDirectHit(
	const FInteractionScanContext& ScanCtx,
	const FRockInteractionQuery& Query,
	TScriptInterface<IRockInteractableTarget>& OutTarget,
	FRockInteractionPoint& OutPoint) const
{
	if (!ScanCtx.HitActor) { return false; }
	const bool bActorImplements = ScanCtx.HitActor->Implements<URockInteractableTarget>();
	if (!bActorImplements) { return false; }

	// When sphere scan is off, no candidates list. Wrap hit target directly
	if (ScanMode == ERockInteractorScanMode::DirectHitOnly)
	{
		UObject* HitObject = ScanCtx.HitActor;
		TScriptInterface<IRockInteractableTarget> HitTarget;
		HitTarget.SetObject(HitObject);
		HitTarget.SetInterface(Cast<IRockInteractableTarget>(HitObject));
		return ResolvePointsFromTarget(HitTarget, ScanCtx, Query, OutTarget, OutPoint);
	}

	for (const auto& Candidate : Candidates)
	{
		if (!Candidate.IsValid()) { continue; }
		if (Candidate.OwningActor != ScanCtx.HitActor) { continue; }
		return ResolvePointsFromTarget(Candidate.Target, ScanCtx, Query, OutTarget, OutPoint);
	}

	// HitTarget not directly resolvable
	return false;
}

bool URockInteractorComponent::ResolvePointsFromTarget(
	const TScriptInterface<IRockInteractableTarget>& Candidate,
	const FInteractionScanContext& ScanCtx,
	const FRockInteractionQuery& Query,
	TScriptInterface<IRockInteractableTarget>& OutTarget,
	FRockInteractionPoint& OutPoint) const
{
	TArray<FRockInteractionPoint> Points;
	{
		SCOPE_CYCLE_COUNTER(STAT_RockInteraction_GatherPoints);
		if (!Candidate->GatherInteractionPoints(Query, Points))
		{
			return false; // Hit but non-interactable, no fallback to LookAt
		}
	}

	int32 IXPointCount = 0;
	const FRockInteractionPoint* FirstIXPoint = nullptr;
	const FRockInteractionPoint* ComponentMatchPoint = nullptr;
	int32 ComponentMatchCount = 0;

	for (const FRockInteractionPoint& Point : Points)
	{
		if (Point.Role != ERockInteractionPointRole::Interaction) { continue; }
		++IXPointCount;
		if (!FirstIXPoint) { FirstIXPoint = &Point; }
		if (ScanCtx.HitComp && Point.SourceComponent.Get() == ScanCtx.HitComp)
		{
			++ComponentMatchCount;
			ComponentMatchPoint = &Point;
		}
	}

	const bool bUniqueComponentMatch = (ComponentMatchCount == 1);
	const FRockInteractionPoint* Resolved = bUniqueComponentMatch ? ComponentMatchPoint
		: (IXPointCount <= 1) ? FirstIXPoint
		: nullptr; // ambiguous: 2+ IX on same component

	if (Resolved || IXPointCount == 0)
	{
		OutTarget = Candidate;
		OutPoint = Resolved ? *Resolved : FRockInteractionPoint();
		return true;
	}

	// Ambiguous: fall through to LookAt
	return false;
}

bool URockInteractorComponent::ScoreCandidatesByLookAt(
	const FInteractionScanContext& ScanCtx,
	const FRockInteractionQuery& Query,
	TScriptInterface<IRockInteractableTarget>& OutTarget,
	FRockInteractionPoint& OutPoint) const
{
	// Every point inside its look-at cone, with its score.
	struct FScored
	{
		TScriptInterface<IRockInteractableTarget> Target;
		FRockInteractionPoint Point;
		float Dot = 0.f;
	};
	TArray<FScored> Scored;
	TArray<FRockInteractionPoint> Points;

	for (const auto& CandidateEntry : Candidates)
	{
		if (!CandidateEntry.IsValid()) { continue; }
		if (CandidateEntry.Target->RequiresDirectHit()) { continue; }
		Points.Reset();
		{
			SCOPE_CYCLE_COUNTER(STAT_RockInteraction_GatherPoints);
			if (!CandidateEntry.Target->GatherInteractionPoints(Query, Points)) { continue; }
		}


		if (Points.IsEmpty())
		{
			if (!IsValid(CandidateEntry.OwningActor)) { continue; }
			const FVector ToActor = (CandidateEntry.OwningActor->GetActorLocation() - ScanCtx.ViewOrigin).GetSafeNormal();
			const float Dot = FVector::DotProduct(ScanCtx.ViewDirection, ToActor);
			if (Dot > ScanCtx.LookAtThresholdCos)
			{
				FScored& Entry = Scored.AddDefaulted_GetRef();
				Entry.Target = CandidateEntry.Target;
				Entry.Dot = Dot;
				Entry.Point.WorldLocation = CandidateEntry.OwningActor->GetActorLocation();
				Entry.Point.SourceComponent = CandidateEntry.OwningActor->GetRootComponent();
				Entry.Point.Role = ERockInteractionPointRole::Interaction;
			}
		}
		else
		{
			FVector ActorLocation = GetOwner()->GetActorLocation();
			for (const FRockInteractionPoint& Point : Points)
			{
				if (FVector::DistSquared(ActorLocation, Point.WorldLocation) > ScanRangeSquared)
				{
					continue;
				}
				const FVector ToPoint = (Point.WorldLocation - ScanCtx.ViewOrigin).GetSafeNormal();
				const float Dot = FVector::DotProduct(ScanCtx.ViewDirection, ToPoint);
				DrawInteractionPointDebug(GetWorld(), Point.WorldLocation, Dot);
				const float EffectiveThresholdCos = Point.LookAtThresholdScale == 1.f
					? ScanCtx.LookAtThresholdCos
					: FMath::Cos(FMath::DegreesToRadians(LookAtThresholdDegrees * Point.LookAtThresholdScale));
				if (Dot > EffectiveThresholdCos)
				{
					FScored& Entry = Scored.AddDefaulted_GetRef();
					Entry.Target = CandidateEntry.Target;
					Entry.Point = Point;
					Entry.Dot = Dot;
				}
			}
		}
	}

	// Best score first (the first of an exact tie wins). With line of sight on, a blocked winner gives way to the next best,
	// up to FocusLineOfSightTraces traces; the pass finds nothing if all of those are blocked.
	const int32 MaxAttempts = bFocusRequiresLineOfSight ? FMath::Max(FocusLineOfSightTraces, 1) : 1;
	for (int32 Attempt = 0; Attempt < MaxAttempts && !Scored.IsEmpty(); ++Attempt)
	{
		int32 BestIndex = 0;
		for (int32 Index = 1; Index < Scored.Num(); ++Index)
		{
			if (Scored[Index].Dot > Scored[BestIndex].Dot) { BestIndex = Index; }
		}

		if (bFocusRequiresLineOfSight)
		{
			FRockInteractionHintPoint Probe;
			Probe.Point = Scored[BestIndex].Point;
			if (!IsHintPointVisible(ScanCtx.ViewOrigin, Probe))
			{
				Scored.RemoveAt(BestIndex, EAllowShrinking::No);
				continue;
			}
		}

		OutTarget = Scored[BestIndex].Target;
		OutPoint = Scored[BestIndex].Point;
		return true;
	}

	return false;
}

void URockInteractorComponent::ResolveVisibilityProxy(
	const FRockInteractionQuery& Query,
	TScriptInterface<IRockInteractableTarget>& Target, FRockInteractionPoint& InOutPoint) const
{
	if (InOutPoint.Role == ERockInteractionPointRole::Visibility)
	{
		TArray<FRockInteractionPoint> OutPoints;
		{
			SCOPE_CYCLE_COUNTER(STAT_RockInteraction_GatherPoints);
			Target->GatherInteractionPoints(Query, OutPoints);
		}
		const FRockInteractionPoint* OwnerIX = OutPoints.FindByPredicate(
			[&](const FRockInteractionPoint& P)
			{
				return P.Role == ERockInteractionPointRole::Interaction && P.PointTag == InOutPoint.PointTag;
			});
		if (OwnerIX)
		{
			InOutPoint = *OwnerIX;
		}
	}
}

// ----------------------------------------------------------------
// Hints

void URockInteractorComponent::TickHints()
{
	if (!bEnableHints)
	{
		HintPoints.Reset();
		return;
	}

	const UWorld* World = GetWorld();
	if (!World) { return; }

	const double Now = World->GetTimeSeconds();
	if (Now >= NextHintRefreshTime)
	{
		NextHintRefreshTime = Now + HintRefreshRate;
		RefreshHintList();
	}

	UpdateHintFocusFlags();
	if (bTraceHintVisibility)
	{
		TraceHintVisibility();
	}
}

void URockInteractorComponent::RefreshHintList()
{
	SCOPE_CYCLE_COUNTER(STAT_RockInteraction_Hints);

	FVector ViewOrigin;
	FVector ViewDirection;
	const AActor* Owner = GetOwner();
	if (!Owner || !GetViewPoint(ViewOrigin, ViewDirection))
	{
		HintPoints.Reset();
		return;
	}

	const float Range = HintRange > 0.f ? HintRange : ScanRange;
	const float RangeSquared = Range * Range;
	const float MinAimDot = FMath::Cos(FMath::DegreesToRadians(HintMaxAimDegrees));
	const FVector PawnLocation = Owner->GetActorLocation();
	const FRockInteractionQuery Query = BuildQuery();

	TArray<FRockInteractionHintPoint> NewHints;
	TArray<FRockInteractionPoint> Points;

	const auto Consider = [&](const FRockInteractionCandidateEntry& Entry, const FRockInteractionPoint& Point)
	{
		const float DistSquared = FVector::DistSquared(PawnLocation, Point.WorldLocation);
		if (DistSquared > RangeSquared) { return; }

		const FVector ToPoint = (Point.WorldLocation - ViewOrigin).GetSafeNormal();
		const float Dot = FVector::DotProduct(ViewDirection, ToPoint);
		if (Dot < MinAimDot) { return; }

		FRockInteractionHintPoint& Hint = NewHints.AddDefaulted_GetRef();
		Hint.Point = Point;
		Hint.OwningActor = Entry.OwningActor.Get();
		Hint.Distance = FMath::Sqrt(DistSquared);
		Hint.AimAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f)));
		Hint.bVisible = !bTraceHintVisibility;
	};

	for (const FRockInteractionCandidateEntry& Entry : Candidates)
	{
		if (!Entry.IsValid()) { continue; }

		Points.Reset();
		{
			SCOPE_CYCLE_COUNTER(STAT_RockInteraction_GatherPoints);
			if (!Entry.Target->GatherInteractionPoints(Query, Points)) { continue; }
		}

		if (Points.IsEmpty())
		{
			// An interactable with no points of its own is marked at the actor, as LookAt scores it.
			if (!::IsValid(Entry.OwningActor)) { continue; }
			FRockInteractionPoint Fallback;
			Fallback.WorldLocation = Entry.OwningActor->GetActorLocation();
			Fallback.SourceComponent = Entry.OwningActor->GetRootComponent();
			Consider(Entry, Fallback);
			continue;
		}

		for (const FRockInteractionPoint& Point : Points)
		{
			// Visibility proxies only widen the look-at area; they are not something to interact with.
			if (Point.Role != ERockInteractionPointRole::Interaction) { continue; }
			Consider(Entry, Point);
		}
	}

	NewHints.Sort([](const FRockInteractionHintPoint& A, const FRockInteractionHintPoint& B) { return A.AimAngleDegrees < B.AimAngleDegrees; });
	if (NewHints.Num() > MaxHints)
	{
		NewHints.SetNum(MaxHints, EAllowShrinking::No);
	}

	// A point that stays listed keeps what its last trace found; a new one stays hidden until it has been traced.
	if (bTraceHintVisibility)
	{
		for (FRockInteractionHintPoint& Hint : NewHints)
		{
			if (const FRockInteractionHintPoint* Old = HintPoints.FindByPredicate([&Hint](const FRockInteractionHintPoint& Existing) { return Existing.IsSamePoint(Hint); }))
			{
				Hint.bVisible = Old->bVisible;
			}
		}
	}

	HintPoints = MoveTemp(NewHints);
	HintTraceCursor = 0;
}

void URockInteractorComponent::UpdateHintFocusFlags()
{
	for (FRockInteractionHintPoint& Hint : HintPoints)
	{
		Hint.bFocused = bHasFocus
			&& Hint.OwningActor.Get() == CurrentContext.Target.GetObject()
			&& Hint.Point.PointTag == CurrentContext.Point.PointTag
			&& Hint.Point.SourceComponent == CurrentContext.Point.SourceComponent
			&& Hint.Point.SocketName == CurrentContext.Point.SocketName;
	}
}

void URockInteractorComponent::TraceHintVisibility()
{
	if (HintPoints.IsEmpty()) { return; }

	FVector ViewOrigin;
	FVector ViewDirection;
	if (!GetViewPoint(ViewOrigin, ViewDirection)) { return; }

	SCOPE_CYCLE_COUNTER(STAT_RockInteraction_HintTrace);
	const int32 Count = HintPoints.Num();
	const int32 Traces = FMath::Min(HintTracesPerPass, Count);
	for (int32 Done = 0; Done < Traces; ++Done)
	{
		HintTraceCursor %= Count;
		FRockInteractionHintPoint& Hint = HintPoints[HintTraceCursor++];
		Hint.bVisible = IsHintPointVisible(ViewOrigin, Hint);
	}
}

bool URockInteractorComponent::IsHintPointVisible(const FVector& ViewOrigin, const FRockInteractionHintPoint& Hint) const
{
	const UWorld* World = GetWorld();
	if (!World) { return true; }

	const FVector ToPoint = Hint.Point.WorldLocation - ViewOrigin;
	const float Length = ToPoint.Size();
	if (Length <= HintVisibilityTolerance) { return true; }

	// The target itself is not ignored: its own body hides a point on the far side. The tolerance keeps the surface the point sits on from doing so.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RockInteractionHintVisibility), false);
	Params.AddIgnoredActor(GetOwner());
	const FVector End = ViewOrigin + ToPoint / Length * (Length - HintVisibilityTolerance);
	return !World->LineTraceTestByChannel(ViewOrigin, End, HintVisibilityChannel, Params);
}

void URockInteractorComponent::DrawInteractionPointDebug(const UWorld* World, const FVector& Location, float LookAtDotProduct) const
{
#if ENABLE_DRAW_DEBUG
	const bool bShowPoints = CVarShowInteractionPoints.GetValueOnGameThread();
	if (!bShowPoints)
	{
		return;
	}
	const float Degrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(LookAtDotProduct, -1.f, 1.f)));
	const float DotAt15Deg = FMath::Cos(FMath::DegreesToRadians(15.f));
	const float T = FMath::Clamp(
		FMath::GetMappedRangeValueClamped(
			FVector2D(DotAt15Deg, 1.f),
			FVector2D(1.f, 0.f),
			LookAtDotProduct), 0.f, 1.f);
	const FColor Color = FLinearColor::LerpUsingHSV(
		FLinearColor(0.f, 1.f, 0.f),
		FLinearColor(1.f, 0.f, 0.f),
		T).ToFColor(true);

	DrawDebugSphere(World, Location, 12.f, 8, Color, false, PrimaryComponentTick.TickInterval, 0, .25f);
	DrawDebugString(World, Location + FVector(0, 0, 20.f), FString::Printf(TEXT("%.2f"), Degrees), nullptr, Color, PrimaryComponentTick.TickInterval, true, 1.2f);
#endif
}

// ----------------------------------------------------------------

bool URockInteractorComponent::GetViewPoint(FVector& OutOrigin, FVector& OutDirection) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}

	const AController* Controller = Pawn->GetController();
	if (!Controller)
	{
		return false;
	}

	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(OutOrigin, ViewRotation);
	OutDirection = ViewRotation.Vector();
	return true;
}

// ----------------------------------------------------------------
// Public API

bool URockInteractorComponent::HasFocus() const
{
	return bHasFocus;
}

int32 URockInteractorComponent::GetOptionCount() const
{
	return CurrentOptions.AvailableOptions.Num();
}

const FRockInteractionContext& URockInteractorComponent::GetFocusedContext() const
{
	return CurrentContext;
}

const FRockInteractionOptions& URockInteractorComponent::GetFocusedOptions() const
{
	return CurrentOptions;
}

void URockInteractorComponent::SetFocusedTarget(const TScriptInterface<IRockInteractableTarget>& NewTarget)
{
	// Unsub from old target
	if (::IsValid(CurrentContext.Target.GetObject()))
	{
		if (FSimpleMulticastDelegate* OldDelegate = CurrentContext.Target->GetInteractionStateChangedDelegate())
		{
			OldDelegate->RemoveAll(this);
		}
	}

	CurrentContext.Target = NewTarget;

	// Sub to new target
	if (NewTarget)
	{
		if (FSimpleMulticastDelegate* NewDelegate = NewTarget->GetInteractionStateChangedDelegate())
		{
			NewDelegate->AddUObject(this, &URockInteractorComponent::OnFocusedTargetStateChanged);
		}
	}
}

void URockInteractorComponent::OnFocusedTargetStateChanged()
{
	if (!bHasFocus)
	{
		return;
	}
	if (!::IsValid(CurrentContext.Target.GetObject()))
	{
		ClearFocus();
		return;
	}

	// Re-gather options, check if they actually changed
	FRockInteractionOptions NewOptions;
	CurrentContext.Target->GatherInteractionOptions(CurrentContext, NewOptions);

	// Nothing left to offer: drop focus rather than keep a prompt with no options.
	if (NewOptions.IsEmpty())
	{
		ClearFocus();
		return;
	}

	if (NewOptions.AvailableOptions != CurrentOptions.AvailableOptions)
	{
		CurrentOptions = MoveTemp(NewOptions);
		OnOptionsChanged.Broadcast(CurrentOptions);
	}
}

void URockInteractorComponent::ClearFocus()
{
	if (!bHasFocus) { return; }

	SetFocusedTarget(nullptr); // handles unsub
	bHasFocus = false;
	CurrentContext = FRockInteractionContext();
	CurrentOptions.Reset();

	OnFocusChanged.Broadcast(CurrentContext);
	OnOptionsChanged.Broadcast(CurrentOptions);
}

FRockInteractionQuery URockInteractorComponent::BuildQuery()
{
	FRockInteractionQuery Query;
	Query.Instigator = GetOwner();
	Query.InteractionTags = QueryInteractionTags;
	// Let child classes append dynamic tags from ASC, wherever
	// e.g. Query.InteractionTags.AppendTags(GetDynamicContextTags());
	return Query;
}

void URockInteractorComponent::TriggerInteraction(int32 OptionIndex)
{
	if (!bHasFocus)
	{
		return;
	}

	if (!::IsValid(CurrentContext.Target.GetObject()))
	{
		ClearFocus();
		return;
	}

	if (!CurrentOptions.AvailableOptions.IsValidIndex(OptionIndex))
	{
		return;
	}

	// Notify target that interaction has begun
	if (CurrentContext.Target)
	{
		CurrentContext.Target->OnInteractionBegin(CurrentContext);
	}

	// Fire delegate for game layer (GA_Interact listens here to activate the ability)
	const FRockInteractionOption& Option = CurrentOptions.AvailableOptions[OptionIndex];
	OnInteractionTriggered.Broadcast(CurrentContext, Option);
}
