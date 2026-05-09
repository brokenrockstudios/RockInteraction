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
DECLARE_CYCLE_STAT(TEXT("RockInteraction_SphereScan"), STAT_RockInteraction_SphereScan, STATGROUP_RockInteraction);

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

	if (Candidates.IsEmpty())
	{
		ClearFocus();
		return;
	}

	ScoreAndSelectFocused();
}

void URockInteractorComponent::StartSphereScan()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) { return; }
	UWorld* World = GetWorld();
	if (!ensure(World)) { return; }
	World->GetTimerManager().SetTimer(ScanTimerHandle, this, &ThisClass::TickSphereScan, ScanRate, /* bLoop= */ true);
}

// ----------------------------------------------------------------
// Sphere overlap at ScanRate (Default 20hz)
void URockInteractorComponent::TickSphereScan()
{
	SCOPE_CYCLE_COUNTER(STAT_RockInteraction_SphereScan);

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RockInteractionInstigatorScan), /* bTraceComplex= */ false);
	Params.AddIgnoredActor(Owner);

	// TODO: Move to AsyncOverlap

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByChannel(
		OverlapResults,
		Owner->GetActorLocation(),
		FQuat::Identity,
		ScanChannel,
		FCollisionShape::MakeSphere(ScanRange),
		Params);

#if ENABLE_DRAW_DEBUG
	const bool bShowPoints = CVarShowInteractionPoints.GetValueOnGameThread();
	if (bShowPoints)
	{
		DrawDebugSphere(World, Owner->GetActorLocation(), ScanRange, 12, OverlapResults.Num() > 0 ? FColor::Green : FColor::Cyan, false, ScanRate);
	}
#endif

	UpdateCandidates(OverlapResults);
}

// ----------------------------------------------------------------

