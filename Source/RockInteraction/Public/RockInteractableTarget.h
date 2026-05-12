// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RockInteractionTypes.h"
#include "UObject/Interface.h"

#include "RockInteractableTarget.generated.h"

class UGameplayAbility;
class URockInteractionOptionBuilder;
struct FRockInteractionContext;
struct FRockInteractionOptions;


// ----------------------------------------------------------------

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URockInteractableTarget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement on any actor that can be interacted with.
 *
 * Selection flow (driven externally by the interaction subsystem):
 *
 *   20hz sphere sweep
 *    * builds/maintains candidate actor list
 *
 *   Per-frame (or every 2nd frame) on candidates:
 *    * GatherInteractionPoints() → fresh FRockInteractionPoint array
 *
 *   Per-frame line trace + LookAt scoring:
 *     * 0 or 1 points + direct hit  → immediate winner
 *     * 2+ points, or no direct hit → best LookAt percentage per point wins
 *
 *   On winner selected:
 *     * GatherInteractionOptions() → ability tags, costs, UI data, etc.
 *
 *   On interaction executed:
 *     * OnInteractionBegin()
 *     * or leverage GameplayAbility or alternative channels
 */
class ROCKINTERACTION_API IRockInteractableTarget
{
	GENERATED_BODY()
public:
	/**
	 * Populate OutPoints with this actor's current interaction points.
	 *
	 * HOT PATH: KEEP IT PERFORMANT
	 * Called frequently (every frame or every other frame) while this actor is
	 * in the player's proximity candidate list. Keep this cheap! Resolve socket
	 * transforms via iteration rather than per-name lookup where possible.
	 *
	 * Return false if this actor is currently non-interactable (disabled, locked,
	 * out of resources, etc.). Returning false removes it from winner consideration
	 * without removing it from the candidate list.
	 *
	 * Point count drives selection behavior:
	 *   0 - no specific points; box-center used for LookAt fallback
	 *   1 - single point; direct line trace hit on this actor wins immediately
	 *   2+ - multi-point; direct hit ignored, per-point LookAt scoring applies
	 *
	 * @param Context		Interaction context (instigator, query tags, etc.)
	 * @param OutPoints		Array to populate; will be Reset() before this call
	 * @return				True if currently interactable, false if not
	 */
	virtual bool GatherInteractionPoints(const FRockInteractionQuery& Context, TArray<FRockInteractionPoint>& OutPoints) const = 0;

	/**
	 * Populate interaction options for a specific winning point.
	 *
	 * Only called on the actor/point that won candidate selection.
	 * Use this to push ability activation tags, costs, widget data, etc.
	 * into the option builder.
	 *
	 * @param Context		Interaction context
	 * @param InteractionOptions	Builder to populate with available interaction options
	 */
	virtual void GatherInteractionOptions(const FRockInteractionContext& Context, FRockInteractionOptions& InteractionOptions) = 0;

	// // On the target actor. Purely static, no context needed
	// void AMyActor::GatherInteractionAbilities(TArray<TSubclassOf<UGameplayAbility>>& OutAbilities) const{ 
	//	OutAbilities.Add(UGA_PullLever::StaticClass());
	//	OutAbilities.Add(UGA_InspectObject::StaticClass());
	// Due to potential race conditions of replicating an ability, we need to preemptively grant the instigator the abilities 	
	// NOTE: MUST be deterministic thru the lifetime of the actor. Do not 'change' what shows up in the list.
	// If you need more conditional state, consider something like 'enter minigame' mode that then grants you appropriate functionality
	// This is only meant for first 'interaction' type abilities only.
	virtual void GatherInteractionAbilities(TArray<TSubclassOf<UGameplayAbility>>& OutAbilities) const = 0;
	
	/**
	 * Whether this actor requires a direct line-trace hit to become focused.
	 *
	 * When true, LookAt scoring is skipped entirely — this actor can only be
	 * selected if the player's crosshair trace hits it directly. Useful for
	 * large interactables (doors, panels) where LookAt would win trivially
	 * anyway, or where you want precise per-component IX point disambiguation.
	 *
	 * Multiple IX points are still supported provided each is bound to a
	 * distinct source component; component-match resolution handles them cleanly.
	 *
	 * Returning true is also a minor per-frame performance optimization, as LookAt
	 * scoring (including GatherInteractionPoints) is skipped for this actor entirely.
	 *
	 * Default is false. Standard LookAt scoring applies.
	 */
	virtual bool RequiresDirectHit() const { return false; }

	/**
	 * Called when the player commits to an interaction on this actor.
	 * Override to apply immediate state changes, trigger animations, etc.
	 * Default is a no-op; not all interactables need to react here
	 * (GAS abilities triggered via GatherInteractionOptions may handle it instead).
	 *
	 * @param Context	Interaction context
	 */
	virtual void OnInteractionBegin(const FRockInteractionContext& Context)
	{
	}

	// Optional: implement if this actor's interaction state can change while in proximity
	// Broadcast whenever options would be different (locked, opened, out of ammo, etc.)
	virtual FSimpleMulticastDelegate* GetInteractionStateChangedDelegate() { return nullptr; }

	/** Display name shown in the interaction UI. Override to provide a localized string. */
	UFUNCTION(BlueprintCallable)
	virtual FText GetInteractableDisplayName() const
	{
		return NSLOCTEXT("RockInteractableTarget", "DisplayName", "InteractableActor");
	}
};
