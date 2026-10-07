// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "RockInteractionTypes.generated.h"

class IRockInteractableTarget;

ROCKINTERACTION_API DECLARE_LOG_CATEGORY_EXTERN(LogRockInteraction, Display, All);

UENUM(BlueprintType)
enum class ERockInteractionPointRole : uint8
{
	Interaction, // IX_ : selectable, shown in UI, committed on interact
	Visibility, // IX_VP_ : Visibility Proxy. Never becomes BestPoint
};

// Sphere + LookAt is slightly more expensive but requires 1 IX per mesh or dedicated component
UENUM()
enum class ERockInteractorScanMode : uint8
{
	DirectHitWithSphereOverlap, // current behavior, line trace first, then fallback to sphere + LookAt if no direct hit
	DirectHitOnly, // skip sphere entirely, pure line trace
};

USTRUCT()
struct ROCKINTERACTION_API FRockInteractionCandidateEntry
{
	GENERATED_BODY()

	// Both are UPROPERTYs so GC sees them and nulls them once the object is gone; IsValid() also catches a target
	// that was destroyed but not yet collected.
	UPROPERTY()
	TScriptInterface<IRockInteractableTarget> Target;

	UPROPERTY()
	TObjectPtr<AActor> OwningActor = nullptr;

	bool operator==(const FRockInteractionCandidateEntry& Other) const
	{
		return Target.GetObject() == Other.Target.GetObject();
	}

	/** True while the target object is alive (not null, not destroyed, not garbage). */
	bool IsValid() const
	{
		return ::IsValid(Target.GetObject());
	}
};

/**
 * A single point of interest on an interactable actor.
 *
 * Point count semantics (see IRockInteractableTarget::GatherInteractionPoints):
 *   0 points  - Actor is interactable but has no specific points; box-center used for LookAt scoring.
 *   1 point   - Single point; eligible to win via direct line trace hit.
 *   2+ points - Multi-point; direct hit is ignored, winner resolved by per-point LookAt percentage.
 */
USTRUCT(BlueprintType)
struct ROCKINTERACTION_API FRockInteractionPoint
{
	GENERATED_BODY()

	/**
	 * World-space transform of this point, populated fresh each GatherInteractionPoints call.
	 * Treat as a near-current snapshot - do not cache across frames.
	 */
	UPROPERTY(BlueprintReadWrite)
	FVector WorldLocation = FVector::ZeroVector;

	/** Stable identity used for GAS events, UI, and option mapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag PointTag;

	/**
	 * Source component this point was derived from (socket on a mesh, scene component origin, etc.).
	 * Valid at execution time for cases where sub-frame animation accuracy matters (e.g. lever tip on bone).
	 * Not used during candidate selection.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TWeakObjectPtr<USceneComponent> SourceComponent = nullptr;

	/**
	 * Socket name on SourceComponent. NAME_None means use the component's own origin.
	 * Only meaningful if SourceComponent is valid.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FName SocketName = NAME_None;

	/** Multiplier applied to the instigator's LookAtThresholdDegrees for this point.
	 *  1.0 = default threshold. 1.5 = 50% wider cone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float LookAtThresholdScale = 1.f;

	/** Interaction points are selectable and committed on interact.
	*  Visibility points act as a look-at scoring proxies and future visibility probes. They widen
	*  the effective target area but are never surfaced as the active interaction point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	ERockInteractionPointRole Role = ERockInteractionPointRole::Interaction;
};

static_assert(sizeof(FRockInteractionPoint) == 56 || sizeof(FRockInteractionPoint) == 64, "Check layout");

/**
 * One entry of the interactor's hint list: an Interaction-role point near the local player that a game can mark with a dot.
 * Read through URockInteractorComponent::GetHintPoints(); the plugin draws nothing.
 */
USTRUCT(BlueprintType)
struct ROCKINTERACTION_API FRockInteractionHintPoint
{
	GENERATED_BODY()

	/** The point as gathered at the last hint refresh. PointTag identifies the verb, so a game can pick a colour or icon from it. */
	UPROPERTY(BlueprintReadOnly)
	FRockInteractionPoint Point;

	/** The actor that owns the point's target. */
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> OwningActor;

	/** Distance from the interacting pawn to the point. */
	UPROPERTY(BlueprintReadOnly)
	float Distance = 0.f;

	/** Angle between the view direction and the direction from the view to the point. The list is ordered by it. */
	UPROPERTY(BlueprintReadOnly)
	float AimAngleDegrees = 0.f;

	/** True for the point that currently has interaction focus. Updated every scoring pass, not only at refresh. */
	UPROPERTY(BlueprintReadOnly)
	bool bFocused = false;

	/** False while a visibility trace from the view to the point is blocked, and until the first trace has run. Always true when tracing is off. */
	UPROPERTY(BlueprintReadOnly)
	bool bVisible = false;

	/** Same target actor, point tag, source component and socket. A moved point is still the same point. */
	bool IsSamePoint(const FRockInteractionHintPoint& Other) const
	{
		return OwningActor == Other.OwningActor
			&& Point.PointTag == Other.Point.PointTag
			&& Point.SourceComponent == Other.Point.SourceComponent
			&& Point.SocketName == Other.Point.SocketName;
	}
};

// ----------------------------------------------------------------

USTRUCT(BlueprintType)
struct ROCKINTERACTION_API FRockInteractionQuery
{
	GENERATED_BODY()

	/** The actor performing the interaction. Typically, a Pawn, but kept as AActor
	 *  to allow non-pawn instigators (automation systems, vehicles in some setups). */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite)
	TWeakObjectPtr<AActor> Instigator;

	// Optional tags to gate/filter options on the target side
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer InteractionTags;

	// Game-specific query extensions (equipped item checks, faction state, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInstancedStruct QueryData;

	APawn* GetInstigatorPawn() const;
};


/**
 * Base data payload for an interaction context.
 *
 * Inherit from this struct to attach game-specific state to an interaction
 * without modifying the core context. For example:
 *
 *   USTRUCT()
 *   struct FFenInteractionContextData : public FRockInteractionContextData
 *   {
 *       GENERATED_BODY()
 *       UPROPERTY() bool bPlayerHasKeycard = false;
 *   };
 *
 * The interaction subsystem passes this through opaquely - targets and
 * instigators can cast to their expected derived type as needed.
 */
USTRUCT(BlueprintType)
struct ROCKINTERACTION_API FRockInteractionContextData
{
	GENERATED_BODY()

	/**
	 * The instigator performing this interaction query.
	 * Typically the local player's controller or pawn.
	 */
	//UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "General")
	//TScriptInterface<IRockInteractableInstigator> Instigator;

	/**
	 * Optional tags providing additional context for this interaction.
	 * Can be used by targets to gate options (e.g. require a specific tag
	 * to unlock an interaction, or filter options by interaction type).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	FGameplayTagContainer InteractionTags;
};

// Helper context internal usage for InteractorComponent
struct FInteractionScanContext
{
	FVector ViewOrigin = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	AActor* HitActor = nullptr;
	UPrimitiveComponent* HitComp = nullptr;
	float LookAtThresholdCos = 0.f;
	FHitResult HitResult;
};