void URockInteractorComponent::UpdateCandidates(TArray<FOverlapResult>& Overlaps)
{
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
	for (const auto& Prev : PreviousCandidates)
	{
		if (!NewCandidates.Contains(Prev))
		{
			OnCandidateExited(Prev);
		}
	}
	for (const auto& Next : NewCandidates)
	{
		if (!PreviousCandidates.Contains(Next))
		{
			OnCandidateEntered(Next);
		}
	}

	Swap(PreviousCandidates, Candidates);
	Candidates = MoveTemp(NewCandidates);

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

	FVector ViewOrigin, ViewDirection;
	if (!GetViewPoint(ViewOrigin, ViewDirection))
	{
		ClearFocus();
		return;
	}

	// --- Single line trace for short circuit ---
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	FHitResult HitResult;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RockInteractionInstigatorTrace), false);
	TraceParams.AddIgnoredActor(Owner);

	const float LookAtThresholdCos = FMath::Cos(FMath::DegreesToRadians(LookAtThresholdDegrees));

	// Trace out to max range regardless of candidate distance; we just want to know what the player is looking at, and if it's one of our candidates then that wins outright
	// Because the camera could be 'further back,' we need to make sure the ScanRange is at least far enough to make it past the sphere overlap or something quite far. 
	// If we've done a registered PersistentActor, we might want this to be something like 100m? 
	const FVector TraceEnd = ViewOrigin + ViewDirection * ScanRange * 10;
	World->LineTraceSingleByChannel(HitResult, ViewOrigin, TraceEnd, ScanChannel, TraceParams);
	//DrawDebugLine(World, ViewOrigin, TraceEnd, HitResult.bBlockingHit ? FColor::Red : FColor::Green, false, 0.05f, 0);

	TScriptInterface<IRockInteractableTarget> BestTarget;
	FRockInteractionPoint BestPoint;
	float BestScore = -1.f;

	FRockInteractionQuery Query;
	// TODO: populate query tags from instigator state if needed

	TArray<FRockInteractionPoint> OutPoints;

	for (const TScriptInterface<IRockInteractableTarget>& Candidate : Candidates)
	{
		if (!Candidate)
		{
			continue;
		}

		OutPoints.Reset();
		const bool bInteractable = Candidate->GatherInteractionPoints(Query, OutPoints);
		if (!bInteractable)
		{
			continue;
		}
		const AActor* hitActor = HitResult.GetActor();
		const AActor* CandidateActor = Cast<AActor>(Candidate.GetObject());

		// --- Short circuit: direct hit on actor with 0 or 1 interaction points ---
		if (hitActor == CandidateActor)
		{
			int32 IXPointCount = 0;
			const FRockInteractionPoint* FirstIXPoint = nullptr;
			const FRockInteractionPoint* ComponentMatchPoint = nullptr;
			UPrimitiveComponent* HitComp = HitResult.GetComponent();
			int32 ComponentMatchCount = 0;
			for (const FRockInteractionPoint& Point : OutPoints)
			{
				if (Point.Role != ERockInteractionPointRole::Interaction) { continue; }
				++IXPointCount;
				if (!FirstIXPoint) { FirstIXPoint = &Point; }
				if (HitComp && Point.SourceComponent.Get() == HitComp)
				{
					++ComponentMatchCount;
					ComponentMatchPoint = &Point;
				}
			}
			const bool bUniqueComponentMatch = ComponentMatchCount == 1 && ComponentMatchPoint;
			// Single IX point or component match both resolve here
			const FRockInteractionPoint* Resolved = bUniqueComponentMatch ? ComponentMatchPoint
				: (IXPointCount <= 1) ? FirstIXPoint : nullptr;

			if (Resolved || IXPointCount == 0)
			{
				BestTarget = Candidate;
				BestPoint = Resolved ? *Resolved : FRockInteractionPoint();
				BestScore = 2.f;
				break;
			}
		}

		// --- LookAt scoring ---
		if (OutPoints.IsEmpty())
		{
			// Use actor origin as fallback point
			if (!CandidateActor)
			{
				continue;
			}
			const FVector ToActor = (CandidateActor->GetActorLocation() - ViewOrigin).GetSafeNormal();
			const float Dot = FVector::DotProduct(ViewDirection, ToActor);
			if (Dot > LookAtThresholdCos && Dot > BestScore)
			{
				BestScore = Dot;
				BestTarget = Candidate;
				FRockInteractionPoint ActorPointFallback;
				ActorPointFallback.WorldLocation = CandidateActor->GetActorLocation();
				ActorPointFallback.SourceComponent = CandidateActor->GetRootComponent();
				ActorPointFallback.Role = ERockInteractionPointRole::Interaction;
				BestPoint = ActorPointFallback;
			}
		}
		else
		{
			for (const FRockInteractionPoint& Point : OutPoints)
			{
				const FVector PointLocation = Point.WorldLocation;
				const FVector ToPoint = (PointLocation - ViewOrigin).GetSafeNormal();
				const float Dot = FVector::DotProduct(ViewDirection, ToPoint);
				DrawInteractionPointDebug(World, PointLocation, Dot);
				const float EffectiveThresholdCos = Point.LookAtThresholdScale == 1.f ? LookAtThresholdCos : FMath::Cos(FMath::DegreesToRadians(LookAtThresholdDegrees * Point.LookAtThresholdScale));
				if (Dot > EffectiveThresholdCos && Dot > BestScore)
				{
					BestScore = Dot;
					BestTarget = Candidate;
					if (Point.Role == ERockInteractionPointRole::Interaction)
					{
						BestPoint = Point;
					}
					else if (BestPoint.Role == ERockInteractionPointRole::Visibility)
					{
						// No IX_ point has won yet. Use IV_ as a placeholder that will be resolved back to an IX post-loop
						BestPoint = Point;
					}
				}
			}
		}
	}

	if (!BestTarget)
	{
		ClearFocus();
		return;
	}

	// --- Resolve IV_ proxy to its owning IX_ point ---
	if (BestPoint.Role == ERockInteractionPointRole::Visibility && BestPoint.PointTag.IsValid())
	{
		// OutPoints still contains the last candidate's points. We need the winner's points
		OutPoints.Reset();
		BestTarget->GatherInteractionPoints(Query, OutPoints);

		const FRockInteractionPoint* OwnerIX = OutPoints.FindByPredicate(
			[&](const FRockInteractionPoint& P)
			{
				return P.Role == ERockInteractionPointRole::Interaction && P.PointTag == BestPoint.PointTag;
			});
		if (OwnerIX)
		{
			BestPoint = *OwnerIX;
		}
	}

	// --- Dirty check: did focus target change? ---
	const bool bTargetChanged = (CurrentContext.Target != BestTarget);

	CurrentContext.Point = BestPoint;
	CurrentContext.Query = Query;
	if (HitResult.GetActor() == Cast<AActor>(BestTarget.GetObject()))
	{
		CurrentContext.TraceHitResult = HitResult;
	}

	bHasFocus = true;

#if ENABLE_DRAW_DEBUG
	const bool bShowPoints = CVarShowInteractionPoints.GetValueOnGameThread();
	if (!bShowPoints)
	{
		DrawDebugSphere(World, BestPoint.WorldLocation, 12.f, 8, FColor::White, false, 0.05f, 1, .25);
	}
#endif

	if (bTargetChanged)
	{
		SetFocusedTarget(BestTarget); // handles unsub/sub of state change delegate
		// Regather options for new target
		CurrentOptions.Reset();
		BestTarget->GatherInteractionOptions(CurrentContext, CurrentOptions);

		OnFocusChanged.Broadcast(CurrentContext);
		OnOptionsChanged.Broadcast(CurrentOptions);
	}
	else
	{
		// Same target - check if options changed via state delegate
		// (options are refreshed when the target broadcasts GetInteractionStateChangedDelegate)
		// No re-gather here by default; targets push changes via their delegate
	}
}

void URockInteractorComponent::DrawInteractionPointDebug(const UWorld* World, const FVector& Location, float LookAtDotProduct)
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
