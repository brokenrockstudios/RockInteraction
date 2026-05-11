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

// Sets default values for this component's properties
URockInteractorComponent::URockInteractorComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

// ----------------------------------------------------------------
// Lifecycle

void URockInteractorComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;
	const bool bShouldRun = Pawn->IsLocallyControlled() || Pawn->HasAuthority();
	if (bShouldRun)
	{
		StartSphereScan();
	}
	else
	{
		// Sometimes our pawn gets spawned before it has a controller, so let's just wait until we have one.
		Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &ThisClass::OnControllerChanged);
	}
}

void URockInteractorComponent::OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	const bool bShouldRun = Pawn->IsLocallyControlled() || Pawn->HasAuthority();
	if (NewController && bShouldRun)
	{
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::OnControllerChanged);
		StartSphereScan();
	}
}

void URockInteractorComponent::AddPersistentCandidate(const TScriptInterface<IRockInteractableTarget>& Target)
{
	if (Target)
	{
		PersistentCandidates.AddUnique(Target);
	}
}

void URockInteractorComponent::RemovePersistentCandidate(const TScriptInterface<IRockInteractableTarget>& Target)
{
	PersistentCandidates.RemoveSwap(Target);
}

void URockInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SphereScanDelegate.Unbind();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------
// LookAt scoring runs every frame, 1x line trace + cheap dot products only
void URockInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ScoreAndSelectFocused();

#if ENABLE_DRAW_DEBUG
	const bool bShowPoints = CVarShowInteractionPoints.GetValueOnGameThread();
	if (bShowPoints)
	{
		AActor* Owner = GetOwner();
		UWorld* World = GetWorld();
		DrawDebugSphere(World, Owner->GetActorLocation(), ScanRange, 12, Candidates.Num() > 0 ? FColor::Green : FColor::Cyan, false, 0);
		if (bHasFocus)
		{
			DrawDebugSphere(World, CurrentContext.Point.WorldLocation, 12.f, 8, FColor::White, false, 0, 1, .25);
		}
	}
#endif
}

void URockInteractorComponent::StartSphereScan()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) { return; }
	UWorld* World = GetWorld();
	if (!ensure(World)) { return; }
	SphereScanDelegate.BindUObject(this, &URockInteractorComponent::OnScanComplete);
	World->GetTimerManager().SetTimer(ScanTimerHandle, this, &ThisClass::TickSphereScan, ScanRate, /* bLoop= */ true);
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


// ----------------------------------------------------------------

