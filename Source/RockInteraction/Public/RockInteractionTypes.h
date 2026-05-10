// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "RockInteractionTypes.generated.h"

ROCKINTERACTION_API DECLARE_LOG_CATEGORY_EXTERN(LogRockInteraction, Display, All);

UENUM(BlueprintType)
enum class ERockInteractionPointRole : uint8
{
	Interaction, // IX_ : selectable, shown in UI, committed on interact
	Visibility, // IX_VP_ : Visibility Proxy. Never becomes BestPoint
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

// ----------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRockInteractionQuery
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
