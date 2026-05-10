// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RockInteractionTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "RockInteractionContext.generated.h"

class IRockInteractableTarget;

/**
 * Full context for a given interaction query or execution.
 *
 * Passed into GatherInteractionPoints, GatherInteractionOptions, and
 * OnInteractionBegin. The same context is reused across these calls;
 * Point will be empty (default) during GatherInteractionPoints and
 * populated with the winning point for further calls.
 *
 * ContextData is an instanced struct so the game layer can extend it
 * without modifying this header. Use BaseStruct meta to restrict the
 * allowed types in editor if desired:
 */
USTRUCT(BlueprintType)
struct ROCKINTERACTION_API FRockInteractionContext
{
	GENERATED_BODY()
public:
	// Everything from the gather phase
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRockInteractionQuery Query;

	/**
	 * The target being interacted with.
	 * Provides a back-reference so ability code receiving only a context
	 * can reach back to the target without an additional lookup.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "General")
	TScriptInterface<IRockInteractableTarget> Target;

	/**
	 * The specific interaction point that was selected as the winner.
	 * Default (empty) during GatherInteractionPoints.
	 * Only meaningful during GatherInteractionOptions and OnInteractionBegin.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "General")
	FRockInteractionPoint Point;

	// Line trace hit that triggered selection - available for position-sensitive interactions
	UPROPERTY(BlueprintReadWrite)
	FHitResult TraceHitResult;

	/**
	 * Game-specific context data. Extend FRockInteractionContextData to
	 * attach additional state (equipped items, faction flags, etc.).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta = (BaseStruct = "/Script/RockInteraction.RockInteractionContextData"))
	FInstancedStruct ContextData;

	bool IsValid() const;
};