void URockInteractorComponent::UpdateCandidates(TArray<FOverlapResult>& Overlaps)
{
	SCOPE_CYCLE_COUNTER(STAT_RockInteraction_UpdateCandidates);

	TArray<TScriptInterface<IRockInteractableTarget>> NewCandidates;

	// Inject persistent candidates. Always relevant regardless of overlap
	for (const auto& Persistent : PersistentCandidates)
	{
		NewCandidates.AddUnique(Persistent);
	}

	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Actor = Result.GetActor();
		if (!Actor) { continue; }

		// Check the actor itself first, then its components
		if (Actor->Implements<URockInteractableTarget>())
		{
			TScriptInterface<IRockInteractableTarget> Target;
			Target.SetObject(Actor);
			Target.SetInterface(Cast<IRockInteractableTarget>(Actor));
			NewCandidates.AddUnique(Target);
		}
	}

	// Diff: exits first, then enters
	for (const auto& Prev : Candidates)
	{
		if (!NewCandidates.Contains(Prev))
		{
			OnCandidateExited(Prev);
		}
	}
	for (const auto& Next : NewCandidates)
	{
		if (!Candidates.Contains(Next))
		{
			OnCandidateEntered(Next);
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

void URockInteractorComponent::OnCandidatesUpdated(const TArray<TScriptInterface<IRockInteractableTarget>>& NewCandidates)
{
	// Do game specific things, such as adding GAS abilities based upon candidates.
}


// ----------------------------------------------------------------
// Per-frame scoring

void URockInteractorComponent::ScoreAndSelectFocused()
{
	SCOPE_CYCLE_COUNTER(STAT_RockInteraction_ScoreAndSelect);

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
	FRockInteractionQuery Query;
	Query.Instigator = Owner;
	// TODO: populate query tags from instigator state if needed

	// --- Score ---
	TScriptInterface<IRockInteractableTarget> BestTarget;
	FRockInteractionPoint BestPoint;

	// Try Direct Hit, otherwise fallback to LookAt
	if (!TryResolveDirectHit(ScanCtx, Query, BestTarget, BestPoint))
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

	CurrentContext.Point = BestPoint;
	CurrentContext.Query = Query;
	CurrentContext.TraceHitResult = ScanCtx.HitResult;

	bHasFocus = true;

	if (bTargetChanged)
	{
		SetFocusedTarget(BestTarget);
		CurrentOptions.Reset();
		BestTarget->GatherInteractionOptions(CurrentContext, CurrentOptions);
		OnFocusChanged.Broadcast(CurrentContext);
		OnOptionsChanged.Broadcast(CurrentOptions);
	}
}


bool URockInteractorComponent::TryResolveDirectHit(
	const FInteractionScanContext& ScanCtx,
	const FRockInteractionQuery& Query,
	TScriptInterface<IRockInteractableTarget>& OutTarget,
	FRockInteractionPoint& OutPoint) const
{
	if (!ScanCtx.HitActor) { return false; }
	if (!ScanCtx.HitActor->Implements<URockInteractableTarget>()) { return false; }

	for (const auto& Candidate : Candidates)
	{
		if (!Candidate) { continue; }
		const AActor* CandidateActor = Cast<AActor>(Candidate.GetObject());
		if (CandidateActor != ScanCtx.HitActor) { continue; }

		TArray<FRockInteractionPoint> Points;
		{
			SCOPE_CYCLE_COUNTER(STAT_RockInteraction_GatherPoints);
			if (!Candidate->GatherInteractionPoints(Query, Points))
			{
				return false; // Actor is hit but currently non-interactable; no fallback to LookAt
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

		// Ambiguous: fall through to LookAt, but we already found the candidate so stop searching
		return false;
	}

	// HitActor not in candidates
	return false;
}

bool URockInteractorComponent::ScoreCandidatesByLookAt(
	const FInteractionScanContext& ScanCtx,
	const FRockInteractionQuery& Query,
	TScriptInterface<IRockInteractableTarget>& OutTarget,
	FRockInteractionPoint& OutPoint) const
{
	float BestScore = -FLT_MAX;
	TArray<FRockInteractionPoint> Points;

	for (const auto& Candidate : Candidates)
	{
		if (!Candidate) { continue; }
		if (Candidate->RequiresDirectHit()) { continue; }
		Points.Reset();
		{
			SCOPE_CYCLE_COUNTER(STAT_RockInteraction_GatherPoints);
			if (!Candidate->GatherInteractionPoints(Query, Points)) { continue; }
		}

		const AActor* CandidateActor = Cast<AActor>(Candidate.GetObject());

		if (Points.IsEmpty())
		{
			if (!CandidateActor) { continue; }
			const FVector ToActor = (CandidateActor->GetActorLocation() - ScanCtx.ViewOrigin).GetSafeNormal();
			const float Dot = FVector::DotProduct(ScanCtx.ViewDirection, ToActor);
			if (Dot > ScanCtx.LookAtThresholdCos && Dot > BestScore)
			{
				BestScore = Dot;
				OutTarget = Candidate;
				FRockInteractionPoint Fallback;
				Fallback.WorldLocation = CandidateActor->GetActorLocation();
				Fallback.SourceComponent = CandidateActor->GetRootComponent();
				Fallback.Role = ERockInteractionPointRole::Interaction;
				OutPoint = Fallback;
			}
		}
		else
		{
			for (const FRockInteractionPoint& Point : Points)
			{
				const FVector ToPoint = (Point.WorldLocation - ScanCtx.ViewOrigin).GetSafeNormal();
				const float Dot = FVector::DotProduct(ScanCtx.ViewDirection, ToPoint);
				DrawInteractionPointDebug(GetWorld(), Point.WorldLocation, Dot);
				const float EffectiveThresholdCos = Point.LookAtThresholdScale == 1.f
					? ScanCtx.LookAtThresholdCos
					: FMath::Cos(FMath::DegreesToRadians(LookAtThresholdDegrees * Point.LookAtThresholdScale));
				if (Dot > EffectiveThresholdCos && Dot > BestScore)
				{
					BestScore = Dot;
					OutTarget = Candidate;
					OutPoint = Point;
				}
			}
		}
	}

	return OutTarget.GetObject() != nullptr;
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

	DrawDebugSphere(World, Location, 12.f, 8, Color, false, 0.05f, 0, .25);
	DrawDebugString(World, Location + FVector(0, 0, 20.f), FString::Printf(TEXT("%.2f"), Degrees), nullptr, Color, 0.05f, true, 1.2);
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
	if (CurrentContext.Target)
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
	if (!bHasFocus || !CurrentContext.Target)
	{
		return;
	}

	// Re-gather options, check if they actually changed
	FRockInteractionOptions NewOptions;
	CurrentContext.Target->GatherInteractionOptions(CurrentContext, NewOptions);

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
}

void URockInteractorComponent::TriggerInteraction(int32 OptionIndex)
{
	if (!bHasFocus)
	{
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
